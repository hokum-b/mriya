#define _POSIX_C_SOURCE 200809L
#include <X11/Xlib.h>
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
#include <errno.h>
#include <stdarg.h>

typedef enum { STATE_NORMAL, STATE_FLOATING, STATE_MAXIMIZED, STATE_FULLSCREEN } ClientState;

#define MAX_CLIENTS 256
#define MAX_WORKSPACES 10
#define SCROLL_STEP 300
#define BORDER_WIDTH 2

typedef struct Client {
    Window window;
    int x, y;
    unsigned int width, height;
    int orig_x, orig_y;
    unsigned int orig_width, orig_height;
    ClientState state;
    int workspace;
    struct Monitor *monitor;
    int mapped;
    int urgent;
    int ispanel;
    struct Client *next;
    struct Client *prev;
    struct Client *next_stack;
    struct Client *prev_stack;
    int border_width;
    int orig_border_width;
} Client;

typedef struct Monitor {
    int x, y;
    unsigned int width, height;
    int num;
    int bh;
    int bbh;
    Client *clients;
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

typedef struct {
    const char *symbol;
    void (*arrange)(Monitor *);
} Layout;

typedef struct {
    const char *class;
    const char *instance;
    const char *title;
    unsigned int tags;
    int isfloating;
    int monitor;
} Rule;

static void tile(Monitor *m);
static void monocle(Monitor *m);

#include "config.h"

#define LENGTH(X) (sizeof X / sizeof X[0])
#define BUTTONMASK (ButtonPressMask|ButtonReleaseMask)
#define CLEANMASK(mask) (mask & ~(numlockmask|LockMask) & (ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask))
#define ISVISIBLE(C) (C->workspace == selmon->workspace)

static Display *dpy;
static int screen;
static Window root;
static int sw, sh;
static Monitor *mons = NULL;
static Monitor *selmon = NULL;
static int running = 1;
static int restart = 0;
static Cursor cursor_normal;
static Cursor cursor_move;
static Cursor cursor_resize;
static int inner_gaps = INNER_GAP;
static int outer_gaps = OUTER_GAP;
static unsigned int numlockmask = 0;

static unsigned long col_norm_bg;
static unsigned long col_norm_border;
static unsigned long col_sel_bg;
static unsigned long col_sel_border;
static unsigned long col_urgent;

enum { WMProtocols, WMDelete, WMState, WMTakeFocus };
static Atom wmatom[4];

enum {
    NetWMWindowType, NetWMWindowTypeDock, NetWMWindowTypeDialog,
    NetWMWindowTypeToolbar, NetWMWindowTypeMenu, NetWMWindowTypeSplash,
    NetWMWindowTypeUtility, NetWMWindowTypeNotification,
    NetWMState, NetWMStateFullScreen,
    NetWMStrutPartial, NetWMStrut,
    NetActiveWindow,
    NetNumberOfDesktops, NetDesktopNames, NetCurrentDesktop, NetWMDesktop,
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
static void configurenotify(XEvent *e);
static void configurerequest(XEvent *e);
static Monitor *createmon(int num, int x, int y, int w, int h);
static void destroynotify(XEvent *e);
static void die(const char *fmt, ...);
static void drawbar(Monitor *m);
static void enternotify(XEvent *e);
static void focus(Client *c);
static void unfocus(Client *c, int setfocus);
static void focusin(XEvent *e);
static void focusmon(const char *arg);
static void ensure_visible(Client *c);

static void focusleft(const char *arg);
static void focusright(const char *arg);
static Atom getatomprop(Client *c, Atom prop);
static int getrootptr(int *x, int *y);
static long getstate(Window w);
static int gettextprop(Window w, Atom atom, char *text, unsigned int size);
static void grabbuttons(Client *c, int focused);
static void grabkeys(void);
static void initatoms(void);
static void initcolors(void);
static void buttonpress(XEvent *e);
static void keypress(XEvent *e);
static void killclient(const char *arg);
static void manage(Window w, XWindowAttributes *wa);
static void mappingnotify(XEvent *e);
static void maprequest(XEvent *e);
static void motionnotify(XEvent *e);
static void movemouse(const char *arg);
static void propertynotify(XEvent *e);
static void quit(const char *arg);
static void resize(Client *c, int x, int y, int w, int h, int interact);
static void configure(Client *c);
static void resizemouse(const char *arg);
static void restack(Monitor *m);
static void restartwm(const char *arg);
static void run(void);
static void scan(void);
static int ignorewindow(Window w);
static int sendevent(Client *c, Atom proto);
static void sendmon(Client *c, Monitor *m);
static void setclientstate(Client *c, long state);
static void setfocus(Client *c);
static void setfullscreen(Client *c, int fullscreen);
static void setup(void);
static void showhide(Client *c);
static void sigchld(int unused);
static void spawn(const char *arg);
static void tag(const char *arg);
static void tagmon(const char *arg);
static void togglefloating(const char *arg);
static void togglemaximize(const char *arg);
static void togglefullscreen(const char *arg);
static void toggletag(const char *arg);
static void toggleview(const char *arg);
static void unmanage(Client *c, int destroyed);
static void unmapnotify(XEvent *e);
static void updateclientlist(void);
static int updategeom(void);
static void updatenumlockmask(void);
static void updatesizehints(Client *c);
static void updatestatus(void);
static void updatetitle(Client *c);
static void updatewindowtype(Client *c);
static void updatewmhints(Client *c);
static void updateworkspaces(void);
static int workspace_has_clients(int ws);
static void view(const char *arg);
static Client *wintoclient(Window w);
static Monitor *wintomon(Window w);
static int xerror(Display *dpy, XErrorEvent *ee);
static int xerrordummy(Display *dpy, XErrorEvent *ee);
static int xerrorstart(Display *dpy, XErrorEvent *ee);
static void zoom(const char *arg);
static void scrollleft(const char *arg);
static void scrollright(const char *arg);
static void setgaps(const char *arg);
static void ws_up(const char *arg);
static void ws_down(const char *arg);
static void arrange(Monitor *m);
static int get_total_strip_width(Monitor *m);
static void attach(Client *c);
static void attachstack(Client *c);

static void (*handler[LASTEvent])(XEvent *) = {
    [ClientMessage] = clientmessage,
    [ConfigureNotify] = configurenotify,
    [ConfigureRequest] = configurerequest,
    [DestroyNotify] = destroynotify,
    [EnterNotify] = enternotify,
    [FocusIn] = focusin,
    [KeyPress] = keypress,
    [MappingNotify] = mappingnotify,
    [MapRequest] = maprequest,
    [MotionNotify] = motionnotify,
    [PropertyNotify] = propertynotify,
    [ButtonPress] = buttonpress,
    [UnmapNotify] = unmapnotify
};

static Key keys[] = {
    { MODKEY, XK_Return, spawn, TERM },
    { MODKEY|ShiftMask, XK_Return, spawn, TERM },
    { MODKEY, XK_d, spawn, DMENU },
    { MODKEY|ShiftMask, XK_q, killclient, NULL },
    { MODKEY|ShiftMask, XK_e, quit, NULL },
    { MODKEY|ShiftMask, XK_r, restartwm, NULL },
    { MODKEY, XK_h, focusleft, NULL },
    { MODKEY, XK_l, focusright, NULL },
    { MODKEY, XK_j, setgaps, "-2" },
    { MODKEY, XK_k, setgaps, "+2" },
    { MODKEY|ShiftMask, XK_j, setgaps, "0" },
    { MODKEY, XK_space, zoom, NULL },
    { MODKEY|ShiftMask, XK_space, togglefloating, NULL },
    { MODKEY, XK_f, togglemaximize, NULL },
    { MODKEY|ShiftMask, XK_f, togglefullscreen, NULL },
    { MODKEY, XK_Tab, view, NULL },
    { MODKEY, XK_Left, focusleft, NULL },
    { MODKEY, XK_Right, focusright, NULL },
    { MODKEY, XK_Up, ws_up, NULL },
    { MODKEY, XK_Down, ws_down, NULL },
    { 0, XF86XK_AudioMute, spawn, VOL_MUTE },
    { 0, XF86XK_AudioLowerVolume, spawn, VOL_DOWN },
    { 0, XF86XK_AudioRaiseVolume, spawn, VOL_UP },
    { 0, XF86XK_MonBrightnessDown, spawn, BRI_DOWN },
    { 0, XF86XK_MonBrightnessUp, spawn, BRI_UP },
    TAGKEYS(XK_1, 0)
    TAGKEYS(XK_2, 1)
    TAGKEYS(XK_3, 2)
    TAGKEYS(XK_4, 3)
    TAGKEYS(XK_5, 4)
    TAGKEYS(XK_6, 5)
    TAGKEYS(XK_7, 6)
    TAGKEYS(XK_8, 7)
    TAGKEYS(XK_9, 8)
};

static Button buttons[] = {
    { MODKEY, Button1, movemouse, NULL },
    { MODKEY, Button3, resizemouse, NULL },
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
    if (signal(SIGCHLD, sigchld) == SIG_ERR)
        die("can't install SIGCHLD handler:");
    while (0 < waitpid(-1, NULL, WNOHANG));
}

static void autostart(void) {
    int i;
    for (i = 0; i < AUTOSTART_LEN; i++) {
        if (fork() == 0) {
            setsid();
            execlp("/bin/sh", "sh", "-c", autostart_cmds[i], NULL);
            _exit(1);
        }
    }
}

static Monitor *createmon(int num, int x, int y, int w, int h) {
    Monitor *m = calloc(1, sizeof(Monitor));
    if (!m) die("calloc:");
    m->num = num;
    m->x = x;
    m->y = y;
    m->width = w;
    m->height = h;
    m->bh = 0;
    m->bbh = 0;
    m->scroll_x = 0;
    m->workspace = 0;
    m->clients = NULL;
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
        mons = createmon(0, 0, 0, sw, sh);
        selmon = mons;
        dirty = 1;
    } else if (mons->width != sw || mons->height != sh) {
        mons->width = sw;
        mons->height = sh;
        dirty = 1;
    }
    return dirty;
}

static Client *wintoclient(Window w) {
    Client *c;
    Monitor *m;
    for (m = mons; m; m = m->next)
        for (c = m->clients; c; c = c->next)
            if (c->window == w) return c;
    return NULL;
}

static Monitor *wintomon(Window w) {
    int x, y;
    Client *c;
    if (w == root && getrootptr(&x, &y)) return selmon;
    if ((c = wintoclient(w))) return c->monitor ? c->monitor : selmon;
    return selmon;
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
    if (!XGetWMNormalHints(dpy, c->window, &size, &msize))
        size.flags = PSize;
    if (size.flags & PBaseSize) {
        c->orig_width = size.base_width;
        c->orig_height = size.base_height;
    } else if (size.flags & PMinSize) {
        c->orig_width = size.min_width;
        c->orig_height = size.min_height;
    } else
        c->orig_width = c->orig_height = 0;
}

static void updatetitle(Client *c) {
    char name[256];
    gettextprop(c->window, XA_WM_NAME, name, sizeof(name));
}

static void updatewindowtype(Client *c) {
    Atom wtype = getatomprop(c, netatom[NetWMWindowType]);
    if (wtype == netatom[NetWMWindowTypeDock] ||
        wtype == netatom[NetWMWindowTypeToolbar] ||
        wtype == netatom[NetWMWindowTypeMenu] ||
        wtype == netatom[NetWMWindowTypeSplash] ||
        wtype == netatom[NetWMWindowTypeDialog] ||
        wtype == netatom[NetWMWindowTypeNotification] ||
        wtype == netatom[NetWMWindowTypeUtility]) {
        c->state = STATE_FLOATING;
        c->border_width = 0;
        c->ispanel = 1;
    }
    Atom state = getatomprop(c, netatom[NetWMState]);
    if (state == netatom[NetWMStateFullScreen])
        setfullscreen(c, 1);
}

static void updatewmhints(Client *c) {
    XWMHints *wmh;
    if ((wmh = XGetWMHints(dpy, c->window))) {
        if (c == selmon->sel && wmh->flags & XUrgencyHint) {
            wmh->flags &= ~XUrgencyHint;
            XSetWMHints(dpy, c->window, wmh);
        } else
            c->urgent = (wmh->flags & XUrgencyHint) ? 1 : 0;
        XFree(wmh);
    }
}

static Atom getatomprop(Client *c, Atom prop) {
    int di;
    unsigned long dl;
    unsigned char *p = NULL;
    Atom da, atom = None;
    if (XGetWindowProperty(dpy, c->window, prop, 0L, sizeof(atom), False, XA_ATOM,
        &da, &di, &dl, &dl, &p) == Success && p) {
        atom = *(Atom *)p;
        XFree(p);
    }
    return atom;
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
    if (n != 0) result = *p;
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
    if (c->state == STATE_FULLSCREEN) return;
    XSetInputFocus(dpy, c->window, RevertToPointerRoot, CurrentTime);
    XChangeProperty(dpy, root, XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False),
        XA_WINDOW, 32, PropModeReplace, (unsigned char *)&(c->window), 1);
    sendevent(c, wmatom[WMTakeFocus]);
}

