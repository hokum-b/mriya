#define _POSIX_C_SOURCE 200809L
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <X11/XF86keysym.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>

#include "parser.h"

typedef enum { STATE_NORMAL, STATE_FLOATING, STATE_MAXIMIZED, STATE_FULLSCREEN } ClientState;

#define MAX_CLIENTS 256
#define MAX_WORKSPACES 10
#define TITLE_HEIGHT 20
#define BUTTON_SIZE 12
#define BUTTON_MARGIN 2

typedef struct Client {
    Window window;
    Window frame;
    int x, y;
    unsigned int width, height;
    ClientState state;
    ClientState prev_state;
    int workspace;
    struct Monitor *monitor;
    int mapped;
    int urgent;
    int ispanel;
    int is_transient;
    struct Client *next;
    struct Client *prev;
    struct Client *next_stack;
    struct Client *prev_stack;
    int border_width;
    int minw, minh, maxw, maxh;
    int applied_valid;
    int applied_x, applied_y;
    unsigned int applied_w, applied_h;
    int applied_title_h, applied_bw;
    int ignore_unmap;
    int published_ws;
    int custom_width;
    int managed;
    int want_fullscreen;
    int want_maximized;
    int fs_x, fs_y;
    unsigned int fs_width, fs_height;
    int fs_border_width;
    int max_x, max_y;
    unsigned int max_width, max_height;
    int float_x, float_y;
    unsigned int float_width, float_height;
    char title[256];
} Client;

typedef struct Monitor {
    int x, y;
    unsigned int width, height;
    int wx, wy;
    unsigned int ww, wh;
    Client *clients;
    Client *clients_tail;
    Client *stack;
    Client *sel;
    int scroll_x;
    int workspace;
    struct Monitor *next;
    struct Monitor *prev;
    Client *lastsel[MAX_WORKSPACES];
} Monitor;

typedef struct {
    unsigned int mod;
    KeySym keysym;
    void (*func)(const char *arg);
    const char *arg;
} Key;

typedef struct {
    unsigned int mod;
    unsigned int button;
    void (*func)(const char *arg);
    const char *arg;
} Button;

void spawn(const char *arg);
void killclient(const char *arg);
void quit(const char *arg);
void focusleft(const char *arg);
void focusright(const char *arg);
void ws_up(const char *arg);
void ws_down(const char *arg);
void setgaps(const char *arg);
void zoom(const char *arg);
void togglefloating(const char *arg);
void togglemaximize(const char *arg);
void togglefullscreen(const char *arg);
void view(const char *arg);
void toggleview(const char *arg);
void tag(const char *arg);
void toggletag(const char *arg);
void movemouse(const char *arg);
void resizemouse(const char *arg);

char norm_bg[64] = "#222222";
char norm_outer_border[64] = "#ede5d4";
char norm_inner_border[64] = "#111111";
char sel_bg[64] = "#111111";
char sel_outer_border[64] = "#ede5d4";
char sel_inner_border[64] = "#ffffff";
char urgent_color[64] = "#ede5d4";
char title_active_bg[64] = "#111111";
char title_active_fg[64] = "#ffffff";
char title_inactive_bg[64] = "#222222";
char title_inactive_fg[64] = "#ede5d4";
char title_font[64] = "fixed";
int snap_val = 32;
int outer_border_width = 0;
int inner_border_width = 0;
unsigned int default_modkey = Mod4Mask;

Key keys[256];
int nkeys = 0;
char *autostart_cmds[256];
int nautostart = 0;
Button buttons[256];
int nbuttons = 0;

#define LENGTH(X) (sizeof X / sizeof X[0])
#define BUTTONMASK (ButtonPressMask|ButtonReleaseMask)
#define MOUSEMASK (BUTTONMASK|PointerMotionMask)
#define CLEANMASK(mask) ((mask) & ~(numlockmask|LockMask) & (ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask))
#define ISVISIBLE(C) (C->workspace == selmon->workspace)

static Display *dpy;
static int screen;
static Window root;
static int sw, sh;
static Monitor *mons = NULL;
static Monitor *selmon = NULL;
static int running = 1;
static int shutting_down = 0;
static volatile sig_atomic_t need_rekey = 0;
static int rekey_pipe[2] = { -1, -1 };
static Cursor cursor_normal;
static Cursor cursor_move;
static Cursor cursor_resize;
int inner_gaps = 30;
int outer_gaps = 50;
int border_width = 0;
int show_titlebar = 1;
int show_title = 1;
int show_buttons = 1;
int insert_end = 1;
int strip_align = 0;
int theme_mode = 0;
static unsigned int numlockmask = 0;
static KeyCode keycodes[256];

static unsigned long col_norm_outer_border;
static unsigned long col_norm_inner_border;
static unsigned long col_sel_outer_border;
static unsigned long col_sel_inner_border;
static unsigned long col_title_active_bg;
static unsigned long col_title_active_fg;
static unsigned long col_title_inactive_bg;
static unsigned long col_title_inactive_fg;
static GC gc;
static XFontStruct *font;

enum { WMProtocols, WMDelete, WMState, WMTakeFocus };
static Atom wmatom[4];

enum {
    NetWMWindowType, NetWMWindowTypeDock, NetWMWindowTypeDialog,
    NetWMWindowTypeToolbar, NetWMWindowTypeMenu, NetWMWindowTypeSplash,
    NetWMWindowTypeUtility, NetWMWindowTypeNotification,
    NetWMState, NetWMStateFullScreen, NetWMStateMaximizedVert, NetWMStateMaximizedHorz,
    NetWMStateHidden, NetWMStateAbove, NetWMStateDemandsAttention,
    NetWMStrutPartial, NetWMStrut,
    NetActiveWindow, NetCloseWindow,
    NetNumberOfDesktops, NetDesktopNames, NetCurrentDesktop, NetWMDesktop,
    NetClientList, NetClientListStacking, NetWMName, NetUTF8String,
    NetSupported, NetWorkarea, NetDesktopViewport,
    NetLast
};
static Atom netatom[NetLast];
static Atom net_supporting_wm_check;
static Window wmcheckwin;

static int (*xerrorxlib)(Display *, XErrorEvent *);

static void autostart(void);
static void checkotherwm(void);
static void cleanup(void);
static void clientmessage(XEvent *e);
static void closeclient(Client *c);
static void configurenotify(XEvent *e);
static void configurerequest(XEvent *e);
static Monitor *createmon(int x, int y, int w, int h);
static void destroynotify(XEvent *e);
static void die(const char *fmt, ...);
static void drawtitle(Client *c);
static void reframe(Client *c);
static void updateframes(void);
static void enternotify(XEvent *e);
static void expose(XEvent *e);
static void focus(Client *c);
static void unfocus(Client *c, int setfocus);
static void focusin(XEvent *e);
static void ensure_visible(Client *c);

static int getrootptr(int *x, int *y);
static long getstate(Window w);
static int gettextprop(Window w, Atom atom, char *text, unsigned int size);
static int getatomprops(Client *c, Atom prop, Atom *out, int max);
static void grabbuttons(Client *c, int focused);
static void grabkeys(void);
static void initatoms(void);
static void initcolors(void);
static void buttonpress(XEvent *e);
static void keypress(XEvent *e);
void killclient(const char *arg);
static void manage(Window w, XWindowAttributes *wa);
static void mappingnotify(XEvent *e);
static void maprequest(XEvent *e);
void movemouse(const char *arg);
static void propertynotify(XEvent *e);
void quit(const char *arg);
static void resize(Client *c, int x, int y, int w, int h, int interact);
static void configure(Client *c);
void resizemouse(const char *arg);
static void restack(Monitor *m);
static void run(void);
static void scan(void);
static int ignorewindow(Window w);
static int sendevent(Client *c, Atom proto);
static void setclientstate(Client *c, long state);
static void setfocus(Client *c);
static void setfullscreen(Client *c, int fullscreen);
static void setmaximized(Client *c, int maximized);
static void setnetwmstate(Client *c, Atom atom, int on);
static void setup(void);
static void showhide(Client *c);
static void sigchld(int unused);
void spawn(const char *arg);
void tag(const char *arg);
void togglefloating(const char *arg);
void togglemaximize(const char *arg);
void togglefullscreen(const char *arg);
void toggletag(const char *arg);
void toggleview(const char *arg);
static void unmanage(Client *c, int destroyed);
static void unmapnotify(XEvent *e);
static void updateclientlist(void);
static int updategeom(void);
static void updatenumlockmask(void);
static void updatesizehints(Client *c);
static void updatetitle(Client *c);
static void updatewindowtype(Client *c);
static void updatewmhints(Client *c);
static void updateworkspaces(void);
static int workspace_has_clients(int ws);
static int desktop_to_workspace(int desktop);
void view(const char *arg);
static Client *wintoclient(Window w);
static Client *frametoclient(Window w);
static int xerror(Display *d, XErrorEvent *e);
static int xerrordummy(Display *d, XErrorEvent *e);
static int xerrorstart(Display *d, XErrorEvent *e);
void zoom(const char *arg);
void setgaps(const char *arg);
void ws_up(const char *arg);
void ws_down(const char *arg);
static void arrange(Monitor *m);
static int get_total_strip_width(Monitor *m);
static void attach(Client *c);
static void attachstack(Client *c);
static void detach(Client *c);
static void map_client(Client *c);
static void unmap_client(Client *c);
static void updateworkarea(void);

static void (*handler[LASTEvent])(XEvent *) = {
    [ClientMessage] = clientmessage,
    [ConfigureNotify] = configurenotify,
    [ConfigureRequest] = configurerequest,
    [DestroyNotify] = destroynotify,
    [EnterNotify] = enternotify,
    [Expose] = expose,
    [FocusIn] = focusin,
    [KeyPress] = keypress,
    [MappingNotify] = mappingnotify,
    [MapRequest] = maprequest,
    [PropertyNotify] = propertynotify,
    [ButtonPress] = buttonpress,
    [UnmapNotify] = unmapnotify
};

static void die(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    if (fmt[0] && fmt[strlen(fmt)-1] == ':') {
        fputc(' ', stderr);
        perror(NULL);
    } else {
        fputc('\n', stderr);
    }
    exit(1);
}

static int gettextprop(Window w, Atom atom, char *text, unsigned int size) {
    char **list = NULL;
    int n;
    XTextProperty name;
    if (!text || size == 0) return 0;
    text[0] = '\0';
    if (!XGetTextProperty(dpy, w, &name, atom) || !name.nitems) return 0;
    if (name.encoding == XA_STRING)
        strncpy(text, (char *)name.value, size - 1);
    else {
        if (XmbTextPropertyToTextList(dpy, &name, &list, &n) >= Success && n > 0 && *list) {
            strncpy(text, *list, size - 1);
            XFreeStringList(list);
        }
    }
    text[size - 1] = '\0';
    XFree(name.value);
    return 1;
}

static void sigchld(int unused) {
    int saved_errno = errno;
    (void)unused;
    while (waitpid(-1, NULL, WNOHANG) > 0);
    errno = saved_errno;
}

static void sigrekey(int sig) {
    int saved_errno = errno;
    char c = 1;
    (void)sig;
    if (rekey_pipe[1] >= 0)
        write(rekey_pipe[1], &c, 1);
    need_rekey = 1;
    errno = saved_errno;
}

static void autostart(void) {
    int i;
    for (i = 0; i < nautostart; i++)
        if (autostart_cmds[i])
            spawn(autostart_cmds[i]);
}

static Monitor *createmon(int x, int y, int w, int h) {
    Monitor *m = calloc(1, sizeof(Monitor));
    if (!m) die("calloc:");
    m->x = x;
    m->y = y;
    m->width = w;
    m->height = h;
    m->wx = x;
    m->wy = y;
    m->ww = w;
    m->wh = h;
    m->scroll_x = 0;
    m->workspace = 0;
    m->clients = NULL;
    m->clients_tail = NULL;
    m->stack = NULL;
    m->sel = NULL;
    m->next = NULL;
    m->prev = NULL;
    memset(m->lastsel, 0, sizeof(m->lastsel));
    return m;
}