static void focus(Client *c) {
    if (!c || !ISVISIBLE(c))
        for (c = selmon->stack; c && !ISVISIBLE(c); c = c->next_stack);
    if (selmon->sel && selmon->sel != c)
        unfocus(selmon->sel, 0);
    if (c) {
        if (c->monitor != selmon) selmon = c->monitor;
        if (c->state == STATE_FLOATING) XRaiseWindow(dpy, c->window);
        selmon->sel = c;
        selmon->lastsel[selmon->workspace] = c;
        XSetWindowBorder(dpy, c->window, col_sel_border);
        setfocus(c);
        drawbar(selmon);
    } else {
        selmon->sel = NULL;
        XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
        XDeleteProperty(dpy, root, XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False));
    }
}

static void unfocus(Client *c, int setfocus) {
    if (!c) return;
    grabbuttons(c, 0);
    XSetWindowBorder(dpy, c->window, col_norm_border);
    if (setfocus) {
        XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
        XDeleteProperty(dpy, root, XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False));
    }
}

static void grabbuttons(Client *c, int focused) {
    unsigned int i, j;
    unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
    XUngrabButton(dpy, AnyButton, AnyModifier, c->window);
    if (!focused) return;
    for (i = 0; i < LENGTH(buttons); i++)
        for (j = 0; j < LENGTH(modifiers); j++)
            XGrabButton(dpy, buttons[i].button,
                buttons[i].mod | modifiers[j],
                c->window, False, BUTTONMASK,
                GrabModeAsync, GrabModeSync, None, None);
}