static int updategeom(void) {
    int dirty = 0;
    if (!mons) {
        mons = createmon(0, 0, sw, sh);
        selmon = mons;
        dirty = 1;
    } else if (mons->width != (unsigned int)sw || mons->height != (unsigned int)sh) {
        mons->width = sw;
        mons->height = sh;
        mons->ww = sw;
        mons->wh = sh;
        dirty = 1;
    }
    if (dirty) updateworkarea();
    return dirty;
}

static void updateworkarea(void) {
    Monitor *m;
    Client *c;
    for (m = mons; m; m = m->next) {
        long left = 0, right = 0, top = 0, bottom = 0;
        for (c = m->clients; c; c = c->next) {
            int di, have = 0;
            unsigned long n, dl;
            unsigned char *p = NULL;
            Atom da;
            if (!c->ispanel) continue;
            if (XGetWindowProperty(dpy, c->window, netatom[NetWMStrutPartial], 0L, 12L,
                False, XA_CARDINAL, &da, &di, &n, &dl, &p) == Success && p) {
                if (n >= 12 && da == XA_CARDINAL && di == 32) {
                    long *v = (long *)p;
                    if (v[0] > left) left = v[0];
                    if (v[1] > right) right = v[1];
                    if (v[2] > top) top = v[2];
                    if (v[3] > bottom) bottom = v[3];
                    have = 1;
                }
                XFree(p);
                p = NULL;
            }
            if (!have && XGetWindowProperty(dpy, c->window, netatom[NetWMStrut], 0L, 4L,
                False, XA_CARDINAL, &da, &di, &n, &dl, &p) == Success && p) {
                if (n >= 4 && da == XA_CARDINAL && di == 32) {
                    long *v = (long *)p;
                    if (v[0] > left) left = v[0];
                    if (v[1] > right) right = v[1];
                    if (v[2] > top) top = v[2];
                    if (v[3] > bottom) bottom = v[3];
                }
                XFree(p);
            }
        }
        m->wx = m->x + (int)left;
        m->wy = m->y + (int)top;
        m->ww = m->width > (unsigned int)(left + right) ? m->width - (unsigned int)(left + right) : 1;
        m->wh = m->height > (unsigned int)(top + bottom) ? m->height - (unsigned int)(top + bottom) : 1;
    }
}

static void map_client(Client *c) {
    if (c->mapped) return;
    if (c->frame) {
        XMapWindow(dpy, c->frame);
        XMapWindow(dpy, c->window);
    } else {
        XMapWindow(dpy, c->window);
    }
    c->mapped = 1;
}

static void unmap_client(Client *c) {
    if (!c->mapped) return;
    if (c->frame) {
        XUnmapWindow(dpy, c->frame);
    } else {
        c->ignore_unmap++;
        XUnmapWindow(dpy, c->window);
    }
    c->mapped = 0;
}

static Client *wintoclient(Window w) {
    Client *c;
    Monitor *m;
    for (m = mons; m; m = m->next)
        for (c = m->clients; c; c = c->next)
            if (c->window == w) return c;
    return NULL;
}

static Client *frametoclient(Window w) {
    Client *c;
    Monitor *m;
    for (m = mons; m; m = m->next)
        for (c = m->clients; c; c = c->next)
            if (c->frame == w) return c;
    return NULL;
}

static int getrootptr(int *x, int *y) {
    int di;
    unsigned int dui;
    Window dummy;
    return XQueryPointer(dpy, root, &dummy, &dummy, x, y, &di, &di, &dui);
}

static void updatesizehints(Client *c) {
    long msize;
    XSizeHints size;
    c->minw = c->minh = 0;
    c->maxw = c->maxh = 0;
    if (!XGetWMNormalHints(dpy, c->window, &size, &msize))
        size.flags = PSize;
    if (size.flags & PMinSize) {
        c->minw = size.min_width;
        c->minh = size.min_height;
    }
    if (size.flags & PMaxSize) {
        c->maxw = size.max_width;
        c->maxh = size.max_height;
    }
}

static void updatetitle(Client *c) {
    if (!gettextprop(c->window, netatom[NetWMName], c->title, sizeof(c->title))
        && !gettextprop(c->window, XA_WM_NAME, c->title, sizeof(c->title)))
        c->title[0] = '\0';
    if (c->frame) drawtitle(c);
}

static void updatewindowtype(Client *c) {
    Atom types[16];
    Atom states[32];
    int nt = getatomprops(c, netatom[NetWMWindowType], types, LENGTH(types));
    int ns = getatomprops(c, netatom[NetWMState], states, LENGTH(states));
    int i, panel = 0, dialog = 0, fs = 0, maxv = 0, maxh = 0;
    for (i = 0; i < nt; i++) {
        if (types[i] == netatom[NetWMWindowTypeDock] ||
            types[i] == netatom[NetWMWindowTypeToolbar] ||
            types[i] == netatom[NetWMWindowTypeMenu] ||
            types[i] == netatom[NetWMWindowTypeSplash] ||
            types[i] == netatom[NetWMWindowTypeNotification] ||
            types[i] == netatom[NetWMWindowTypeUtility])
            panel = 1;
        else if (types[i] == netatom[NetWMWindowTypeDialog])
            dialog = 1;
    }
    if (panel) {
        c->state = STATE_FLOATING;
        c->border_width = 0;
        c->ispanel = 1;
    } else if (dialog) {
        c->state = STATE_FLOATING;
    }
    for (i = 0; i < ns; i++) {
        if (states[i] == netatom[NetWMStateFullScreen]) fs = 1;
        else if (states[i] == netatom[NetWMStateMaximizedVert]) maxv = 1;
        else if (states[i] == netatom[NetWMStateMaximizedHorz]) maxh = 1;
    }
    c->want_fullscreen = fs;
    c->want_maximized = maxv && maxh;
    if (c->managed) {
        setfullscreen(c, fs);
        if (c->want_maximized) setmaximized(c, 1);
        else if (c->state == STATE_MAXIMIZED) setmaximized(c, 0);
    }
}

static void updatewmhints(Client *c) {
    XWMHints *wmh;
    if ((wmh = XGetWMHints(dpy, c->window))) {
        if (c == selmon->sel && wmh->flags & XUrgencyHint) {
            wmh->flags &= ~XUrgencyHint;
            XSetWMHints(dpy, c->window, wmh);
            c->urgent = 0;
        } else
            c->urgent = (wmh->flags & XUrgencyHint) ? 1 : 0;
        XFree(wmh);
        setnetwmstate(c, netatom[NetWMStateDemandsAttention], c->urgent);
    }
}

static int getatomprops(Client *c, Atom prop, Atom *out, int max) {
    int di, n = 0;
    unsigned long nitems, dl;
    unsigned char *p = NULL;
    Atom da;
    if (max <= 0) return 0;
    if (XGetWindowProperty(dpy, c->window, prop, 0L, 32L, False, XA_ATOM,
        &da, &di, &nitems, &dl, &p) != Success || !p)
        return 0;
    if (da == XA_ATOM && di == 32) {
        Atom *atoms = (Atom *)p;
        for (unsigned long i = 0; i < nitems && n < max; i++)
            out[n++] = atoms[i];
    }
    XFree(p);
    return n;
}

static long getdesktop(Window w) {
    int di;
    unsigned long dl, n;
    unsigned char *p = NULL;
    Atom da;
    long result = -1;
    if (XGetWindowProperty(dpy, w, netatom[NetWMDesktop], 0L, 1L, False, XA_CARDINAL,
        &da, &di, &n, &dl, &p) != Success || !p) return -1;
    if (n == 0 || da != XA_CARDINAL || di != 32) { XFree(p); return -1; }
    result = *(long *)p;
    XFree(p);
    return result;
}

static long getstate(Window w) {
    int format;
    long result = -1;
    unsigned char *p = NULL;
    unsigned long n, extra;
    Atom real;
    if (XGetWindowProperty(dpy, w, wmatom[WMState], 0L, 2L, False, wmatom[WMState],
        &real, &format, &n, &extra, (unsigned char **)&p) != Success)
        return -1;
    if (n != 0 && format == 32) result = *(long *)p;
    XFree(p);
    return result;
}

static void setclientstate(Client *c, long state) {
    long data[] = { state, None };
    XChangeProperty(dpy, c->window, wmatom[WMState], wmatom[WMState], 32,
        PropModeReplace, (unsigned char *)data, 2);
}

static int sendevent(Client *c, Atom proto) {
    int n;
    Atom *protocols;
    int exists = 0;
    XEvent ev;
    if (XGetWMProtocols(dpy, c->window, &protocols, &n)) {
        while (!exists && n--)
            exists = protocols[n] == proto;
        XFree(protocols);
    }
    if (exists) {
        ev.type = ClientMessage;
        ev.xclient.window = c->window;
        ev.xclient.message_type = wmatom[WMProtocols];
        ev.xclient.format = 32;
        ev.xclient.data.l[0] = proto;
        ev.xclient.data.l[1] = CurrentTime;
        XSendEvent(dpy, c->window, False, NoEventMask, &ev);
    }
    return exists;
}

static void setfocus(Client *c) {
    XSetInputFocus(dpy, c->window, RevertToPointerRoot, CurrentTime);
    XChangeProperty(dpy, root, netatom[NetActiveWindow],
        XA_WINDOW, 32, PropModeReplace, (unsigned char *)&(c->window), 1);
    sendevent(c, wmatom[WMTakeFocus]);
}

static void focus(Client *c) {
    Client *old = selmon->sel;
    if (!c || !ISVISIBLE(c))
        for (c = selmon->stack; c && !ISVISIBLE(c); c = c->next_stack);
    if (selmon->sel && selmon->sel != c)
        unfocus(selmon->sel, 0);
    if (c) {
        if (c->monitor != selmon) selmon = c->monitor;
        if (c->state == STATE_FLOATING) {
            if (c->frame) XRaiseWindow(dpy, c->frame);
            else XRaiseWindow(dpy, c->window);
        }
        selmon->sel = c;
        selmon->lastsel[selmon->workspace] = c;
        setfocus(c);
        XUngrabKeyboard(dpy, CurrentTime);
        if (!c->frame) XSetWindowBorder(dpy, c->window, col_sel_outer_border);
        drawtitle(c);
        if (old && old != c) {
            if (!old->frame) XSetWindowBorder(dpy, old->window, col_norm_outer_border);
            drawtitle(old);
        }
    } else {
        selmon->sel = NULL;
        XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
        XUngrabKeyboard(dpy, CurrentTime);
        XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
    }
}

static void unfocus(Client *c, int setfocus) {
    if (!c) return;
    grabbuttons(c, 0);
    if (!c->frame) XSetWindowBorder(dpy, c->window, col_norm_outer_border);
    drawtitle(c);
    if (setfocus) {
        XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
        XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
    }
}

static void grabbuttons(Client *c, int focused) {
    unsigned int i, j;
    unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
    XUngrabButton(dpy, AnyButton, AnyModifier, c->window);
    if (!focused) return;
    for (i = 0; i < (unsigned int)nbuttons; i++)
        for (j = 0; j < LENGTH(modifiers); j++)
            XGrabButton(dpy, buttons[i].button,
                buttons[i].mod | modifiers[j],
                c->window, False, BUTTONMASK,
                GrabModeAsync, GrabModeAsync, None, None);
}

static void updatenumlockmask(void) {
    unsigned int i, j;
    XModifierKeymap *modmap;
    numlockmask = 0;
    modmap = XGetModifierMapping(dpy);
    for (i = 0; i < 8; i++)
        for (j = 0; j < (unsigned int)modmap->max_keypermod; j++)
            if (modmap->modifiermap[i * modmap->max_keypermod + j]
                == XKeysymToKeycode(dpy, XK_Num_Lock))
                numlockmask = (1 << i);
    XFreeModifiermap(modmap);
}

static void grabkeys(void) {
    unsigned int i, j;
    unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
    XUngrabKey(dpy, AnyKey, AnyModifier, root);
    for (i = 0; i < (unsigned int)nkeys; i++) {
        keycodes[i] = XKeysymToKeycode(dpy, keys[i].keysym);
        if (!keycodes[i]) continue;
        for (j = 0; j < LENGTH(modifiers); j++)
            XGrabKey(dpy, keycodes[i], keys[i].mod | modifiers[j], root,
                True, GrabModeAsync, GrabModeAsync);
    }
}

static void get_col_metrics(Monitor *m, int *col_w, int *inner) {
    int usable = (int)m->ww - 2 * outer_gaps;
    int cw = (usable - inner_gaps) / 2;
    int ai = usable - 2 * cw;
    if (cw < 200) { cw = 200; ai = inner_gaps; }
    *col_w = cw;
    *inner = ai;
}

static int is_tiled(Client *c) {
    return ISVISIBLE(c) && c->state != STATE_FLOATING
        && c->state != STATE_FULLSCREEN && !c->ispanel;
}

static int is_wide(Client *c) {
    return c->state == STATE_MAXIMIZED || c->custom_width;
}

static int get_total_strip_width(Monitor *m) {
    Client *c;
    int col_w, inner, total = 2 * outer_gaps, n = 0;
    get_col_metrics(m, &col_w, &inner);
    for (c = m->clients; c; c = c->next) {
        if (!is_tiled(c)) continue;
        total += (is_wide(c) ? (int)c->width : col_w) + inner;
        n++;
    }
    if (n > 0)
        total -= inner;
    return total;
}

static void clamp_scroll(Monitor *m) {
    int total = get_total_strip_width(m);
    int width = (int)m->ww;
    if (total < width) {
        if (strip_align == 0)
            m->scroll_x = 0;
        else if (strip_align == 1)
            m->scroll_x = width - total;
        else
            m->scroll_x = (width - total) / 2;
    } else {
        int max_scroll = -(total - width);
        if (max_scroll > 0) max_scroll = 0;
        if (m->scroll_x > 0) m->scroll_x = 0;
        if (m->scroll_x < max_scroll) m->scroll_x = max_scroll;
    }
}

static void arrange(Monitor *m) {
    Client *c;
    int total_width = 0;
    int col_w, inner;
    int mon_x = m->wx;
    int mon_y = m->wy;
    int mon_h = (int)m->wh;
    int title_h = show_titlebar ? TITLE_HEIGHT : 0;

    get_col_metrics(m, &col_w, &inner);
    clamp_scroll(m);

    for (c = m->clients; c; c = c->next) {
        if (c->ispanel) {
            map_client(c);
            XRaiseWindow(dpy, c->window);
            continue;
        }
        if (!ISVISIBLE(c)) {
            unmap_client(c);
            continue;
        }
        map_client(c);
        if (c->state == STATE_FLOATING) {
            resize(c, c->x, c->y, c->width, c->height, 0);
            if (c->frame) XRaiseWindow(dpy, c->frame);
            else XRaiseWindow(dpy, c->window);
            continue;
        }
        if (c->state == STATE_FULLSCREEN) {
            resize(c, m->x, m->y, m->width, m->height, 0);
            continue;
        }
        c->x = mon_x + outer_gaps + total_width + m->scroll_x;
        c->y = mon_y + outer_gaps;
        c->height = mon_h - 2 * outer_gaps - title_h;
        if (!is_wide(c)) c->width = col_w;
        resize(c, c->x, c->y, c->width, c->height, 0);
        total_width += (is_wide(c) ? (int)c->width : col_w) + inner;
    }
    XFlush(dpy);
}

static void restack(Monitor *m) {
    Client *c;
    XEvent ev;
    if (!m->sel) return;
    for (c = m->stack; c; c = c->next_stack)
        if (c->ispanel)
            XRaiseWindow(dpy, c->window);
    for (c = m->stack; c; c = c->next_stack)
        if (c->state == STATE_FLOATING && ISVISIBLE(c) && c != m->sel) {
            if (c->frame) XRaiseWindow(dpy, c->frame);
            else XRaiseWindow(dpy, c->window);
        }
    if (m->sel->state == STATE_FLOATING) {
        if (m->sel->frame) XRaiseWindow(dpy, m->sel->frame);
        else XRaiseWindow(dpy, m->sel->window);
    }
    while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
}

static void resize(Client *c, int x, int y, int w, int h, int interact) {
    (void)interact;
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    if (c->state == STATE_FLOATING || c->custom_width) {
        if (c->minw > 0 && w < c->minw) w = c->minw;
        if (c->minh > 0 && h < c->minh) h = c->minh;
        if (c->maxw > 0 && w > c->maxw) w = c->maxw;
        if (c->maxh > 0 && h > c->maxh) h = c->maxh;
    }
    c->x = x;
    c->y = y;
    c->width = w;
    c->height = h;
    int title_h = (c->state == STATE_FULLSCREEN || c->ispanel || !c->frame) ? 0 : TITLE_HEIGHT;
    int bw = (c->state == STATE_FULLSCREEN || c->ispanel) ? 0 : border_width;
    if (c->applied_valid && c->applied_x == x && c->applied_y == y
        && c->applied_w == (unsigned int)w && c->applied_h == (unsigned int)h
        && c->applied_title_h == title_h && c->applied_bw == bw)
        return;
    c->applied_valid = 1;
    c->applied_x = x;
    c->applied_y = y;
    c->applied_w = w;
    c->applied_h = h;
    c->applied_title_h = title_h;
    c->applied_bw = bw;
    int frame_w = w;
    int frame_h = h + title_h;
    int client_x = bw;
    int client_y = title_h + bw;
    int cw = w - 2 * bw;
    int chh = h - 2 * bw;
    if (cw < 1) cw = 1;
    if (chh < 1) chh = 1;

    if (c->frame) {
        XMoveResizeWindow(dpy, c->frame, x, y, frame_w, frame_h);
        XMoveResizeWindow(dpy, c->window, client_x, client_y, cw, chh);
        drawtitle(c);
    } else {
        XWindowChanges wc;
        wc.x = x + client_x;
        wc.y = y + client_y;
        wc.width = cw;
        wc.height = chh;
        wc.border_width = bw;
        XConfigureWindow(dpy, c->window, CWX|CWY|CWWidth|CWHeight|CWBorderWidth, &wc);
    }
    configure(c);
}

static void configure(Client *c) {
    XConfigureEvent ce;
    ce.type = ConfigureNotify;
    ce.display = dpy;
    ce.event = c->window;
    ce.window = c->window;
    int title_h = (c->state == STATE_FULLSCREEN || c->ispanel || !c->frame) ? 0 : TITLE_HEIGHT;
    int bw = (c->state == STATE_FULLSCREEN || c->ispanel) ? 0 : border_width;
    if (c->frame) {
        ce.x = bw;
        ce.y = title_h + bw;
    } else {
        ce.x = c->x + bw;
        ce.y = c->y + bw;
    }
    int cw = c->width - 2 * bw;
    int chh = c->height - 2 * bw;
    ce.width = cw < 1 ? 1 : cw;
    ce.height = chh < 1 ? 1 : chh;
    ce.border_width = c->frame ? 0 : bw;
    ce.above = None;
    ce.override_redirect = False;
    XSendEvent(dpy, c->window, False, StructureNotifyMask, (XEvent *)&ce);
}

static void showhide(Client *c) {
    if (!c) return;
    if (ISVISIBLE(c)) {
        if (c->frame) XMoveWindow(dpy, c->frame, c->x, c->y);
        else XMoveWindow(dpy, c->window, c->x, c->y);
        map_client(c);
        showhide(c->next);
    } else {
        showhide(c->next);
        unmap_client(c);
    }
}

void ws_up(const char *arg) {
    (void)arg;
    int prev_ws = selmon->workspace;
    if (selmon->workspace > 0) {
        selmon->lastsel[prev_ws] = selmon->sel;
        selmon->workspace--;
        selmon->scroll_x = 0;
        if (selmon->lastsel[selmon->workspace]) {
            focus(selmon->lastsel[selmon->workspace]);
            ensure_visible(selmon->lastsel[selmon->workspace]);
        } else {
            focus(NULL);
        }
        restack(selmon);
        arrange(selmon);
        updateworkspaces();
    }
}

void ws_down(const char *arg) {
    (void)arg;
    int prev_ws = selmon->workspace;
    if (selmon->workspace < MAX_WORKSPACES - 1) {
        selmon->lastsel[prev_ws] = selmon->sel;
        selmon->workspace++;
        selmon->scroll_x = 0;
        if (selmon->lastsel[selmon->workspace]) {
            focus(selmon->lastsel[selmon->workspace]);
            ensure_visible(selmon->lastsel[selmon->workspace]);
        } else {
            focus(NULL);
        }
        restack(selmon);
        arrange(selmon);
        updateworkspaces();
    }
}

void setgaps(const char *arg) {
    if (!arg) return;
    if (arg[0] == '0') { inner_gaps = 30; outer_gaps = 50; }
    else if (arg[0] == '-') { inner_gaps -= 2; outer_gaps -= 2; }
    else if (arg[0] == '+') { inner_gaps += 2; outer_gaps += 2; }
    if (inner_gaps < 0) inner_gaps = 0;
    if (outer_gaps < 0) outer_gaps = 0;
    arrange(selmon);
}

static void ensure_visible(Client *c) {
    Monitor *m;
    Client *cl;
    int pos = 0, found = 0;
    int col_w, inner;
    int win_w;

    if (!c) return;
    m = c->monitor ? c->monitor : selmon;
    if (c->state == STATE_FLOATING || c->state == STATE_FULLSCREEN) return;
    get_col_metrics(m, &col_w, &inner);

    for (cl = m->clients; cl; cl = cl->next) {
        if (!is_tiled(cl)) continue;
        if (cl == c) { found = 1; break; }
        pos += (is_wide(cl) ? (int)cl->width : col_w) + inner;
    }
    if (!found) return;

    win_w = is_wide(c) ? (int)c->width : col_w;
    int win_left = m->wx + outer_gaps + pos + m->scroll_x;
    int win_right = win_left + win_w;
    int bound_left = m->wx + outer_gaps;
    int bound_right = m->wx + (int)m->ww - outer_gaps;

    if (win_left < bound_left)
        m->scroll_x += bound_left - win_left;
    else if (win_right > bound_right)
        m->scroll_x -= win_right - bound_right;

    clamp_scroll(m);
}

void focusleft(const char *arg) {
    (void)arg;
    Client *c;
    if (!selmon->sel) {
        for (c = selmon->clients; c && !ISVISIBLE(c); c = c->next);
        if (c) focus(c);
        return;
    }
    for (c = selmon->sel->prev; c && !ISVISIBLE(c); c = c->prev);
    if (!c)
        for (c = selmon->stack; c && !ISVISIBLE(c); c = c->next_stack);
    if (c) {
        focus(c);
        ensure_visible(c);
        restack(selmon);
        arrange(selmon);
    }
}

void focusright(const char *arg) {
    (void)arg;
    Client *c;
    if (!selmon->sel) {
        for (c = selmon->clients; c && !ISVISIBLE(c); c = c->next);
        if (c) focus(c);
        return;
    }
    for (c = selmon->sel->next; c && !ISVISIBLE(c); c = c->next);
    if (!c)
        for (c = selmon->clients; c && !ISVISIBLE(c); c = c->next);
    if (c) {
        focus(c);
        ensure_visible(c);
        restack(selmon);
        arrange(selmon);
    }
}