static void updatenumlockmask(void) {
    unsigned int i, j;
    XModifierKeymap *modmap;
    numlockmask = 0;
    modmap = XGetModifierMapping(dpy);
    for (i = 0; i < 8; i++)
        for (j = 0; j < modmap->max_keypermod; j++)
            if (modmap->modifiermap[i * modmap->max_keypermod + j]
                == XKeysymToKeycode(dpy, XK_Num_Lock))
                numlockmask = (1 << i);
    XFreeModifiermap(modmap);
}

static void grabkeys(void) {
    unsigned int i, j;
    unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
    KeyCode code;
    XUngrabKey(dpy, AnyKey, AnyModifier, root);
    for (i = 0; i < LENGTH(keys); i++)
        if ((code = XKeysymToKeycode(dpy, keys[i].keysym)))
            for (j = 0; j < LENGTH(modifiers); j++)
                XGrabKey(dpy, code, keys[i].mod | modifiers[j], root,
                    True, GrabModeAsync, GrabModeAsync);
}

static int get_total_strip_width(Monitor *m) {
    Client *c;
    int total = 2 * outer_gaps;
    int usable = m->width - 2 * outer_gaps - 4 * BORDER_WIDTH;
    int col_w = (usable - inner_gaps) / 2;
    int actual_inner = usable - 2 * col_w;
    if (col_w < 200) { col_w = 200; actual_inner = inner_gaps; }

    for (c = m->clients; c; c = c->next) {
        if (!ISVISIBLE(c) || c->state == STATE_FLOATING || c->state == STATE_FULLSCREEN) continue;
        if (c->state == STATE_MAXIMIZED)
            total += c->width + 2 * c->border_width;
        else
            total += col_w + 2 * c->border_width;
        total += actual_inner;
    }
    if (total > 2 * outer_gaps)
        total -= actual_inner;
    return total;
}

static void arrange(Monitor *m) {
    Client *c;
    int total_width = 0;
    int mon_y = m->y;
    int mon_h = m->height;
    int usable = m->width - 2 * outer_gaps - 4 * BORDER_WIDTH;
    int col_w = (usable - inner_gaps) / 2;
    int actual_inner = usable - 2 * col_w;
    if (col_w < 200) { col_w = 200; actual_inner = inner_gaps; }

    int total = get_total_strip_width(m);
    if (total < m->width) {
#if STRIP_ALIGN == 0
        m->scroll_x = 0;
#elif STRIP_ALIGN == 1
        m->scroll_x = m->width - total;
#else
        m->scroll_x = (m->width - total) / 2;
#endif
    } else {
        int max_scroll = -(total - m->width);
        if (max_scroll > 0) max_scroll = 0;
        if (m->scroll_x > 0) m->scroll_x = 0;
        if (m->scroll_x < max_scroll) m->scroll_x = max_scroll;
    }

    for (c = m->clients; c; c = c->next) {
        if (c->ispanel) {
            XRaiseWindow(dpy, c->window);
            continue;
        }
        if (!ISVISIBLE(c)) {
            XMoveWindow(dpy, c->window, c->width * -2, c->y);
            continue;
        }
        if (c->state == STATE_FLOATING) {
            resize(c, c->x, c->y, c->width, c->height, 0);
            XSetWindowBorder(dpy, c->window, c == m->sel ? col_sel_border : col_norm_border);
            XRaiseWindow(dpy, c->window);
            continue;
        }
        if (c->state == STATE_FULLSCREEN) {
            resize(c, m->x, m->y, m->width, m->height, 0);
            continue;
        }
        c->x = m->x + outer_gaps + total_width + m->scroll_x;
        c->y = mon_y + outer_gaps;
        c->height = mon_h - 2 * outer_gaps - 2 * c->border_width;

        if (c->state == STATE_MAXIMIZED) {
            resize(c, c->x, c->y, c->width, c->height, 0);
            XSetWindowBorder(dpy, c->window, c == m->sel ? col_sel_border : col_norm_border);
            total_width += c->width + 2 * c->border_width + actual_inner;
            continue;
        }
        c->width = col_w;
        resize(c, c->x, c->y, c->width, c->height, 0);
        XSetWindowBorder(dpy, c->window, c == m->sel ? col_sel_border : col_norm_border);
        total_width += col_w + 2 * c->border_width + actual_inner;
    }
}

static void tile(Monitor *m) {
    arrange(m);
}

static void monocle(Monitor *m) {
    unsigned int n = 0;
    Client *c;
    int mon_y = m->y;
    int mon_h = m->height;
    for (c = m->clients; c; c = c->next)
        if (ISVISIBLE(c)) n++;
    if (n > 0)
        for (c = m->clients; c; c = c->next)
            if (ISVISIBLE(c))
                resize(c, m->x + outer_gaps, mon_y + outer_gaps, m->width - 2 * outer_gaps - 2 * c->border_width, mon_h - 2 * outer_gaps - 2 * c->border_width, 0);
}

static void restack(Monitor *m) {
    Client *c;
    XEvent ev;
    drawbar(m);
    if (!m->sel) {
        arrange(m);
        return;
    }
    for (c = m->stack; c; c = c->next_stack)
        if (c->ispanel)
            XRaiseWindow(dpy, c->window);
    for (c = m->stack; c; c = c->next_stack)
        if (c->state == STATE_FLOATING && ISVISIBLE(c) && c != m->sel)
            XRaiseWindow(dpy, c->window);
    if (m->sel->state == STATE_FLOATING)
        XRaiseWindow(dpy, m->sel->window);
    arrange(m);
    XSync(dpy, False);
    while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
}

static void resize(Client *c, int x, int y, int w, int h, int interact) {
    XWindowChanges wc;
    if (c->state == STATE_FULLSCREEN) return;
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    c->x = wc.x = x;
    c->y = wc.y = y;
    c->width = wc.width = w;
    c->height = wc.height = h;
    wc.border_width = c->border_width;
    XConfigureWindow(dpy, c->window, CWX|CWY|CWWidth|CWHeight|CWBorderWidth, &wc);
    configure(c);
    XSync(dpy, False);
}

static void configure(Client *c) {
    XConfigureEvent ce;
    ce.type = ConfigureNotify;
    ce.display = dpy;
    ce.event = c->window;
    ce.window = c->window;
    ce.x = c->x;
    ce.y = c->y;
    ce.width = c->width;
    ce.height = c->height;
    ce.border_width = c->border_width;
    ce.above = None;
    ce.override_redirect = False;
    XSendEvent(dpy, c->window, False, StructureNotifyMask, (XEvent *)&ce);
}

static void showhide(Client *c) {
    if (!c) return;
    if (ISVISIBLE(c)) {
        XMoveWindow(dpy, c->window, c->x, c->y);
        if ((!selmon->sel || selmon->sel != c) && c->state != STATE_FULLSCREEN)
            XSetWindowBorder(dpy, c->window, col_norm_border);
        showhide(c->next);
    } else {
        showhide(c->next);
        XMoveWindow(dpy, c->window, c->width * -2, c->y);
    }
}

static void scrollleft(const char *arg) {
    if (selmon->sel && selmon->sel->state == STATE_MAXIMIZED)
        togglemaximize(NULL);
    int total = get_total_strip_width(selmon);
    if (total <= selmon->width) return;
    selmon->scroll_x += SCROLL_STEP;
    if (selmon->scroll_x > 0) selmon->scroll_x = 0;
    arrange(selmon);
}

static void scrollright(const char *arg) {
    if (selmon->sel && selmon->sel->state == STATE_MAXIMIZED)
        togglemaximize(NULL);
    int total = get_total_strip_width(selmon);
    if (total <= selmon->width) return;
    int max_scroll = -(total - selmon->width);
    if (max_scroll > 0) max_scroll = 0;
    selmon->scroll_x -= SCROLL_STEP;
    if (selmon->scroll_x < max_scroll) selmon->scroll_x = max_scroll;
    arrange(selmon);
}

static void ws_up(const char *arg) {
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
        updateworkspaces();
    }
}

static void ws_down(const char *arg) {
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
        updateworkspaces();
    }
}

static void setgaps(const char *arg) {
    if (arg[0] == '0') { inner_gaps = INNER_GAP; outer_gaps = OUTER_GAP; }
    else if (arg[0] == '-') { inner_gaps -= 2; outer_gaps -= 2; }
    else if (arg[0] == '+') { inner_gaps += 2; outer_gaps += 2; }
    if (inner_gaps < 0) inner_gaps = 0;
    if (outer_gaps < 0) outer_gaps = 0;
    arrange(selmon);
}

static void ensure_visible(Client *c) {
    Monitor *m = selmon;
    Client *cl;
    int pos = 0, found = 0;
    int usable = m->width - 2 * outer_gaps - 4 * BORDER_WIDTH;
    int col_w = (usable - inner_gaps) / 2;
    int actual_inner = usable - 2 * col_w;
    if (col_w < 200) { col_w = 200; actual_inner = inner_gaps; }
    int win_w;

    if (!c || c->state == STATE_FLOATING || c->state == STATE_FULLSCREEN) return;

    for (cl = m->clients; cl; cl = cl->next) {
        if (!ISVISIBLE(cl) || cl->state == STATE_FLOATING || cl->state == STATE_FULLSCREEN) continue;
        if (cl == c) { found = 1; break; }
        if (cl->state == STATE_MAXIMIZED)
            pos += cl->width + 2 * cl->border_width + actual_inner;
        else
            pos += col_w + 2 * cl->border_width + actual_inner;
    }
    if (!found) return;

    if (c->state == STATE_MAXIMIZED) {
        int target_x = m->x + outer_gaps + m->scroll_x + pos;
        int target_right = target_x + c->width + 2 * c->border_width;
        int screen_right = m->x + m->width - outer_gaps;
        int screen_left = m->x + outer_gaps;

        if (target_x < screen_left)
            m->scroll_x += screen_left - target_x;
        else if (target_right > screen_right)
            m->scroll_x -= target_right - screen_right;
    } else {
        win_w = col_w + 2 * c->border_width;
        int win_left = m->x + outer_gaps + pos + m->scroll_x;
        int win_right = win_left + win_w;
        int bound_left = m->x + outer_gaps;
        int bound_right = m->x + m->width - outer_gaps;

        if (win_left < bound_left)
            m->scroll_x += bound_left - win_left;
        else if (win_right > bound_right)
            m->scroll_x -= win_right - bound_right;
    }

    int total = get_total_strip_width(m);
    if (total < m->width) {
#if STRIP_ALIGN == 0
        m->scroll_x = 0;
#elif STRIP_ALIGN == 1
        m->scroll_x = m->width - total;
#else
        m->scroll_x = (m->width - total) / 2;
#endif
    } else {
        int max_scroll = -(total - m->width);
        if (m->scroll_x > 0) m->scroll_x = 0;
        if (m->scroll_x < max_scroll) m->scroll_x = max_scroll;
    }
}

static void focusleft(const char *arg) {
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
    }
}

static void focusright(const char *arg) {
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
    }
}

static void attach(Client *c) {
    if (INSERT_END) {
        Client *last;
        for (last = c->monitor->clients; last && last->next; last = last->next);
        if (last) {
            last->next = c;
            c->prev = last;
        } else {
            c->monitor->clients = c;
            c->prev = NULL;
        }
    } else {
        c->next = c->monitor->clients;
        if (c->monitor->clients) c->monitor->clients->prev = c;
        c->monitor->clients = c;
        c->prev = NULL;
    }
}

static void attachstack(Client *c) {
    c->next_stack = c->monitor->stack;
    if (c->monitor->stack) c->monitor->stack->prev_stack = c;
    c->monitor->stack = c;
    c->prev_stack = NULL;
}

static void manage(Window w, XWindowAttributes *wa) {
    Client *c;
    Window trans = None;
    XWindowChanges wc;
    c = calloc(1, sizeof(Client));
    if (!c) die("calloc:");
    c->window = w;
    c->x = c->orig_x = wa->x;
    c->y = c->orig_y = wa->y;
    c->width = c->orig_width = wa->width;
    c->height = c->orig_height = wa->height;
    c->state = STATE_NORMAL;
    c->workspace = selmon->workspace;
    c->monitor = selmon;
    c->mapped = 1;
    c->border_width = BORDER_WIDTH;
    updatesizehints(c);
    updatetitle(c);
    updatewindowtype(c);
    updatewmhints(c);
    XSelectInput(dpy, w, EnterWindowMask|FocusChangeMask|PropertyChangeMask|StructureNotifyMask);
    grabbuttons(c, 0);
    if (XGetTransientForHint(dpy, w, &trans) && trans != None)
        c->state = STATE_FLOATING;
    wc.border_width = c->border_width;
    XConfigureWindow(dpy, w, CWBorderWidth, &wc);
    XSetWindowBorder(dpy, w, col_norm_border);
    attach(c);
    attachstack(c);
    XChangeProperty(dpy, root, XInternAtom(dpy, "_NET_CLIENT_LIST", False), XA_WINDOW, 32,
        PropModeAppend, (unsigned char *)&(c->window), 1);
    XChangeProperty(dpy, root, XInternAtom(dpy, "_NET_CLIENT_LIST_STACKING", False), XA_WINDOW, 32,
        PropModeAppend, (unsigned char *)&(c->window), 1);
    setclientstate(c, NormalState);
    if (c->state == STATE_FULLSCREEN) XRaiseWindow(dpy, c->window);
    if (!c->ispanel) {
        focus(c);
        ensure_visible(c);
    }
    restack(selmon);
    XMapWindow(dpy, c->window);
    updateworkspaces();
}