static void attach(Client *c) {
    Monitor *m = c->monitor;
    c->next = NULL;
    c->prev = NULL;
    if (insert_end) {
        c->prev = m->clients_tail;
        if (m->clients_tail)
            m->clients_tail->next = c;
        else
            m->clients = c;
        m->clients_tail = c;
    } else {
        c->next = m->clients;
        if (m->clients)
            m->clients->prev = c;
        else
            m->clients_tail = c;
        m->clients = c;
    }
}

static void attachstack(Client *c) {
    c->next_stack = c->monitor->stack;
    c->prev_stack = NULL;
    if (c->monitor->stack) c->monitor->stack->prev_stack = c;
    c->monitor->stack = c;
}

static void detach(Client *c) {
    Monitor *m = c->monitor;
    if (c->prev) c->prev->next = c->next;
    else m->clients = c->next;
    if (c->next) c->next->prev = c->prev;
    else m->clients_tail = c->prev;
    if (c->prev_stack) c->prev_stack->next_stack = c->next_stack;
    else m->stack = c->next_stack;
    if (c->next_stack) c->next_stack->prev_stack = c->prev_stack;
    c->next = NULL;
    c->prev = NULL;
    c->next_stack = NULL;
    c->prev_stack = NULL;
}

static void manage(Window w, XWindowAttributes *wa) {
    Client *c;
    Window trans = None;
    XWindowChanges wc;
    c = calloc(1, sizeof(Client));
    if (!c) die("calloc:");
    c->window = w;
    c->x = wa->x;
    c->y = wa->y;
    c->height = wa->height;
    int col_w, inner;
    get_col_metrics(selmon, &col_w, &inner);

    c->state = STATE_NORMAL;
    c->prev_state = STATE_NORMAL;
    c->width = col_w;
    c->workspace = selmon->workspace;
    {
        long ws = getdesktop(w);
        if (ws >= 0 && ws < MAX_WORKSPACES)
            c->workspace = (int)ws;
    }
    c->monitor = selmon;
    c->mapped = 0;
    c->published_ws = -1;
    c->border_width = border_width;
    updatesizehints(c);
    updatetitle(c);
    updatewindowtype(c);
    c->prev_state = c->state;
    updatewmhints(c);
    XSelectInput(dpy, w, EnterWindowMask|FocusChangeMask|PropertyChangeMask|StructureNotifyMask);
    grabbuttons(c, 0);

    int is_transient = (XGetTransientForHint(dpy, w, &trans) && trans != None);
    c->is_transient = is_transient;
    if (is_transient)
        c->state = STATE_FLOATING;

    if (c->state == STATE_NORMAL && !c->ispanel)
        c->height = (int)selmon->wh - 2 * outer_gaps - (show_titlebar ? TITLE_HEIGHT : 0);
    if (c->height < 1) c->height = 1;

    if (c->ispanel || is_transient || !show_titlebar) {
        c->frame = None;
        XSetWindowBorder(dpy, w, c->ispanel ? 0 : c->border_width);
    } else {
        XSetWindowAttributes frame_attr;
        frame_attr.override_redirect = True;
        frame_attr.background_pixel = 0;
        frame_attr.event_mask = ButtonPressMask | ExposureMask | EnterWindowMask;
        c->frame = XCreateWindow(dpy, root, c->x, c->y, c->width, c->height + TITLE_HEIGHT, 0,
                                 CopyFromParent, InputOutput, CopyFromParent,
                                 CWOverrideRedirect | CWBackPixel | CWEventMask, &frame_attr);
        XSelectInput(dpy, c->frame, ButtonPressMask | ExposureMask | EnterWindowMask);
        if (wa->map_state != IsUnmapped)
            c->ignore_unmap++;
        XReparentWindow(dpy, w, c->frame, 0, TITLE_HEIGHT);
        wc.border_width = 0;
        XConfigureWindow(dpy, w, CWBorderWidth, &wc);
        c->border_width = 0;
    }

    attach(c);
    attachstack(c);
    c->managed = 1;
    setclientstate(c, NormalState);
    if (c->frame) {
        XMapWindow(dpy, c->frame);
        XMapWindow(dpy, c->window);
    } else {
        XMapWindow(dpy, c->window);
    }
    c->mapped = 1;
    if (c->want_fullscreen)
        setfullscreen(c, 1);
    else if (c->want_maximized)
        setmaximized(c, 1);
    updateworkarea();
    if (!c->ispanel) {
        focus(c);
        ensure_visible(c);
    }
    restack(selmon);
    arrange(selmon);
    updateclientlist();
    updateworkspaces();
}

static void reframe(Client *c) {
    int want = show_titlebar && !c->ispanel && !c->is_transient;
    if (want && !c->frame) {
        XWindowChanges wc;
        XSetWindowAttributes frame_attr;
        frame_attr.override_redirect = True;
        frame_attr.background_pixel = 0;
        frame_attr.event_mask = ButtonPressMask | ExposureMask | EnterWindowMask;
        c->frame = XCreateWindow(dpy, root, c->x, c->y, c->width, c->height + TITLE_HEIGHT, 0,
                                 CopyFromParent, InputOutput, CopyFromParent,
                                 CWOverrideRedirect | CWBackPixel | CWEventMask, &frame_attr);
        XSelectInput(dpy, c->frame, ButtonPressMask | ExposureMask | EnterWindowMask);
        XReparentWindow(dpy, c->window, c->frame, 0, TITLE_HEIGHT);
        wc.border_width = 0;
        XConfigureWindow(dpy, c->window, CWBorderWidth, &wc);
        c->border_width = 0;
        c->applied_valid = 0;
        XMapWindow(dpy, c->frame);
        drawtitle(c);
    } else if (!want && c->frame) {
        XSetErrorHandler(xerrordummy);
        XReparentWindow(dpy, c->window, root, c->x, c->y + TITLE_HEIGHT);
        XDestroyWindow(dpy, c->frame);
        c->frame = None;
        c->applied_valid = 0;
        XSetWindowBorder(dpy, c->window, c->ispanel ? 0 : c->border_width);
        XSetErrorHandler(xerror);
    }
}

static void updateframes(void) {
    Monitor *m;
    Client *c;
    for (m = mons; m; m = m->next)
        for (c = m->clients; c; c = c->next)
            reframe(c);
}

static void unmanage(Client *c, int destroyed) {
    Monitor *m = c->monitor;
    Client *nextfocus = NULL;
    Monitor *mon;
    int wassel = (c == m->sel);

    for (mon = mons; mon; mon = mon->next) {
        int i;
        for (i = 0; i < MAX_WORKSPACES; i++) {
            if (mon->lastsel[i] == c)
                mon->lastsel[i] = NULL;
        }
    }

    if (wassel) {
        if (c->prev && ISVISIBLE(c->prev))
            nextfocus = c->prev;
        else if (c->next && ISVISIBLE(c->next))
            nextfocus = c->next;
        else {
            Client *cl;
            for (cl = m->clients; cl && (cl == c || !ISVISIBLE(cl)); cl = cl->next);
            if (cl) nextfocus = cl;
        }
        m->sel = NULL;
        if (!shutting_down) unfocus(c, 0);
    } else {
        nextfocus = m->sel;
    }

    detach(c);

    if (!destroyed) {
        XSetErrorHandler(xerrordummy);
        XSelectInput(dpy, c->window, NoEventMask);
        if (c->frame) {
            int th = TITLE_HEIGHT;
            XReparentWindow(dpy, c->window, root, c->x, c->y + th);
            XDestroyWindow(dpy, c->frame);
        } else {
            XWindowChanges wc;
            wc.border_width = c->border_width;
            XConfigureWindow(dpy, c->window, CWBorderWidth, &wc);
        }
        XUngrabButton(dpy, AnyButton, AnyModifier, c->window);
        setclientstate(c, WithdrawnState);
        XSetErrorHandler(xerror);
        XFlush(dpy);
    } else {
        if (c->frame) XDestroyWindow(dpy, c->frame);
    }
    free(c);
    if (shutting_down) return;
    XUngrabKeyboard(dpy, CurrentTime);
    if (m == selmon) focus(nextfocus);
    updateclientlist();
    restack(m);
    updateworkarea();
    arrange(m);
    updateworkspaces();
}

static void closeclient(Client *c) {
    if (!c) return;
    if (!sendevent(c, wmatom[WMDelete])) {
        XSetErrorHandler(xerrordummy);
        XKillClient(dpy, c->window);
        XSync(dpy, False);
        XSetErrorHandler(xerror);
    }
}

void killclient(const char *arg) {
    (void)arg;
    closeclient(selmon->sel);
}

static void setnetwmstate(Client *c, Atom atom, int on) {
    Atom states[32];
    Atom next[33];
    int ns = getatomprops(c, netatom[NetWMState], states, LENGTH(states));
    int i, n = 0, found = 0;
    for (i = 0; i < ns; i++) {
        if (states[i] == atom) { found = 1; continue; }
        next[n++] = states[i];
    }
    if (on && !found) next[n++] = atom;
    if (n > 0)
        XChangeProperty(dpy, c->window, netatom[NetWMState], XA_ATOM, 32,
            PropModeReplace, (unsigned char *)next, n);
    else
        XDeleteProperty(dpy, c->window, netatom[NetWMState]);
}

static void setfullscreen(Client *c, int fullscreen) {
    if (fullscreen && c->state != STATE_FULLSCREEN) {
        c->prev_state = c->state;
        c->state = STATE_FULLSCREEN;
        c->fs_x = c->x;
        c->fs_y = c->y;
        c->fs_width = c->width;
        c->fs_height = c->height;
        c->fs_border_width = c->border_width;
        c->border_width = 0;
        resize(c, c->monitor->x, c->monitor->y, c->monitor->width, c->monitor->height, 0);
        if (c->frame) XRaiseWindow(dpy, c->frame);
        else XRaiseWindow(dpy, c->window);
        setnetwmstate(c, netatom[NetWMStateFullScreen], 1);
        c->want_fullscreen = 1;
    } else if (!fullscreen && c->state == STATE_FULLSCREEN) {
        c->state = c->prev_state;
        c->x = c->fs_x;
        c->y = c->fs_y;
        c->width = c->fs_width;
        c->height = c->fs_height;
        c->border_width = c->fs_border_width;
        resize(c, c->x, c->y, c->width, c->height, 0);
        setnetwmstate(c, netatom[NetWMStateFullScreen], 0);
        c->want_fullscreen = 0;
    }
}

static void setmaximized(Client *c, int maximized) {
    if (maximized) {
        if (c->state == STATE_FULLSCREEN) setfullscreen(c, 0);
        if (c->state == STATE_MAXIMIZED) return;
        c->prev_state = c->state;
        c->max_x = c->x;
        c->max_y = c->y;
        c->max_width = c->width;
        c->max_height = c->height;
        c->state = STATE_MAXIMIZED;
        c->width = (int)c->monitor->ww - 2 * outer_gaps;
        setnetwmstate(c, netatom[NetWMStateMaximizedVert], 1);
        setnetwmstate(c, netatom[NetWMStateMaximizedHorz], 1);
        c->want_maximized = 1;
    } else if (c->state == STATE_MAXIMIZED) {
        c->state = c->prev_state;
        c->x = c->max_x;
        c->y = c->max_y;
        c->width = c->max_width;
        c->height = c->max_height;
        setnetwmstate(c, netatom[NetWMStateMaximizedVert], 0);
        setnetwmstate(c, netatom[NetWMStateMaximizedHorz], 0);
        c->want_maximized = 0;
    }
}

void togglefullscreen(const char *arg) {
    (void)arg;
    if (!selmon->sel) return;
    setfullscreen(selmon->sel, selmon->sel->state != STATE_FULLSCREEN);
    restack(selmon);
    arrange(selmon);
}