static void unmanage(Client *c, int destroyed) {
    Monitor *m = c->monitor;
    XWindowChanges wc;
    Client *nextfocus = NULL;

    if (c == m->sel) {
        if (c->prev && ISVISIBLE(c->prev))
            nextfocus = c->prev;
        else if (c->next && ISVISIBLE(c->next))
            nextfocus = c->next;
        else {
            Client *cl;
            for (cl = m->clients; cl && !ISVISIBLE(cl); cl = cl->next);
            if (cl) nextfocus = cl;
        }
        m->sel = NULL;
    } else {
        nextfocus = m->sel;
    }

    wc.border_width = c->border_width;
    if (c->prev) c->prev->next = c->next;
    if (c->next) c->next->prev = c->prev;
    if (c == m->clients) m->clients = c->next;
    if (c->next_stack) c->next_stack->prev_stack = c->prev_stack;
    if (c->prev_stack) c->prev_stack->next_stack = c->next_stack;
    if (c == m->stack) m->stack = c->next_stack;
    if (!destroyed) {
        XGrabServer(dpy);
        XSetErrorHandler(xerrordummy);
        XSelectInput(dpy, c->window, NoEventMask);
        XConfigureWindow(dpy, c->window, CWBorderWidth, &wc);
        XUngrabButton(dpy, AnyButton, AnyModifier, c->window);
        setclientstate(c, WithdrawnState);
        XSync(dpy, False);
        XSetErrorHandler(xerror);
        XUngrabServer(dpy);
    }
    free(c);
    focus(nextfocus);
    updateclientlist();
    restack(m);
    updateworkspaces();
}

static void killclient(const char *arg) {
    if (!selmon->sel) return;
    if (!sendevent(selmon->sel, wmatom[WMDelete])) {
        XGrabServer(dpy);
        XSetErrorHandler(xerrordummy);
        XSetCloseDownMode(dpy, DestroyAll);
        XKillClient(dpy, selmon->sel->window);
        XSync(dpy, False);
        XSetErrorHandler(xerror);
        XUngrabServer(dpy);
    }
}

static void setfullscreen(Client *c, int fullscreen) {
    if (fullscreen && c->state != STATE_FULLSCREEN) {
        XChangeProperty(dpy, c->window, wmatom[WMState], XA_ATOM, 32,
            PropModeReplace, (unsigned char *)&wmatom[WMState], 1);
        c->state = STATE_FULLSCREEN;
        c->orig_x = c->x;
        c->orig_y = c->y;
        c->orig_width = c->width;
        c->orig_height = c->height;
        c->orig_border_width = c->border_width;
        c->border_width = 0;
        resize(c, c->monitor->x, c->monitor->y, c->monitor->width, c->monitor->height, 0);
        XRaiseWindow(dpy, c->window);
    } else if (!fullscreen && c->state == STATE_FULLSCREEN) {
        XChangeProperty(dpy, c->window, wmatom[WMState], XA_ATOM, 32,
            PropModeReplace, (unsigned char *)0, 0);
        c->state = STATE_NORMAL;
        c->x = c->orig_x;
        c->y = c->orig_y;
        c->width = c->orig_width;
        c->height = c->orig_height;
        c->border_width = c->orig_border_width;
        resize(c, c->x, c->y, c->width, c->height, 0);
    }
}

static void togglefullscreen(const char *arg) {
    if (selmon->sel)
        setfullscreen(selmon->sel, selmon->sel->state != STATE_FULLSCREEN);
}

static void togglemaximize(const char *arg) {
    Client *c = selmon->sel;
    if (!c) return;
    if (c->state == STATE_MAXIMIZED) {
        c->state = STATE_NORMAL;
        c->width = c->orig_width;
        c->height = c->orig_height;
    } else {
        if (c->state == STATE_FULLSCREEN) setfullscreen(c, 0);
        c->orig_width = c->width;
        c->orig_height = c->height;
        c->state = STATE_MAXIMIZED;
        c->width = selmon->width - 2 * outer_gaps - 2 * BORDER_WIDTH;
    }
    focus(c);
    ensure_visible(c);
    restack(selmon);
}

static void togglefloating(const char *arg) {
    Client *c = selmon->sel;
    if (!c) return;
    if (c->state == STATE_FULLSCREEN) setfullscreen(c, 0);
    if (c->state == STATE_MAXIMIZED) {
        c->state = STATE_FLOATING;
        c->x = c->orig_x;
        c->y = c->orig_y;
        c->width = c->orig_width;
        c->height = c->orig_height;
        if (c->width < 100 || c->height < 100) {
            c->width = (selmon->width - 2 * outer_gaps - 2 * BORDER_WIDTH) / 2;
            c->height = (selmon->height - 2 * outer_gaps - 2 * BORDER_WIDTH) / 2;
            c->x = selmon->x + (selmon->width - c->width - 2 * BORDER_WIDTH) / 2;
            c->y = selmon->y + (selmon->height - c->height - 2 * BORDER_WIDTH) / 2;
        }
    } else if (c->state == STATE_FLOATING) {
        c->state = STATE_NORMAL;
    } else {
        c->orig_x = c->x;
        c->orig_y = c->y;
        c->orig_width = c->width;
        c->orig_height = c->height;
        c->state = STATE_FLOATING;
        c->width = (selmon->width - 2 * outer_gaps - 2 * BORDER_WIDTH) / 2;
        c->height = (selmon->height - 2 * outer_gaps - 2 * BORDER_WIDTH) / 2;
        c->x = selmon->x + (selmon->width - c->width - 2 * BORDER_WIDTH) / 2;
        c->y = selmon->y + (selmon->height - c->height - 2 * BORDER_WIDTH) / 2;
    }
    restack(selmon);
}

static void zoom(const char *arg) {
    Client *c = selmon->sel;
    if (!c || c->state != STATE_NORMAL) return;
    if (c != selmon->clients) {
        if (c->prev) c->prev->next = c->next;
        if (c->next) c->next->prev = c->prev;
        c->next = selmon->clients;
        c->prev = NULL;
        selmon->clients->prev = c;
        selmon->clients = c;
    }
    focus(c);
    restack(selmon);
}

static void tag(const char *arg) {
    int tag;
    if (!selmon->sel) return;
    tag = arg[0] - '0';
    if (tag >= 0 && tag < MAX_WORKSPACES)
        selmon->sel->workspace = tag;
    focus(NULL);
    restack(selmon);
    updateworkspaces();
}

static void toggletag(const char *arg) {
    int tag;
    if (!selmon->sel) return;
    tag = arg[0] - '0';
    if (tag >= 0 && tag < MAX_WORKSPACES) {
        if (selmon->sel->workspace == tag) selmon->sel->workspace = 0;
        else selmon->sel->workspace = tag;
    }
    focus(NULL);
    restack(selmon);
    updateworkspaces();
}

static void view(const char *arg) {
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
    updateworkspaces();
}

static void toggleview(const char *arg) {
    int tag;
    tag = arg[0] - '0';
    if (tag >= 0 && tag < MAX_WORKSPACES) {
        if (selmon->workspace == tag) selmon->workspace = 0;
        else selmon->workspace = tag;
    }
    focus(NULL);
    restack(selmon);
    updateworkspaces();
}

static void tagmon(const char *arg) {
    if (!selmon->sel || !mons->next) return;
    sendmon(selmon->sel, arg[0] == '-' ? selmon->prev : selmon->next);
}

static void sendmon(Client *c, Monitor *m) {
    if (c->monitor == m) return;
    if (c->prev) c->prev->next = c->next;
    if (c->next) c->next->prev = c->prev;
    if (c == c->monitor->clients) c->monitor->clients = c->next;
    if (c->next_stack) c->next_stack->prev_stack = c->prev_stack;
    if (c->prev_stack) c->prev_stack->next_stack = c->next_stack;
    if (c == c->monitor->stack) c->monitor->stack = c->next_stack;
    c->monitor = m;
    c->workspace = m->workspace;
    attach(c);
    attachstack(c);
    focus(NULL);
    restack(m);
    updateworkspaces();
}

static void movemouse(const char *arg) {
    int x, y, ocx, ocy, nx, ny;
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
    if (!getrootptr(&x, &y)) return;

    int orig_scroll_x = selmon->scroll_x;
    int dragging = 0;

    do {
        XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
        switch (ev.type) {
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
                if (abs(selmon->x + outer_gaps - nx) < SNAP) nx = selmon->x + outer_gaps;
                else if (abs((selmon->x + selmon->width - outer_gaps) - (nx + c->width + 2 * c->border_width)) < SNAP)
                    nx = selmon->x + selmon->width - outer_gaps - c->width - 2 * c->border_width;
                if (abs(selmon->y + outer_gaps - ny) < SNAP) ny = selmon->y + outer_gaps;
                else if (abs((selmon->y + selmon->height - outer_gaps) - (ny + c->height + 2 * c->border_width)) < SNAP)
                    ny = selmon->y + selmon->height - outer_gaps - c->height - 2 * c->border_width;
                resize(c, nx, ny, c->width, c->height, 1);
            } else {
                int dx = ev.xmotion.x - x;
                int dy = ev.xmotion.y - y;

                if (!dragging && (abs(dx) > SNAP || abs(dy) > SNAP))
                    dragging = 1;

                if (dragging && abs(dy) > SNAP) {
                    selmon->scroll_x = orig_scroll_x + dx;

                    int total = get_total_strip_width(selmon);
                    if (total < selmon->width) {
                        selmon->scroll_x = (selmon->width - total) / 2;
                    } else {
                        int max_scroll = -(total - selmon->width);
                        if (max_scroll > 0) max_scroll = 0;
                        if (selmon->scroll_x > 0) selmon->scroll_x = 0;
                        if (selmon->scroll_x < max_scroll) selmon->scroll_x = max_scroll;
                    }

                    arrange(selmon);
                } else if (dragging) {
                    int usable = selmon->width - 2 * outer_gaps - 4 * BORDER_WIDTH;
                    int col_w = (usable - inner_gaps) / 2;
                    int actual_inner = usable - 2 * col_w;
                    if (col_w < 200) { col_w = 200; actual_inner = inner_gaps; }

                    int win_w = (c->state == STATE_MAXIMIZED) ? c->width + 2 * c->border_width : col_w + 2 * c->border_width;
                    int threshold = win_w / 4;

                    if (dx > threshold && c->next && ISVISIBLE(c->next)) {
                        Client *t = c->next;
                        while (t && !ISVISIBLE(t)) t = t->next;
                        if (t) {
                            if (c->prev) c->prev->next = c->next;
                            if (c->next) c->next->prev = c->prev;
                            if (c == selmon->clients) selmon->clients = c->next;
                            c->next = t->next;
                            c->prev = t;
                            if (t->next) t->next->prev = c;
                            t->next = c;
                            x += win_w + actual_inner;
                            ocx = c->x;
                            arrange(selmon);
                        }
                    } else if (dx < -threshold && c->prev && ISVISIBLE(c->prev)) {
                        Client *t = c->prev;
                        while (t && !ISVISIBLE(t)) t = t->prev;
                        if (t) {
                            if (c->prev) c->prev->next = c->next;
                            if (c->next) c->next->prev = c->prev;
                            if (c == selmon->clients) selmon->clients = c->next;
                            c->prev = t->prev;
                            c->next = t;
                            if (t->prev) t->prev->next = c;
                            t->prev = c;
                            if (!c->prev) selmon->clients = c;
                            x -= win_w + actual_inner;
                            ocx = c->x;
                            arrange(selmon);
                        }
                    }
                }
            }
            break;
        }
    } while (ev.type != ButtonRelease);
    XUngrabPointer(dpy, CurrentTime);
}

static void resizemouse(const char *arg) {
    int nw, nh;
    Client *c;
    XEvent ev;
    Time lasttime = 0;
    if (!(c = selmon->sel)) return;
    if (c->state == STATE_FULLSCREEN) return;
    
    restack(selmon);
    if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
        None, cursor_resize, CurrentTime) != GrabSuccess) return;
    XWarpPointer(dpy, None, c->window, 0, 0, 0, 0, c->width + c->border_width - 1, c->height + c->border_width - 1);
    do {
        XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
        switch (ev.type) {
        case ConfigureRequest:
        case Expose:
        case MapRequest:
            handler[ev.type](&ev);
            break;
        case MotionNotify:
            if ((ev.xmotion.time - lasttime) <= (1000 / 60)) continue;
            lasttime = ev.xmotion.time;
            
            nw = ev.xmotion.x - c->x - 2 * c->border_width + 1;
            if (nw < 1) nw = 1;
            nh = ev.xmotion.y - c->y - 2 * c->border_width + 1;
            if (nh < 1) nh = 1;
            
            if (c->state == STATE_FLOATING) {
                if (c->x + nw + 2 * c->border_width > selmon->x + selmon->width - outer_gaps)
                    nw = selmon->x + selmon->width - outer_gaps - c->x - 2 * c->border_width;
                if (c->y + nh + 2 * c->border_width > selmon->y + selmon->height - outer_gaps)
                    nh = selmon->y + selmon->height - outer_gaps - c->y - 2 * c->border_width;
                resize(c, c->x, c->y, nw, nh, 1);
            } else {
                c->state = STATE_MAXIMIZED;
                c->width = nw;
                c->height = nh;
                
                int total = get_total_strip_width(selmon);
                if (total < selmon->width) {
                    selmon->scroll_x = (selmon->width - total) / 2;
                } else {
                    int max_scroll = -(total - selmon->width);
                    if (max_scroll > 0) max_scroll = 0;
                    if (selmon->scroll_x > 0) selmon->scroll_x = 0;
                    if (selmon->scroll_x < max_scroll) selmon->scroll_x = max_scroll;
                }
                
                arrange(selmon);
            }
            break;
        }
    } while (ev.type != ButtonRelease);
    XWarpPointer(dpy, None, c->window, 0, 0, 0, 0, c->width + c->border_width - 1, c->height + c->border_width - 1);
    XUngrabPointer(dpy, CurrentTime);
    while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
}