void togglemaximize(const char *arg) {
    (void)arg;
    Client *c = selmon->sel;
    if (!c) return;
    if (c->state == STATE_FULLSCREEN) {
        setfullscreen(c, 0);
        if (c->state == STATE_MAXIMIZED) {
            focus(c);
            ensure_visible(c);
            restack(selmon);
            arrange(selmon);
            return;
        }
    }
    setmaximized(c, c->state != STATE_MAXIMIZED);
    focus(c);
    ensure_visible(c);
    restack(selmon);
    arrange(selmon);
}

void togglefloating(const char *arg) {
    (void)arg;
    Client *c = selmon->sel;
    if (!c) return;
    if (c->state == STATE_FULLSCREEN) setfullscreen(c, 0);
    if (c->state == STATE_MAXIMIZED) setmaximized(c, 0);
    if (c->state == STATE_FLOATING) {
        c->state = STATE_NORMAL;
        c->x = c->float_x;
        c->y = c->float_y;
        c->width = c->float_width;
        c->height = c->float_height;
    } else {
        c->float_x = c->x;
        c->float_y = c->y;
        c->float_width = c->width;
        c->float_height = c->height;
        c->state = STATE_FLOATING;
        if (c->width < 100 || c->height < 100) {
            c->width = (selmon->ww - 2 * outer_gaps) / 2;
            c->height = (selmon->wh - 2 * outer_gaps) / 2 - (show_titlebar ? TITLE_HEIGHT : 0);
        }
        c->x = selmon->wx + ((int)selmon->ww - (int)c->width) / 2;
        c->y = selmon->wy + ((int)selmon->wh - (int)c->height) / 2;
    }
    restack(selmon);
    arrange(selmon);
}

void zoom(const char *arg) {
    (void)arg;
    Client *c = selmon->sel;
    if (!c || c->state != STATE_NORMAL || c->custom_width) return;
    if (c != selmon->clients) {
        if (c->prev) c->prev->next = c->next;
        if (c->next) c->next->prev = c->prev;
        else selmon->clients_tail = c->prev;
        c->next = selmon->clients;
        c->prev = NULL;
        selmon->clients->prev = c;
        selmon->clients = c;
    }
    focus(c);
    restack(selmon);
    arrange(selmon);
}

void tag(const char *arg) {
    int tag;
    if (!selmon->sel || !arg) return;
    tag = arg[0] - '0';
    if (tag >= 0 && tag < MAX_WORKSPACES)
        selmon->sel->workspace = tag;
    focus(NULL);
    restack(selmon);
    arrange(selmon);
    updateworkspaces();
}

void toggletag(const char *arg) {
    int tag;
    if (!selmon->sel || !arg) return;
    tag = arg[0] - '0';
    if (tag >= 0 && tag < MAX_WORKSPACES) {
        if (selmon->sel->workspace == tag) selmon->sel->workspace = 0;
        else selmon->sel->workspace = tag;
    }
    focus(NULL);
    restack(selmon);
    arrange(selmon);
    updateworkspaces();
}

void view(const char *arg) {
    int tag;
    int prev_ws = selmon->workspace;
    selmon->lastsel[prev_ws] = selmon->sel;
    if (arg == NULL) {
        selmon->workspace = selmon->workspace ? 0 : 1;
    } else {
        tag = arg[0] - '0';
        if (tag >= 0 && tag < MAX_WORKSPACES) selmon->workspace = tag;
    }
    selmon->scroll_x = 0;
    if (selmon->lastsel[selmon->workspace]) {
        focus(selmon->lastsel[selmon->workspace]);
        ensure_visible(selmon->lastsel[selmon->workspace]);
    } else {
        focus(NULL);
    }
    restack(selmon);
    arrange(selmon);
    updateworkspaces();
}

void toggleview(const char *arg) {
    int tag;
    if (!arg) return;
    tag = arg[0] - '0';
    if (tag >= 0 && tag < MAX_WORKSPACES) {
        if (selmon->workspace == tag) selmon->workspace = 0;
        else selmon->workspace = tag;
    }
    focus(NULL);
    restack(selmon);
    arrange(selmon);
    updateworkspaces();
}

void movemouse(const char *arg) {
    (void)arg;
    int x, y, ocx, ocy, nx = 0, ny = 0;
    Client *c;
    XEvent ev;
    Time lasttime = 0;
    if (!(c = selmon->sel)) return;
    if (c->state == STATE_FULLSCREEN) return;

    restack(selmon);
    ocx = c->x;
    ocy = c->y;
    if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
        None, cursor_move, CurrentTime) != GrabSuccess) return;
    if (!getrootptr(&x, &y)) {
        XUngrabPointer(dpy, CurrentTime);
        return;
    }

    int orig_scroll_x = selmon->scroll_x;
    int dragging = 0;

    do {
        XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask|StructureNotifyMask|SubstructureNotifyMask, &ev);
        switch (ev.type) {
        case DestroyNotify:
            if (ev.xdestroywindow.window == c->window || ev.xdestroywindow.window == c->frame)
                goto done_move;
            handler[ev.type](&ev);
            break;
        case UnmapNotify:
            if (ev.xunmap.window == c->window || ev.xunmap.window == c->frame)
                goto done_move;
            handler[ev.type](&ev);
            break;
        case ConfigureRequest:
        case Expose:
        case MapRequest:
            handler[ev.type](&ev);
            break;
        case MotionNotify:
            if ((ev.xmotion.time - lasttime) <= (1000 / 60)) continue;
            lasttime = ev.xmotion.time;

            if (c->state == STATE_FLOATING) {
                nx = ocx + (ev.xmotion.x - x);
                ny = ocy + (ev.xmotion.y - y);
                if (abs(selmon->wx + outer_gaps - nx) < snap_val) nx = selmon->wx + outer_gaps;
                else if (abs((selmon->wx + (int)selmon->ww - outer_gaps) - (nx + (int)c->width)) < snap_val)
                    nx = selmon->wx + (int)selmon->ww - outer_gaps - c->width;
                if (abs(selmon->wy + outer_gaps - ny) < snap_val) ny = selmon->wy + outer_gaps;
                else if (abs((selmon->wy + (int)selmon->wh - outer_gaps) - (ny + (int)c->height)) < snap_val)
                    ny = selmon->wy + (int)selmon->wh - outer_gaps - c->height;
                resize(c, nx, ny, c->width, c->height, 1);
            } else {
                int dx = ev.xmotion.x - x;
                int dy = ev.xmotion.y - y;

                if (!dragging && (abs(dx) > snap_val || abs(dy) > snap_val))
                    dragging = 1;

                if (dragging && abs(dy) > snap_val) {
                    selmon->scroll_x = orig_scroll_x + dx;
                    clamp_scroll(selmon);
                    arrange(selmon);
                } else if (dragging) {
                    int col_w, inner;
                    Client *cl, *prev_t = NULL, *next_t = NULL;
                    int seen = 0;
                    get_col_metrics(selmon, &col_w, &inner);

                    int win_w = is_wide(c) ? (int)c->width : col_w;
                    int threshold = win_w / 4;

                    for (cl = selmon->clients; cl; cl = cl->next) {
                        if (!is_tiled(cl)) continue;
                        if (cl == c) { seen = 1; continue; }
                        if (!seen) prev_t = cl;
                        else { next_t = cl; break; }
                    }

                    if (dx > threshold && next_t) {
                        Client *t = next_t;
                        if (c->prev) c->prev->next = c->next;
                        else selmon->clients = c->next;
                        if (c->next) c->next->prev = c->prev;
                        else selmon->clients_tail = c->prev;
                        c->prev = t;
                        c->next = t->next;
                        if (t->next) t->next->prev = c;
                        else selmon->clients_tail = c;
                        t->next = c;
                        x += win_w + inner;
                        ocx = c->x;
                        arrange(selmon);
                    } else if (dx < -threshold && prev_t) {
                        Client *t = prev_t;
                        if (c->prev) c->prev->next = c->next;
                        else selmon->clients = c->next;
                        if (c->next) c->next->prev = c->prev;
                        else selmon->clients_tail = c->prev;
                        c->next = t;
                        c->prev = t->prev;
                        if (t->prev) t->prev->next = c;
                        else selmon->clients = c;
                        t->prev = c;
                        x -= win_w + inner;
                        ocx = c->x;
                        arrange(selmon);
                    }
                }
            }
            break;
        }
    } while (ev.type != ButtonRelease);
done_move:
    XUngrabPointer(dpy, CurrentTime);
}

void resizemouse(const char *arg) {
    (void)arg;
    int nw, nh;
    Client *c;
    XEvent ev;
    Time lasttime = 0;
    if (!(c = selmon->sel)) return;
    if (c->state == STATE_FULLSCREEN) return;
    
    restack(selmon);
    if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
        None, cursor_resize, CurrentTime) != GrabSuccess) return;
    XWarpPointer(dpy, None, c->window, 0, 0, 0, 0, c->width - 1, c->height - 1);
    do {
        XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask|StructureNotifyMask|SubstructureNotifyMask, &ev);
        switch (ev.type) {
        case DestroyNotify:
            if (ev.xdestroywindow.window == c->window || ev.xdestroywindow.window == c->frame)
                goto done_resize;
            handler[ev.type](&ev);
            break;
        case UnmapNotify:
            if (ev.xunmap.window == c->window || ev.xunmap.window == c->frame)
                goto done_resize;
            handler[ev.type](&ev);
            break;
        case ConfigureRequest:
        case Expose:
        case MapRequest:
            handler[ev.type](&ev);
            break;
        case MotionNotify:
            if ((ev.xmotion.time - lasttime) <= (1000 / 60)) continue;
            lasttime = ev.xmotion.time;

            {
                int th = c->frame ? TITLE_HEIGHT : 0;
                nw = ev.xmotion.x - c->x + 1;
                if (nw < 1) nw = 1;
                nh = ev.xmotion.y - (c->y + th) + 1;
                if (nh < 1) nh = 1;
            }

            if (c->state == STATE_FLOATING) {
                if (c->x + nw > selmon->wx + (int)selmon->ww - outer_gaps)
                    nw = selmon->wx + (int)selmon->ww - outer_gaps - c->x;
                if (c->y + nh > selmon->wy + (int)selmon->wh - outer_gaps)
                    nh = selmon->wy + (int)selmon->wh - outer_gaps - c->y;
                resize(c, c->x, c->y, nw, nh, 1);
            } else {
                c->custom_width = 1;
                c->width = nw;
                c->height = nh;
                clamp_scroll(selmon);
                arrange(selmon);
            }
            break;
        }
    } while (ev.type != ButtonRelease);
done_resize:
    XWarpPointer(dpy, None, c->window, 0, 0, 0, 0, c->width - 1, c->height - 1);
    XUngrabPointer(dpy, CurrentTime);
    while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
}

void spawn(const char *arg) {
    pid_t pid;
    if (!arg) return;
    pid = fork();
    if (pid < 0) return;
    if (pid == 0) {
        if (dpy) close(ConnectionNumber(dpy));
        setsid();
        signal(SIGCHLD, SIG_DFL);
        execlp("/bin/sh", "sh", "-c", arg, NULL);
        _exit(1);
    }
}