static void spawn(const char *arg) {
    if (fork() == 0) {
        if (dpy) close(ConnectionNumber(dpy));
        setsid();
        execlp("/bin/sh", "sh", "-c", arg, NULL);
        die("mriya: execvp %s", arg);
    }
}

static void drawbar(Monitor *m) {
    XClearWindow(dpy, root);
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

    for (i = 0; i < LENGTH(buttons); i++)
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
    if ((c = wintoclient(ev->window)) && ISVISIBLE(c) && !c->ispanel) {
        focus(c);
        restack(selmon);
    }
}

static void clientmessage(XEvent *e) {
    XClientMessageEvent *cme = &e->xclient;
    Client *c = wintoclient(cme->window);
    if (!c) return;
    if (cme->message_type == wmatom[WMProtocols] && cme->data.l[0] == wmatom[WMDelete])
        killclient(NULL);
    else if (cme->message_type == XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False)) {
        if (c != selmon->sel) focus(c);
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
        }
    }
}

static void configurerequest(XEvent *e) {
    XConfigureRequestEvent *ev = &e->xconfigurerequest;
    XWindowChanges wc;
    Client *c = wintoclient(ev->window);
    if (c) {
        if (ev->value_mask & CWBorderWidth)
            c->border_width = ev->border_width > 0 ? ev->border_width : 0;
        if (c->state == STATE_FLOATING) {
            if (ev->value_mask & CWX) c->x = ev->x;
            if (ev->value_mask & CWY) c->y = ev->y;
            if (ev->value_mask & CWWidth) c->width = ev->width > 0 ? ev->width : 1;
            if (ev->value_mask & CWHeight) c->height = ev->height > 0 ? ev->height : 1;
            if (ISVISIBLE(c)) XMoveResizeWindow(dpy, c->window, c->x, c->y, c->width, c->height);
        } else {
            configure(c);
        }
    } else {
        wc.x = ev->x; wc.y = ev->y; wc.width = ev->width; wc.height = ev->height;
        wc.border_width = ev->border_width; wc.sibling = ev->above; wc.stack_mode = ev->detail;
        XConfigureWindow(dpy, ev->window, ev->value_mask, &wc);
    }
    XSync(dpy, False);
}

static void destroynotify(XEvent *e) {
    XDestroyWindowEvent *ev = &e->xdestroywindow;
    Client *c = wintoclient(ev->window);
    if (c) unmanage(c, 1);
}

static void focusin(XEvent *e) {
    XFocusChangeEvent *ev = &e->xfocus;
    if (selmon->sel && ev->window != selmon->sel->window) setfocus(selmon->sel);
}

static void keypress(XEvent *e) {
    unsigned int i;
    KeySym keysym;
    XKeyEvent *ev = &e->xkey;
    keysym = XkbKeycodeToKeysym(dpy, ev->keycode, 0, 0);
    for (i = 0; i < LENGTH(keys); i++)
        if (keysym == keys[i].keysym && CLEANMASK(keys[i].mod) == CLEANMASK(ev->state) && keys[i].func)
            keys[i].func(keys[i].arg);
}

static void mappingnotify(XEvent *e) {
    XMappingEvent *ev = &e->xmapping;
    XRefreshKeyboardMapping(ev);
    if (ev->request == MappingKeyboard) grabkeys();
}

static void maprequest(XEvent *e) {
    static XWindowAttributes wa;
    XMapRequestEvent *ev = &e->xmaprequest;
    if (!XGetWindowAttributes(dpy, ev->window, &wa)) return;
    if (wa.override_redirect) return;
    if (!wintoclient(ev->window)) manage(ev->window, &wa);
}

static void motionnotify(XEvent *e) {
}

static void propertynotify(XEvent *e) {
    Client *c;
    Window trans;
    XPropertyEvent *ev = &e->xproperty;
    if ((ev->window == root) && (ev->atom == XA_WM_NAME)) updatestatus();
    else if (ev->state == PropertyDelete) return;
    else if ((c = wintoclient(ev->window))) {
        switch (ev->atom) {
        case XA_WM_TRANSIENT_FOR:
            XGetTransientForHint(dpy, c->window, &trans);
            if (c->state == STATE_NORMAL && trans) c->state = STATE_FLOATING;
            break;
        case XA_WM_NORMAL_HINTS:
            updatesizehints(c);
            break;
        case XA_WM_HINTS:
            updatewmhints(c);
            drawbar(selmon);
            break;
        }
        if (ev->atom == XA_WM_NAME || ev->atom == XInternAtom(dpy, "_NET_WM_NAME", False)) {
            updatetitle(c);
            drawbar(selmon);
        }
        if (ev->atom == netatom[NetWMWindowType])
            updatewindowtype(c);
    }
}