static void drawtitle(Client *c) {
    if (!c->frame || c->state == STATE_FULLSCREEN) return;
    int th = TITLE_HEIGHT;
    int fw = c->width;

    XSetForeground(dpy, gc, (c == selmon->sel) ? col_title_active_bg : col_title_inactive_bg);
    XFillRectangle(dpy, c->frame, gc, 0, 0, fw, th);

    if (show_title && c->title[0]) {
        XSetForeground(dpy, gc, (c == selmon->sel) ? col_title_active_fg : col_title_inactive_fg);
        int len = strlen(c->title);
        XDrawString(dpy, c->frame, gc, 4, th - 4, c->title, len > 60 ? 60 : len);
    }

    if (show_buttons) {
        int bx = fw - BUTTON_SIZE - BUTTON_MARGIN;
        int by = BUTTON_MARGIN;
        XSetForeground(dpy, gc, (c == selmon->sel) ? col_title_active_fg : col_title_inactive_fg);
        XDrawRectangle(dpy, c->frame, gc, bx, by, BUTTON_SIZE, BUTTON_SIZE);
        XDrawLine(dpy, c->frame, gc, bx+2, by+2, bx+BUTTON_SIZE-3, by+BUTTON_SIZE-3);
        XDrawLine(dpy, c->frame, gc, bx+2, by+BUTTON_SIZE-3, bx+BUTTON_SIZE-3, by+2);
        bx = fw - 2*BUTTON_SIZE - 2*BUTTON_MARGIN;
        XDrawRectangle(dpy, c->frame, gc, bx, by, BUTTON_SIZE, BUTTON_SIZE);
        XDrawRectangle(dpy, c->frame, gc, bx+2, by+2, BUTTON_SIZE-5, BUTTON_SIZE-5);
    }

    if (border_width > 0) {
        XSetForeground(dpy, gc, (c == selmon->sel) ? col_sel_outer_border : col_norm_outer_border);
        int frame_h = c->height + th;
        int i;
        for (i = 0; i < outer_border_width; i++)
            XDrawRectangle(dpy, c->frame, gc, i, i, fw - 2*i - 1, frame_h - 2*i - 1);
        XSetForeground(dpy, gc, (c == selmon->sel) ? col_sel_inner_border : col_norm_inner_border);
        for (i = outer_border_width; i < border_width; i++)
            XDrawRectangle(dpy, c->frame, gc, i, i, fw - 2*i - 1, frame_h - 2*i - 1);
    }
}

static void buttonpress(XEvent *e) {
    unsigned int i;
    Client *c;
    XButtonPressedEvent *ev = &e->xbutton;

    if ((c = wintoclient(ev->window))) {
        if (!c->ispanel) {
            focus(c);
            restack(selmon);
        }
    } else if ((c = frametoclient(ev->window))) {
        if (!c->ispanel) {
            focus(c);
            restack(selmon);
        }
        if (show_buttons) {
            int fx = ev->x, fy = ev->y;
            if (fy >= 0 && fy < TITLE_HEIGHT) {
                unsigned int fw = c->width;
                if (fx >= (int)(fw - BUTTON_SIZE - BUTTON_MARGIN) && fx < (int)(fw - BUTTON_MARGIN)) {
                    XAllowEvents(dpy, ReplayPointer, CurrentTime);
                    killclient(NULL);
                    return;
                } else if (fx >= (int)(fw - 2*BUTTON_SIZE - 2*BUTTON_MARGIN) && fx < (int)(fw - BUTTON_SIZE - 2*BUTTON_MARGIN)) {
                    XAllowEvents(dpy, ReplayPointer, CurrentTime);
                    togglemaximize(NULL);
                    return;
                }
            }
        }
    } else if (ev->window == root) {
        Window child;
        int rx, ry, cx, cy;
        unsigned int mask;
        if (XQueryPointer(dpy, root, &child, &child, &rx, &ry, &cx, &cy, &mask))
            if ((c = wintoclient(child))) {
                if (!c->ispanel) {
                    focus(c);
                    restack(selmon);
                }
            }
    }

    for (i = 0; i < (unsigned int)nbuttons; i++)
        if (buttons[i].func && buttons[i].button == ev->button
            && CLEANMASK(buttons[i].mod) == CLEANMASK(ev->state))
            buttons[i].func(buttons[i].arg);
    XAllowEvents(dpy, ReplayPointer, CurrentTime);
}

static void enternotify(XEvent *e) {
    XCrossingEvent *ev = &e->xcrossing;
    Client *c;
    if ((ev->mode != NotifyNormal || ev->detail == NotifyInferior) && ev->window != root)
        return;
    if ((c = wintoclient(ev->window))) {
        if (ISVISIBLE(c) && !c->ispanel) {
            focus(c);
            restack(selmon);
        }
    } else if ((c = frametoclient(ev->window))) {
        if (ISVISIBLE(c) && !c->ispanel) {
            focus(c);
            restack(selmon);
        }
    }
}

static void expose(XEvent *e) {
    XExposeEvent *ev = &e->xexpose;
    Client *c;
    if (ev->count == 0 && (c = frametoclient(ev->window)))
        drawtitle(c);
}

static void clientmessage(XEvent *e) {
    XClientMessageEvent *cme = &e->xclient;
    Client *c = wintoclient(cme->window);

    if (cme->message_type == netatom[NetCurrentDesktop]) {
        int ws = desktop_to_workspace((int)cme->data.l[0]);
        if (ws >= 0 && ws != selmon->workspace) {
            selmon->lastsel[selmon->workspace] = selmon->sel;
            selmon->workspace = ws;
            selmon->scroll_x = 0;
            if (selmon->lastsel[selmon->workspace]) {
                focus(selmon->lastsel[selmon->workspace]);
                ensure_visible(selmon->lastsel[selmon->workspace]);
            } else {
                focus(NULL);
            }
            restack(selmon);
            arrange(selmon);
            updateworkspaces();
        }
        return;
    }
    if (cme->message_type == netatom[NetWMDesktop] && c) {
        int ws = desktop_to_workspace((int)cme->data.l[0]);
        if (ws >= 0) {
            c->workspace = ws;
            focus(NULL);
            restack(selmon);
            arrange(selmon);
            updateworkspaces();
        }
        return;
    }
    if (cme->message_type == netatom[NetCloseWindow]) {
        closeclient(c);
        return;
    }
    if (!c) return;
    if (cme->message_type == wmatom[WMProtocols] && (Atom)cme->data.l[0] == wmatom[WMDelete]) {
        closeclient(c);
    } else if (cme->message_type == netatom[NetActiveWindow]) {
        if (c->monitor != selmon) selmon = c->monitor;
        if (!ISVISIBLE(c)) {
            selmon->lastsel[selmon->workspace] = selmon->sel;
            selmon->workspace = c->workspace;
            selmon->scroll_x = 0;
        }
        focus(c);
        ensure_visible(c);
        restack(selmon);
        arrange(selmon);
        updateworkspaces();
    } else if (cme->message_type == netatom[NetWMState]) {
        unsigned long action = cme->data.l[0];
        Atom prop1 = cme->data.l[1];
        Atom prop2 = cme->data.l[2];
        int target_fs = -1;
        int target_max = -1;
        if (prop1 == netatom[NetWMStateFullScreen] || prop2 == netatom[NetWMStateFullScreen]) {
            if (action == 0) target_fs = 0;
            else if (action == 1) target_fs = 1;
            else if (action == 2) target_fs = (c->state != STATE_FULLSCREEN);
            if (target_fs != -1) {
                setfullscreen(c, target_fs);
                restack(selmon);
                arrange(selmon);
            }
        }
        if (prop1 == netatom[NetWMStateMaximizedVert] || prop2 == netatom[NetWMStateMaximizedVert]
            || prop1 == netatom[NetWMStateMaximizedHorz] || prop2 == netatom[NetWMStateMaximizedHorz]) {
            if (action == 0) target_max = 0;
            else if (action == 1) target_max = 1;
            else if (action == 2) target_max = (c->state != STATE_MAXIMIZED);
            if (target_max != -1) {
                setmaximized(c, target_max);
                restack(selmon);
                arrange(selmon);
            }
        }
    }
}

static void configurenotify(XEvent *e) {
    XConfigureEvent *ev = &e->xconfigure;
    if (ev->window == root) {
        sw = ev->width;
        sh = ev->height;
        if (updategeom()) {
            XClearWindow(dpy, root);
            restack(selmon);
            arrange(selmon);
        }
    }
}

static void configurerequest(XEvent *e) {
    XConfigureRequestEvent *ev = &e->xconfigurerequest;
    XWindowChanges wc;
    Client *c = wintoclient(ev->window);
    if (c) {
        if (ev->value_mask & CWBorderWidth && !c->frame)
            c->border_width = ev->border_width > 0 ? ev->border_width : 0;
        if (c->state == STATE_FLOATING) {
            if (ev->value_mask & CWX) c->x = ev->x;
            if (ev->value_mask & CWY) c->y = ev->y;
            if (ev->value_mask & CWWidth) c->width = ev->width > 0 ? ev->width : 1;
            if (ev->value_mask & CWHeight) c->height = ev->height > 0 ? ev->height : 1;
            if (ISVISIBLE(c)) resize(c, c->x, c->y, c->width, c->height, 0);
        } else {
            configure(c);
        }
    } else {
        wc.x = ev->x; wc.y = ev->y; wc.width = ev->width; wc.height = ev->height;
        wc.border_width = ev->border_width;
        wc.sibling = ev->above; wc.stack_mode = ev->detail;
        XConfigureWindow(dpy, ev->window, ev->value_mask, &wc);
    }
}

static void destroynotify(XEvent *e) {
    XDestroyWindowEvent *ev = &e->xdestroywindow;
    Client *c = wintoclient(ev->window);
    if (c) unmanage(c, 1);
}

static void focusin(XEvent *e) {
    XFocusChangeEvent *ev = &e->xfocus;
    Client *c;
    if (!selmon->sel || ev->window == selmon->sel->window) return;
    if (ev->mode == NotifyGrab || ev->mode == NotifyUngrab) return;
    c = wintoclient(ev->window);
    if (c && (c->ispanel || c->is_transient)) return;
    if (!c && ev->window != root && !frametoclient(ev->window)) return;
    setfocus(selmon->sel);
}

static void keypress(XEvent *e) {
    unsigned int i;
    XKeyEvent *ev = &e->xkey;
    for (i = 0; i < (unsigned int)nkeys; i++)
        if (keycodes[i] == ev->keycode
            && CLEANMASK(keys[i].mod) == CLEANMASK(ev->state)
            && keys[i].func)
            keys[i].func(keys[i].arg);
}

static void mappingnotify(XEvent *e) {
    XMappingEvent *ev = &e->xmapping;
    XRefreshKeyboardMapping(ev);
    if (ev->request == MappingKeyboard || ev->request == MappingModifier) {
        if (ev->request == MappingModifier) updatenumlockmask();
        grabkeys();
    }
}

static void maprequest(XEvent *e) {
    XWindowAttributes wa;
    XMapRequestEvent *ev = &e->xmaprequest;
    if (!XGetWindowAttributes(dpy, ev->window, &wa)) return;
    if (wa.override_redirect) return;
    if (!wintoclient(ev->window)) manage(ev->window, &wa);
}

static void propertynotify(XEvent *e) {
    Client *c;
    Window trans = None;
    XPropertyEvent *ev = &e->xproperty;
    if (ev->state == PropertyDelete) return;
    if (ev->window == root) return;
    if (!(c = wintoclient(ev->window))) return;
    switch (ev->atom) {
    case XA_WM_TRANSIENT_FOR:
        XGetTransientForHint(dpy, c->window, &trans);
        if (c->state == STATE_NORMAL && trans != None) c->state = STATE_FLOATING;
        break;
    case XA_WM_NORMAL_HINTS:
        updatesizehints(c);
        break;
    case XA_WM_HINTS:
        updatewmhints(c);
        break;
    }
    if (ev->atom == XA_WM_NAME || ev->atom == netatom[NetWMName])
        updatetitle(c);
    if (ev->atom == netatom[NetWMWindowType] || ev->atom == netatom[NetWMState])
        updatewindowtype(c);
    if (ev->atom == netatom[NetWMStrut] || ev->atom == netatom[NetWMStrutPartial]) {
        updateworkarea();
        arrange(selmon);
    }
}

static void unmapnotify(XEvent *e) {
    XUnmapEvent *ev = &e->xunmap;
    Client *c = wintoclient(ev->window);
    if (!c) return;
    if (c->ignore_unmap > 0) {
        c->ignore_unmap--;
        return;
    }
    if (ev->send_event) setclientstate(c, WithdrawnState);
    else unmanage(c, 0);
}