static void unmapnotify(XEvent *e) {
    XUnmapEvent *ev = &e->xunmap;
    Client *c = wintoclient(ev->window);
    if (!c) return;
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

static int xerrorstart(Display *dpy, XErrorEvent *ee) {
    die("mriya: another window manager is already running");
    return -1;
}

static int xerror(Display *dpy, XErrorEvent *ee) {
    if (ee->error_code == BadWindow || ee->error_code == BadMatch
        || ee->error_code == BadDrawable || ee->error_code == BadAccess
        || ee->error_code == BadValue || ee->error_code == BadAtom)
        return 0;
    fprintf(stderr, "mriya: fatal error: request code=%d, error code=%d\n",
        ee->request_code, ee->error_code);
    return xerrorxlib(dpy, ee);
}

static int xerrordummy(Display *dpy, XErrorEvent *ee) {
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
    netatom[NetWMStrutPartial] = XInternAtom(dpy, "_NET_WM_STRUT_PARTIAL", False);
    netatom[NetWMStrut] = XInternAtom(dpy, "_NET_WM_STRUT", False);
    netatom[NetActiveWindow] = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
    netatom[NetNumberOfDesktops] = XInternAtom(dpy, "_NET_NUMBER_OF_DESKTOPS", False);
    netatom[NetDesktopNames] = XInternAtom(dpy, "_NET_DESKTOP_NAMES", False);
    netatom[NetCurrentDesktop] = XInternAtom(dpy, "_NET_CURRENT_DESKTOP", False);
    netatom[NetWMDesktop] = XInternAtom(dpy, "_NET_WM_DESKTOP", False);
    net_supporting_wm_check = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
}

static void initcolors(void) {
    XColor color;
    Colormap cmap = DefaultColormap(dpy, screen);
    XAllocNamedColor(dpy, cmap, NORM_BG, &color, &color);
    col_norm_bg = color.pixel;
    XAllocNamedColor(dpy, cmap, NORM_BORDER, &color, &color);
    col_norm_border = color.pixel;
    XAllocNamedColor(dpy, cmap, SEL_BG, &color, &color);
    col_sel_bg = color.pixel;
    XAllocNamedColor(dpy, cmap, SEL_BORDER, &color, &color);
    col_sel_border = color.pixel;
    XAllocNamedColor(dpy, cmap, URGENT_COLOR, &color, &color);
    col_urgent = color.pixel;
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
    XDeleteProperty(dpy, root, XInternAtom(dpy, "_NET_CLIENT_LIST", False));
    for (m = mons; m; m = m->next)
        for (c = m->clients; c; c = c->next)
            XChangeProperty(dpy, root, XInternAtom(dpy, "_NET_CLIENT_LIST", False),
                XA_WINDOW, 32, PropModeAppend, (unsigned char *)&(c->window), 1);
}

static void updatestatus(void) {
}

static int workspace_has_clients(int ws) {
    Monitor *m;
    Client *c;
    for (m = mons; m; m = m->next)
        for (c = m->clients; c; c = c->next)
            if (c->workspace == ws) return 1;
    return 0;
}

static void updateworkspaces(void) {
    int i, pos = 0, count = 0;
    char names[256] = "";
    int ws_map[MAX_WORKSPACES];
    Monitor *m;
    Client *c;

    for (i = 0; i < MAX_WORKSPACES; i++) {
        if (workspace_has_clients(i) || i == selmon->workspace) {
            ws_map[count] = i;
            names[pos++] = '0' + i;
            names[pos++] = '\0';
            count++;
        }
    }

    XChangeProperty(dpy, root, netatom[NetNumberOfDesktops], XA_CARDINAL, 32,
        PropModeReplace, (unsigned char *)&count, 1);
    XChangeProperty(dpy, root, netatom[NetDesktopNames], XInternAtom(dpy, "UTF8_STRING", False), 8,
        PropModeReplace, (unsigned char *)names, pos);

    for (i = 0; i < count; i++) {
        if (ws_map[i] == selmon->workspace) {
            long cd = i;
            XChangeProperty(dpy, root, netatom[NetCurrentDesktop], XA_CARDINAL, 32,
                PropModeReplace, (unsigned char *)&cd, 1);
            break;
        }
    }

    for (i = 0; i < count; i++) {
        for (m = mons; m; m = m->next)
            for (c = m->clients; c; c = c->next)
                if (c->workspace == ws_map[i]) {
                    long desktop = i;
                    XChangeProperty(dpy, c->window, netatom[NetWMDesktop], XA_CARDINAL, 32,
                        PropModeReplace, (unsigned char *)&desktop, 1);
                }
    }
}

static void setup(void) {
    XSetWindowAttributes wa;
    sigchld(0);
    checkotherwm();
    screen = DefaultScreen(dpy);
    sw = DisplayWidth(dpy, screen);
    sh = DisplayHeight(dpy, screen);
    root = RootWindow(dpy, screen);
    initatoms();
    initcolors();
    cursor_normal = XCreateFontCursor(dpy, XC_left_ptr);
    cursor_move = XCreateFontCursor(dpy, XC_fleur);
    cursor_resize = XCreateFontCursor(dpy, XC_sizing);
    updategeom();
    wa.cursor = cursor_normal;
    wa.event_mask = SubstructureRedirectMask|SubstructureNotifyMask|ButtonPressMask|PointerMotionMask
        |EnterWindowMask|LeaveWindowMask|StructureNotifyMask|PropertyChangeMask;
    XChangeWindowAttributes(dpy, root, CWEventMask|CWCursor, &wa);
    XSelectInput(dpy, root, wa.event_mask);
    grabkeys();
    {
        unsigned int i, j;
        unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
        for (i = 0; i < LENGTH(buttons); i++)
            for (j = 0; j < LENGTH(modifiers); j++)
                XGrabButton(dpy, buttons[i].button,
                    buttons[i].mod | modifiers[j],
                    root, True, ButtonPressMask,
                    GrabModeAsync, GrabModeSync, None, None);
    }
    Atom supported[] = {
        wmatom[WMProtocols], wmatom[WMDelete], wmatom[WMState], wmatom[WMTakeFocus],
        netatom[NetWMWindowType], netatom[NetWMWindowTypeDock], netatom[NetWMWindowTypeDialog],
        netatom[NetWMWindowTypeToolbar], netatom[NetWMWindowTypeMenu], netatom[NetWMWindowTypeSplash],
        netatom[NetWMWindowTypeUtility], netatom[NetWMWindowTypeNotification],
        netatom[NetWMState], netatom[NetWMStateFullScreen],
        netatom[NetWMStrutPartial], netatom[NetWMStrut],
        netatom[NetActiveWindow],
        netatom[NetNumberOfDesktops], netatom[NetDesktopNames], netatom[NetCurrentDesktop], netatom[NetWMDesktop],
    };
    XChangeProperty(dpy, root, XInternAtom(dpy, "_NET_SUPPORTED", False), XA_ATOM, 32,
        PropModeReplace, (unsigned char *)supported, LENGTH(supported));
    wmcheckwin = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);
    XChangeProperty(dpy, wmcheckwin, net_supporting_wm_check, XA_WINDOW, 32,
        PropModeReplace, (unsigned char *)&wmcheckwin, 1);
    XChangeProperty(dpy, wmcheckwin, XInternAtom(dpy, "_NET_WM_NAME", False), XInternAtom(dpy, "UTF8_STRING", False), 8,
        PropModeReplace, (unsigned char *)"mriyawm", 7);
    XChangeProperty(dpy, root, net_supporting_wm_check, XA_WINDOW, 32,
        PropModeReplace, (unsigned char *)&wmcheckwin, 1);
    XDeleteProperty(dpy, root, XInternAtom(dpy, "_NET_CLIENT_LIST", False));
    XDeleteProperty(dpy, root, XInternAtom(dpy, "_NET_CLIENT_LIST_STACKING", False));
    scan();
    autostart();
    updateworkspaces();
}

static void run(void) {
    XEvent ev;
    while (running && !XNextEvent(dpy, &ev))
        if (handler[ev.type])
            handler[ev.type](&ev);
}

static void cleanup(void) {
    Monitor *m;
    while (mons) {
        m = mons;
        mons = mons->next;
        free(m);
    }
    XUngrabKey(dpy, AnyKey, AnyModifier, root);
    XFreeCursor(dpy, cursor_normal);
    XFreeCursor(dpy, cursor_move);
    XFreeCursor(dpy, cursor_resize);
    XDestroyWindow(dpy, wmcheckwin);
    XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
    XCloseDisplay(dpy);
}

static void quit(const char *arg) {
    running = 0;
}

static void restartwm(const char *arg) {
    restart = 1;
    running = 0;
}

static void focusmon(const char *arg) {
    Monitor *m;
    if (!mons->next) return;
    m = arg[0] == '-' ? selmon->prev : selmon->next;
    if (!m) return;
    selmon = m;
    focus(NULL);
}

int main(int argc, char *argv[]) {
    if (!(dpy = XOpenDisplay(NULL))) die("mriya: cannot open display");
    setup();
    run();
    cleanup();
    if (restart) {
        execlp(argv[0], argv[0], NULL);
        die("mriya: failed to restart");
    }
    return 0;
}