static void checkotherwm(void) {
    xerrorxlib = XSetErrorHandler(xerrorstart);
    XSelectInput(dpy, DefaultRootWindow(dpy), SubstructureRedirectMask);
    XSync(dpy, False);
    XSetErrorHandler(xerror);
    XSync(dpy, False);
}

static int xerrorstart(Display *d, XErrorEvent *e) {
    (void)d;
    (void)e;
    die("mriya: another window manager is already running");
    return -1;
}

static int xerror(Display *d, XErrorEvent *e) {
    if (e->error_code == BadWindow || e->error_code == BadDrawable)
        return 0;
    if (e->error_code == BadMatch
        && (e->request_code == X_SetInputFocus
            || e->request_code == X_ConfigureWindow
            || e->request_code == X_ReparentWindow))
        return 0;
    if (e->error_code == BadAccess
        && (e->request_code == X_GrabKey || e->request_code == X_GrabButton)) {
#ifdef DEBUG
        fprintf(stderr, "mriya: grab failed: request=%d error=%d\n",
            e->request_code, e->error_code);
#endif
        return 0;
    }
#ifdef DEBUG
    fprintf(stderr, "mriya: X error: request=%d error=%d resource=0x%lx\n",
        e->request_code, e->error_code, e->resourceid);
#endif
    fprintf(stderr, "mriya: fatal error: request code=%d, error code=%d\n",
        e->request_code, e->error_code);
    return xerrorxlib(d, e);
}

static int xerrordummy(Display *d, XErrorEvent *e) {
    (void)d;
    (void)e;
    return 0;
}

static void initatoms(void) {
    wmatom[WMProtocols] = XInternAtom(dpy, "WM_PROTOCOLS", False);
    wmatom[WMDelete] = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    wmatom[WMState] = XInternAtom(dpy, "WM_STATE", False);
    wmatom[WMTakeFocus] = XInternAtom(dpy, "WM_TAKE_FOCUS", False);

    netatom[NetWMWindowType] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
    netatom[NetWMWindowTypeDock] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
    netatom[NetWMWindowTypeDialog] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
    netatom[NetWMWindowTypeToolbar] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_TOOLBAR", False);
    netatom[NetWMWindowTypeMenu] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_MENU", False);
    netatom[NetWMWindowTypeSplash] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_SPLASH", False);
    netatom[NetWMWindowTypeUtility] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_UTILITY", False);
    netatom[NetWMWindowTypeNotification] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_NOTIFICATION", False);
    netatom[NetWMState] = XInternAtom(dpy, "_NET_WM_STATE", False);
    netatom[NetWMStateFullScreen] = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
    netatom[NetWMStateMaximizedVert] = XInternAtom(dpy, "_NET_WM_STATE_MAXIMIZED_VERT", False);
    netatom[NetWMStateMaximizedHorz] = XInternAtom(dpy, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
    netatom[NetWMStateHidden] = XInternAtom(dpy, "_NET_WM_STATE_HIDDEN", False);
    netatom[NetWMStateAbove] = XInternAtom(dpy, "_NET_WM_STATE_ABOVE", False);
    netatom[NetWMStateDemandsAttention] = XInternAtom(dpy, "_NET_WM_STATE_DEMANDS_ATTENTION", False);
    netatom[NetWMStrutPartial] = XInternAtom(dpy, "_NET_WM_STRUT_PARTIAL", False);
    netatom[NetWMStrut] = XInternAtom(dpy, "_NET_WM_STRUT", False);
    netatom[NetActiveWindow] = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
    netatom[NetCloseWindow] = XInternAtom(dpy, "_NET_CLOSE_WINDOW", False);
    netatom[NetNumberOfDesktops] = XInternAtom(dpy, "_NET_NUMBER_OF_DESKTOPS", False);
    netatom[NetDesktopNames] = XInternAtom(dpy, "_NET_DESKTOP_NAMES", False);
    netatom[NetCurrentDesktop] = XInternAtom(dpy, "_NET_CURRENT_DESKTOP", False);
    netatom[NetWMDesktop] = XInternAtom(dpy, "_NET_WM_DESKTOP", False);
    netatom[NetClientList] = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
    netatom[NetClientListStacking] = XInternAtom(dpy, "_NET_CLIENT_LIST_STACKING", False);
    netatom[NetWMName] = XInternAtom(dpy, "_NET_WM_NAME", False);
    netatom[NetUTF8String] = XInternAtom(dpy, "UTF8_STRING", False);
    netatom[NetSupported] = XInternAtom(dpy, "_NET_SUPPORTED", False);
    netatom[NetWorkarea] = XInternAtom(dpy, "_NET_WORKAREA", False);
    netatom[NetDesktopViewport] = XInternAtom(dpy, "_NET_DESKTOP_VIEWPORT", False);
    net_supporting_wm_check = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
}

static unsigned long alloc_color(const char *name, unsigned long fallback) {
    XColor color;
    if (XAllocNamedColor(dpy, DefaultColormap(dpy, screen), name, &color, &color))
        return color.pixel;
    return fallback;
}

static void initcolors(void) {
    unsigned long black = BlackPixel(dpy, screen);
    unsigned long white = WhitePixel(dpy, screen);
    col_norm_outer_border = alloc_color(norm_outer_border, white);
    col_norm_inner_border = alloc_color(norm_inner_border, black);
    col_sel_outer_border = alloc_color(sel_outer_border, white);
    col_sel_inner_border = alloc_color(sel_inner_border, white);
    col_title_active_bg = alloc_color(title_active_bg, black);
    col_title_active_fg = alloc_color(title_active_fg, white);
    col_title_inactive_bg = alloc_color(title_inactive_bg, black);
    col_title_inactive_fg = alloc_color(title_inactive_fg, white);
}

static int ignorewindow(Window w) {
    XClassHint ch = { NULL, NULL };
    int ignore = 0;
    if (XGetClassHint(dpy, w, &ch)) {
        if (ch.res_name && strstr(ch.res_name, "sddm"))
            ignore = 1;
        else if (ch.res_class && strstr(ch.res_class, "sddm"))
            ignore = 1;
        if (ch.res_name) XFree(ch.res_name);
        if (ch.res_class) XFree(ch.res_class);
    }
    return ignore;
}

static void scan(void) {
    unsigned int i, num;
    Window d1, d2, *wins = NULL;
    XWindowAttributes wa;
    if (XQueryTree(dpy, root, &d1, &d2, &wins, &num)) {
        for (i = 0; i < num; i++) {
            if (!XGetWindowAttributes(dpy, wins[i], &wa) || wa.override_redirect
                || XGetTransientForHint(dpy, wins[i], &d1)) continue;
            if (ignorewindow(wins[i])) continue;
            if (wa.map_state == IsViewable || getstate(wins[i]) == IconicState)
                manage(wins[i], &wa);
        }
        for (i = 0; i < num; i++) {
            if (!XGetWindowAttributes(dpy, wins[i], &wa)) continue;
            if (ignorewindow(wins[i])) continue;
            if (XGetTransientForHint(dpy, wins[i], &d1)
                && (wa.map_state == IsViewable || getstate(wins[i]) == IconicState))
                manage(wins[i], &wa);
        }
        if (wins) XFree(wins);
    }
}

static void updateclientlist(void) {
    Client *c;
    Monitor *m;
    Window *list = NULL;
    Window *stack = NULL;
    int n = 0, ns = 0, i = 0;
    for (m = mons; m; m = m->next) {
        for (c = m->clients; c; c = c->next) n++;
        for (c = m->stack; c; c = c->next_stack) ns++;
    }
    if (n > 0) list = malloc(sizeof(Window) * n);
    if (ns > 0) stack = malloc(sizeof(Window) * ns);
    if (list)
        for (m = mons; m; m = m->next)
            for (c = m->clients; c; c = c->next)
                list[i++] = c->window;
    i = 0;
    if (stack)
        for (m = mons; m; m = m->next)
            for (c = m->stack; c; c = c->next_stack)
                stack[i++] = c->window;
    XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32,
        PropModeReplace, (unsigned char *)list, n);
    XChangeProperty(dpy, root, netatom[NetClientListStacking], XA_WINDOW, 32,
        PropModeReplace, (unsigned char *)stack, ns);
    free(list);
    free(stack);
}

static int workspace_has_clients(int ws) {
    Monitor *m;
    Client *c;
    for (m = mons; m; m = m->next)
        for (c = m->clients; c; c = c->next)
            if (c->workspace == ws) return 1;
    return 0;
}

static int desktop_to_workspace(int desktop) {
    int i, count = 0;
    if (desktop < 0) return -1;
    for (i = 0; i < MAX_WORKSPACES; i++) {
        if (workspace_has_clients(i) || i == selmon->workspace) {
            if (count == desktop) return i;
            count++;
        }
    }
    return -1;
}

static void updateworkspaces(void) {
    int i, pos = 0, count = 0;
    char names[256] = "";
    int ws_map[MAX_WORKSPACES];
    Monitor *m;
    Client *c;
    static int last_count = -1;
    static int last_current = -1;
    static char last_names[256] = "";
    int mapping_changed = 0;
    long workarea[4];

    for (i = 0; i < MAX_WORKSPACES; i++) {
        if (workspace_has_clients(i) || i == selmon->workspace) {
            ws_map[count] = i;
            names[pos++] = '0' + i;
            names[pos++] = '\0';
            count++;
        }
    }

    if (count != last_count || strcmp(names, last_names) != 0) {
        mapping_changed = 1;
        XChangeProperty(dpy, root, netatom[NetNumberOfDesktops], XA_CARDINAL, 32,
            PropModeReplace, (unsigned char *)&count, 1);
        XChangeProperty(dpy, root, netatom[NetDesktopNames], netatom[NetUTF8String], 8,
            PropModeReplace, (unsigned char *)names, pos);
        last_count = count;
        memcpy(last_names, names, sizeof(last_names));
    }

    if (selmon->workspace != last_current) {
        long cd = selmon->workspace;
        XChangeProperty(dpy, root, netatom[NetCurrentDesktop], XA_CARDINAL, 32,
            PropModeReplace, (unsigned char *)&cd, 1);
        last_current = selmon->workspace;
    }

    workarea[0] = selmon->wx;
    workarea[1] = selmon->wy;
    workarea[2] = selmon->ww;
    workarea[3] = selmon->wh;
    XChangeProperty(dpy, root, netatom[NetWorkarea], XA_CARDINAL, 32,
        PropModeReplace, (unsigned char *)workarea, 4);
    XChangeProperty(dpy, root, netatom[NetDesktopViewport], XA_CARDINAL, 32,
        PropModeReplace, (unsigned char *)workarea, 2);

    for (m = mons; m; m = m->next) {
        for (c = m->clients; c; c = c->next) {
            int desktop = -1;
            for (i = 0; i < count; i++)
                if (ws_map[i] == c->workspace) { desktop = i; break; }
            if (mapping_changed) c->published_ws = -1;
            if (desktop >= 0 && c->published_ws != desktop) {
                long d = desktop;
                XChangeProperty(dpy, c->window, netatom[NetWMDesktop], XA_CARDINAL, 32,
                    PropModeReplace, (unsigned char *)&d, 1);
                c->published_ws = desktop;
            }
        }
    }
}

static void load_default_config(void) {
    nkeys = 0;
    nbuttons = 0;
}

typedef struct {
    char norm_bg[64];
    char norm_outer_border[64];
    char norm_inner_border[64];
    char sel_bg[64];
    char sel_outer_border[64];
    char sel_inner_border[64];
    char urgent_color[64];
    char title_active_bg[64];
    char title_active_fg[64];
    char title_inactive_bg[64];
    char title_inactive_fg[64];
    char title_font[64];
    int inner_gaps, outer_gaps, snap_val;
    int outer_border_width, inner_border_width, border_width;
    int show_titlebar, show_title, show_buttons, insert_end, strip_align;
    int theme_mode;
    unsigned int default_modkey;
} ConfigSnapshot;

static void save_config(ConfigSnapshot *s) {
    memcpy(s->norm_bg, norm_bg, sizeof(s->norm_bg));
    memcpy(s->norm_outer_border, norm_outer_border, sizeof(s->norm_outer_border));
    memcpy(s->norm_inner_border, norm_inner_border, sizeof(s->norm_inner_border));
    memcpy(s->sel_bg, sel_bg, sizeof(s->sel_bg));
    memcpy(s->sel_outer_border, sel_outer_border, sizeof(s->sel_outer_border));
    memcpy(s->sel_inner_border, sel_inner_border, sizeof(s->sel_inner_border));
    memcpy(s->urgent_color, urgent_color, sizeof(s->urgent_color));
    memcpy(s->title_active_bg, title_active_bg, sizeof(s->title_active_bg));
    memcpy(s->title_active_fg, title_active_fg, sizeof(s->title_active_fg));
    memcpy(s->title_inactive_bg, title_inactive_bg, sizeof(s->title_inactive_bg));
    memcpy(s->title_inactive_fg, title_inactive_fg, sizeof(s->title_inactive_fg));
    memcpy(s->title_font, title_font, sizeof(s->title_font));
    s->inner_gaps = inner_gaps;
    s->outer_gaps = outer_gaps;
    s->snap_val = snap_val;
    s->outer_border_width = outer_border_width;
    s->inner_border_width = inner_border_width;
    s->border_width = border_width;
    s->show_titlebar = show_titlebar;
    s->show_title = show_title;
    s->show_buttons = show_buttons;
    s->insert_end = insert_end;
    s->strip_align = strip_align;
    s->theme_mode = theme_mode;
    s->default_modkey = default_modkey;
}

static void restore_config(const ConfigSnapshot *s) {
    memcpy(norm_bg, s->norm_bg, sizeof(s->norm_bg));
    memcpy(norm_outer_border, s->norm_outer_border, sizeof(s->norm_outer_border));
    memcpy(norm_inner_border, s->norm_inner_border, sizeof(s->norm_inner_border));
    memcpy(sel_bg, s->sel_bg, sizeof(s->sel_bg));
    memcpy(sel_outer_border, s->sel_outer_border, sizeof(s->sel_outer_border));
    memcpy(sel_inner_border, s->sel_inner_border, sizeof(s->sel_inner_border));
    memcpy(urgent_color, s->urgent_color, sizeof(s->urgent_color));
    memcpy(title_active_bg, s->title_active_bg, sizeof(s->title_active_bg));
    memcpy(title_active_fg, s->title_active_fg, sizeof(s->title_active_fg));
    memcpy(title_inactive_bg, s->title_inactive_bg, sizeof(s->title_inactive_bg));
    memcpy(title_inactive_fg, s->title_inactive_fg, sizeof(s->title_inactive_fg));
    memcpy(title_font, s->title_font, sizeof(s->title_font));
    inner_gaps = s->inner_gaps;
    outer_gaps = s->outer_gaps;
    snap_val = s->snap_val;
    outer_border_width = s->outer_border_width;
    inner_border_width = s->inner_border_width;
    border_width = s->border_width;
    show_titlebar = s->show_titlebar;
    show_title = s->show_title;
    show_buttons = s->show_buttons;
    insert_end = s->insert_end;
    strip_align = s->strip_align;
    theme_mode = s->theme_mode;
    default_modkey = s->default_modkey;
}

static void redraw_all_titles(void) {
    Monitor *m;
    Client *c;
    for (m = mons; m; m = m->next)
        for (c = m->clients; c; c = c->next)
            drawtitle(c);
}

void reload_config(const char *arg) {
    (void)arg;
    Key saved_keys[256];
    Button saved_buttons[256];
    ConfigSnapshot saved_config;
    int saved_nkeys = nkeys;
    int saved_nbuttons = nbuttons;
    save_config(&saved_config);
    memcpy(saved_keys, keys, sizeof(Key) * nkeys);
    memcpy(saved_buttons, buttons, sizeof(Button) * nbuttons);
    if (parse_config() != 0) {
        nkeys = saved_nkeys;
        memcpy(keys, saved_keys, sizeof(Key) * saved_nkeys);
        nbuttons = saved_nbuttons;
        memcpy(buttons, saved_buttons, sizeof(Button) * saved_nbuttons);
        restore_config(&saved_config);
    }
    initcolors();
    grabkeys();
    XUngrabButton(dpy, AnyButton, AnyModifier, root);
    {
        unsigned int i, j;
        unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
        for (i = 0; i < (unsigned int)nbuttons; i++)
            for (j = 0; j < LENGTH(modifiers); j++)
                XGrabButton(dpy, buttons[i].button,
                    buttons[i].mod | modifiers[j],
                    root, True, ButtonPressMask,
                    GrabModeAsync, GrabModeAsync, None, None);
    }
    {
        XFontStruct *nf = XLoadQueryFont(dpy, title_font);
        if (nf) {
            XFreeFont(dpy, font);
            font = nf;
            XSetFont(dpy, gc, font->fid);
        }
    }
    updateframes();
    redraw_all_titles();
    arrange(selmon);
}

static void setup(void) {
    XSetWindowAttributes wa;
    struct sigaction sa;
    if (pipe(rekey_pipe) == 0) {
        fcntl(rekey_pipe[0], F_SETFL, O_NONBLOCK);
        fcntl(rekey_pipe[1], F_SETFL, O_NONBLOCK);
    }
    sigaction(SIGCHLD, NULL, &sa);
    sa.sa_handler = sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);
    sa.sa_handler = sigrekey;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGUSR1, &sa, NULL);
    checkotherwm();
    screen = DefaultScreen(dpy);
    sw = DisplayWidth(dpy, screen);
    sh = DisplayHeight(dpy, screen);
    root = RootWindow(dpy, screen);
    initatoms();
    load_default_config();
    if (parse_config() != 0)
        load_default_config();
    initcolors();
    wa.override_redirect = True;
    font = XLoadQueryFont(dpy, title_font);
    if (!font) die("mriya: cannot load font");
    gc = XCreateGC(dpy, root, 0, NULL);
    XSetFont(dpy, gc, font->fid);
    cursor_normal = XCreateFontCursor(dpy, XC_left_ptr);
    cursor_move = XCreateFontCursor(dpy, XC_fleur);
    cursor_resize = XCreateFontCursor(dpy, XC_sizing);
    updategeom();
    wa.cursor = cursor_normal;
    wa.event_mask = SubstructureRedirectMask|SubstructureNotifyMask|ButtonPressMask|PointerMotionMask
        |EnterWindowMask|LeaveWindowMask|StructureNotifyMask|PropertyChangeMask;
    XChangeWindowAttributes(dpy, root, CWEventMask|CWCursor, &wa);
    XSelectInput(dpy, root, wa.event_mask);
    updatenumlockmask();
    grabkeys();
    {
        unsigned int i, j;
        unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
        for (i = 0; i < (unsigned int)nbuttons; i++)
            for (j = 0; j < LENGTH(modifiers); j++)
                XGrabButton(dpy, buttons[i].button,
                    buttons[i].mod | modifiers[j],
                    root, True, ButtonPressMask,
                    GrabModeAsync, GrabModeAsync, None, None);
    }
    Atom supported[] = {
        wmatom[WMProtocols], wmatom[WMDelete], wmatom[WMState], wmatom[WMTakeFocus],
        netatom[NetWMWindowType], netatom[NetWMWindowTypeDock], netatom[NetWMWindowTypeDialog],
        netatom[NetWMWindowTypeToolbar], netatom[NetWMWindowTypeMenu], netatom[NetWMWindowTypeSplash],
        netatom[NetWMWindowTypeUtility], netatom[NetWMWindowTypeNotification],
        netatom[NetWMState], netatom[NetWMStateFullScreen],
        netatom[NetWMStateMaximizedVert], netatom[NetWMStateMaximizedHorz],
        netatom[NetWMStrutPartial], netatom[NetWMStrut],
        netatom[NetActiveWindow], netatom[NetCloseWindow],
        netatom[NetNumberOfDesktops], netatom[NetDesktopNames], netatom[NetCurrentDesktop], netatom[NetWMDesktop],
        netatom[NetClientList], netatom[NetClientListStacking],
        netatom[NetWorkarea], netatom[NetDesktopViewport],
    };
    XChangeProperty(dpy, root, netatom[NetSupported], XA_ATOM, 32,
        PropModeReplace, (unsigned char *)supported, LENGTH(supported));
    wmcheckwin = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);
    XChangeProperty(dpy, wmcheckwin, net_supporting_wm_check, XA_WINDOW, 32,
        PropModeReplace, (unsigned char *)&wmcheckwin, 1);
    XChangeProperty(dpy, wmcheckwin, netatom[NetWMName], netatom[NetUTF8String], 8,
        PropModeReplace, (unsigned char *)"mriyawm", 7);
    XChangeProperty(dpy, root, net_supporting_wm_check, XA_WINDOW, 32,
        PropModeReplace, (unsigned char *)&wmcheckwin, 1);
    XDeleteProperty(dpy, root, netatom[NetClientList]);
    XDeleteProperty(dpy, root, netatom[NetClientListStacking]);
    scan();
    showhide(selmon->stack);
    XSync(dpy, False);
    updateworkarea();
    autostart();
    updateclientlist();
    updateworkspaces();
}

static void run(void) {
    XEvent ev;
    int xfd = ConnectionNumber(dpy);
    while (running) {
        fd_set fds;
        int maxfd = xfd;
        FD_ZERO(&fds);
        FD_SET(xfd, &fds);
        if (rekey_pipe[0] >= 0) {
            FD_SET(rekey_pipe[0], &fds);
            if (rekey_pipe[0] > maxfd) maxfd = rekey_pipe[0];
        }
        if (select(maxfd + 1, &fds, NULL, NULL, NULL) < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (rekey_pipe[0] >= 0 && FD_ISSET(rekey_pipe[0], &fds)) {
            char buf[64];
            while (read(rekey_pipe[0], buf, sizeof(buf)) > 0);
            need_rekey = 0;
            updatenumlockmask();
            grabkeys();
        }
        while (running && XPending(dpy)) {
            XNextEvent(dpy, &ev);
            if (need_rekey) {
                need_rekey = 0;
                updatenumlockmask();
                grabkeys();
            }
            if (handler[ev.type])
                handler[ev.type](&ev);
        }
    }
}

static void cleanup(void) {
    Monitor *m;
    Client *c, *tmp;
    int i;
    shutting_down = 1;
    for (m = mons; m; m = m->next) {
        c = m->clients;
        while (c) {
            tmp = c->next;
            unmanage(c, 0);
            c = tmp;
        }
    }
    while (mons) {
        m = mons;
        mons = mons->next;
        free(m);
    }
    for (i = 0; i < nautostart; i++)
        free(autostart_cmds[i]);
    nautostart = 0;
    if (rekey_pipe[0] >= 0) close(rekey_pipe[0]);
    if (rekey_pipe[1] >= 0) close(rekey_pipe[1]);
    XUngrabKey(dpy, AnyKey, AnyModifier, root);
    XFreeCursor(dpy, cursor_normal);
    XFreeCursor(dpy, cursor_move);
    XFreeCursor(dpy, cursor_resize);
    XFreeFont(dpy, font);
    XFreeGC(dpy, gc);
    XDestroyWindow(dpy, wmcheckwin);
    XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
    XCloseDisplay(dpy);
}

void quit(const char *arg) {
    (void)arg;
    running = 0;
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    if (!(dpy = XOpenDisplay(NULL))) die("mriya: cannot open display");
    setup();
    run();
    cleanup();
    return 0;
}
