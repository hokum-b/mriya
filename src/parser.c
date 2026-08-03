#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/stat.h>

#include <X11/keysym.h>
#include <X11/Xlib.h>

#include "parser.h"

extern char norm_bg[64];
extern char norm_outer_border[64];
extern char norm_inner_border[64];
extern char sel_bg[64];
extern char sel_outer_border[64];
extern char sel_inner_border[64];
extern char urgent_color[64];
extern char title_active_bg[64];
extern char title_active_fg[64];
extern char title_inactive_bg[64];
extern char title_inactive_fg[64];
extern char title_font[64];

extern int inner_gaps;
extern int outer_gaps;
extern int snap_val;
extern int outer_border_width;
extern int inner_border_width;
extern int border_width;
extern int show_titlebar;
extern int show_title;
extern int show_buttons;
extern int insert_end;
extern int strip_align;
extern int show_move_indicator;
extern char move_indicator_color[64];
extern unsigned int default_modkey;

typedef struct {
    unsigned int mod;
    KeySym keysym;
    void (*func)(const char *arg);
    const char *arg;
} Key;

extern Key keys[256];
extern int nkeys;

typedef struct {
    unsigned int mod;
    unsigned int button;
    void (*func)(const char *arg);
    const char *arg;
} Button;

extern Button buttons[256];
extern int nbuttons;

extern char *autostart_cmds[256];
extern int nautostart;

extern void killclient(const char *arg);
extern void quit(const char *arg);
extern void restartwm(const char *arg);
extern void reload_config(const char *arg);
extern void focusleft(const char *arg);
extern void focusright(const char *arg);
extern void ws_up(const char *arg);
extern void ws_down(const char *arg);
extern void setgaps(const char *arg);
extern void zoom(const char *arg);
extern void togglefloating(const char *arg);
extern void togglemaximize(const char *arg);
extern void togglefullscreen(const char *arg);
extern void view(const char *arg);
extern void toggleview(const char *arg);
extern void tag(const char *arg);
extern void toggletag(const char *arg);
extern void spawn(const char *arg);
extern void movemouse(const char *arg);
extern void resizemouse(const char *arg);

typedef struct {
    const char *name;
    void (*func)(const char *arg);
    const char *arg;
} FuncMap;

static const FuncMap func_table[] = {
    {"close_window", killclient, NULL},
    {"killclient", killclient, NULL},
    {"quit", quit, NULL},
    {"restartwm", restartwm, NULL},
    {"restart", restartwm, NULL},
    {"reload_config", reload_config, NULL},
    {"reload", reload_config, NULL},
    {"focusleft", focusleft, NULL},
    {"focus_left", focusleft, NULL},
    {"focusright", focusright, NULL},
    {"focus_right", focusright, NULL},
    {"ws_up", ws_up, NULL},
    {"ws_down", ws_down, NULL},
    {"inc_gaps", setgaps, "+2"},
    {"increase_gaps", setgaps, "+2"},
    {"dec_gaps", setgaps, "-2"},
    {"decrease_gaps", setgaps, "-2"},
    {"zero_gaps", setgaps, "0"},
    {"reset_gaps", setgaps, "0"},
    {"setgaps", setgaps, NULL},
    {"zoom", zoom, NULL},
    {"togglefloating", togglefloating, NULL},
    {"toggle_floating", togglefloating, NULL},
    {"togglemaximize", togglemaximize, NULL},
    {"toggle_maximize", togglemaximize, NULL},
    {"togglefullscreen", togglefullscreen, NULL},
    {"toggle_fullscreen", togglefullscreen, NULL},
    {"fullscreen", togglefullscreen, NULL},
    {"view", view, NULL},
    {"toggleview", toggleview, NULL},
    {"tag", tag, NULL},
    {"toggletag", toggletag, NULL},
    {"movemouse", movemouse, NULL},
    {"mousemove", movemouse, NULL},
    {"resizemouse", resizemouse, NULL},
    {"mouseresize", resizemouse, NULL},
    {NULL, NULL, NULL}
};

static const char *ws_args[9] = {"0", "1", "2", "3", "4", "5", "6", "7", "8"};

static const char default_config_content[] =
"norm_bg : #222222\n"
"norm_outer_border : #ede5d4\n"
"norm_inner_border : #111111\n"
"sel_bg : #111111\n"
"sel_outer_border : #ede5d4\n"
"sel_inner_border : #ffffff\n"
"urgent_color : #ede5d4\n"
"title_active_bg : #111111\n"
"title_active_fg : #ffffff\n"
"title_inactive_bg : #222222\n"
"title_inactive_fg : #ede5d4\n"
"title_font : fixed\n"
"\n"
"inner_gap : 30\n"
"outer_gap : 50\n"
"snap : 32\n"
"outer_border_width : 0\n"
"inner_border_width : 0\n"
"\n"
"show_titlebar : 1\n"
"show_title : 1\n"
"show_buttons : 1\n"
"\n"
"mod_key : super\n"
"\n"
"autostart : \"alacritty\"\n"
"autostart : \"dmenu_run\"\n"
"\n"
"bind : mod + Shift + Return : \"alacritty\"\n"
"bind : mod + d : \"dmenu_run\"\n"
"bind : mod + b : \"firefox\"\n"
"\n"
"call : mod + w : close_window\n"
"call : mod + Shift + q : close_window\n"
"call : mod + Shift + e : quit\n"
"call : mod + Shift + r : restartwm\n"
"call : mod + r : reload_config\n"
"\n"
"call : mod + h : focusleft\n"
"call : mod + j : focusright\n"
"call : mod + k : focusleft\n"
"call : mod + l : focusright\n"
"call : mod + Left : focusleft\n"
"call : mod + Right : focusright\n"
"\n"
"call : mod + Up : ws_up\n"
"call : mod + Down : ws_down\n"
"\n"
"call : mod + Shift + j : dec_gaps\n"
"call : mod + Shift + k : inc_gaps\n"
"call : mod + Control + j : zero_gaps\n"
"\n"
"call : mod + space : zoom\n"
"call : mod + Shift + space : togglefloating\n"
"call : mod + f : togglemaximize\n"
"call : mod + Shift + f : togglefullscreen\n"
"call : mod + Tab : view\n"
"\n"
"mousebind : mod + Button1 : movemouse\n"
"mousebind : mod + Button3 : resizemouse\n"
"\n"
"workspace : mod + 1 : view 1\n"
"workspace : mod + 2 : view 2\n"
"workspace : mod + 3 : view 3\n"
"workspace : mod + 4 : view 4\n"
"workspace : mod + 5 : view 5\n"
"workspace : mod + 6 : view 6\n"
"workspace : mod + 7 : view 7\n"
"workspace : mod + 8 : view 8\n"
"workspace : mod + 9 : view 9\n"
"\n"
"workspace : mod + Shift + 1 : tag 1\n"
"workspace : mod + Shift + 2 : tag 2\n"
"workspace : mod + Shift + 3 : tag 3\n"
"workspace : mod + Shift + 4 : tag 4\n"
"workspace : mod + Shift + 5 : tag 5\n"
"workspace : mod + Shift + 6 : tag 6\n"
"workspace : mod + Shift + 7 : tag 7\n"
"workspace : mod + Shift + 8 : tag 8\n"
"workspace : mod + Shift + 9 : tag 9\n";

static void trim(char *buf) {
    char *start = buf;
    char *end;
    while (*start && isspace((unsigned char)*start)) start++;
    if (start != buf) memmove(buf, start, strlen(start) + 1);
    end = buf + strlen(buf) - 1;
    while (end > buf && isspace((unsigned char)*end)) *end-- = '\0';
}

static void eat_comment(char *buf) {
    while (*buf && isspace((unsigned char)*buf)) buf++;
    if (*buf == '#') *buf = '\0';
}

static void unquote(char *buf) {
    size_t len = strlen(buf);
    if (len > 0 && (buf[0] == '"' || buf[0] == '\'')) {
        memmove(buf, buf + 1, len);
        len--;
    }
    if (len > 0 && (buf[len - 1] == '"' || buf[len - 1] == '\''))
        buf[len - 1] = '\0';
}

static void make_parent_dir(const char *path) {
    char dir[4096];
    char *slash;
    snprintf(dir, sizeof(dir), "%s", path);
    slash = strrchr(dir, '/');
    if (!slash) return;
    *slash = '\0';
    if (*dir) {
        for (char *p = dir + 1; *p; p++) {
            if (*p == '/') {
                *p = '\0';
                mkdir(dir, 0755);
                *p = '/';
            }
        }
        mkdir(dir, 0755);
    }
}

static void write_default_config(const char *path) {
    FILE *f = fopen(path, "w");
    if (f) {
        fputs(default_config_content, f);
        fclose(f);
    }
}

static void seed_file(const char *path) {
    if (access(path, F_OK) != 0) {
        make_parent_dir(path);
        write_default_config(path);
    }
}

void ensure_config_files_exist(void) {
    const char *home = getenv("HOME");
    const char *xdg = getenv("XDG_CONFIG_HOME");
    char path[4096];

    seed_file("/etc/mriya/mriya.rc");

    if (xdg && *xdg) {
        snprintf(path, sizeof(path), "%s/mriya/mriya.rc", xdg);
        seed_file(path);
    } else if (home && *home) {
        snprintf(path, sizeof(path), "%s/.config/mriya/mriya.rc", home);
        seed_file(path);
    }
}

static FILE *resolve_config(char *buf, size_t size) {
    const char *home = getenv("HOME");
    const char *xdg = getenv("XDG_CONFIG_HOME");

    if (xdg && *xdg) {
        snprintf(buf, size, "%s/mriya/mriya.rc", xdg);
        if (access(buf, R_OK) == 0) return fopen(buf, "r");
    }
    if (home && *home) {
        snprintf(buf, size, "%s/.config/mriya/mriya.rc", home);
        if (access(buf, R_OK) == 0) return fopen(buf, "r");
    }
    snprintf(buf, size, "/etc/mriya/mriya.rc");
    if (access(buf, R_OK) == 0) return fopen(buf, "r");
    snprintf(buf, size, "/etc/mriya.rc");
    if (access(buf, R_OK) == 0) return fopen(buf, "r");
    return NULL;
}

static KeySym resolve_keysym(const char *name) {
    KeySym ks = XStringToKeysym(name);
    if (ks != NoSymbol) return ks;

    char buf[64];
    size_t n = strlen(name);
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    buf[0] = toupper((unsigned char)name[0]);
    for (size_t i = 1; i < n; i++) buf[i] = tolower((unsigned char)name[i]);
    buf[n] = '\0';
    ks = XStringToKeysym(buf);
    if (ks != NoSymbol) return ks;
    for (size_t i = 0; i < n; i++) buf[i] = toupper((unsigned char)name[i]);
    buf[n] = '\0';
    return XStringToKeysym(buf);
}

static unsigned int decode_modifier(const char *tok) {
    char buf[32];
    size_t n;
    if (!tok) return 0;
    n = strlen(tok);
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    for (size_t i = 0; i < n; i++)
        buf[i] = tolower((unsigned char)tok[i]);
    buf[n] = '\0';
    if (!strcmp(buf, "mod")) return default_modkey;
    if (!strcmp(buf, "shift")) return ShiftMask;
    if (!strcmp(buf, "ctrl") || !strcmp(buf, "control")) return ControlMask;
    if (!strcmp(buf, "alt")) return Mod1Mask;
    if (!strcmp(buf, "super")) return Mod4Mask;
    return 0;
}

static unsigned int parse_chord(const char *chord, KeySym *out_key) {
    unsigned int mods = 0;
    KeySym ks = NoSymbol;
    char buf[256];
    char *token;

    strncpy(buf, chord, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    for (char *p = buf; *p; p++)
        if (*p == '+' || isspace((unsigned char)*p)) *p = '+';

    for (token = strtok(buf, "+"); token; token = strtok(NULL, "+")) {
        unsigned int m = decode_modifier(token);
        if (m) mods |= m;
        else ks = resolve_keysym(token);
    }

    *out_key = ks;
    return mods;
}

static void register_key(unsigned int mods, KeySym ks, void (*fn)(const char *), const char *arg) {
    if (nkeys >= 256 || ks == NoSymbol || !fn) return;
    keys[nkeys].mod = mods;
    keys[nkeys].keysym = ks;
    keys[nkeys].func = fn;
    keys[nkeys].arg = arg;
    nkeys++;
}

static void register_button(unsigned int mods, unsigned int button, void (*fn)(const char *), const char *arg) {
    if (nbuttons >= 256 || button == 0 || !fn) return;
    buttons[nbuttons].mod = mods;
    buttons[nbuttons].button = button;
    buttons[nbuttons].func = fn;
    buttons[nbuttons].arg = arg;
    nbuttons++;
}

static unsigned int parse_button_chord(const char *chord, unsigned int *out_button) {
    unsigned int mods = 0;
    unsigned int button = 0;
    char buf[256];
    char *token;

    *out_button = 0;
    strncpy(buf, chord, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    for (char *p = buf; *p; p++)
        if (*p == '+' || isspace((unsigned char)*p)) *p = '+';

    for (token = strtok(buf, "+"); token; token = strtok(NULL, "+")) {
        unsigned int m = decode_modifier(token);
        if (m) mods |= m;
        else if (!strcasecmp(token, "button1") || !strcasecmp(token, "lmb")) button = Button1;
        else if (!strcasecmp(token, "button2") || !strcasecmp(token, "mmb")) button = Button2;
        else if (!strcasecmp(token, "button3") || !strcasecmp(token, "rmb")) button = Button3;
        else if (!strcasecmp(token, "button4") || !strcasecmp(token, "scrollup")) button = Button4;
        else if (!strcasecmp(token, "button5") || !strcasecmp(token, "scrolldown")) button = Button5;
    }

    *out_button = button;
    return mods;
}

static void assign_color(char *target, const char *value) {
    strncpy(target, value, 63);
    target[63] = '\0';
}

static void handle_setting(const char *key, char *value) {
    if (!strcmp(key, "norm_bg") || !strcmp(key, "normal_bg"))
        assign_color(norm_bg, value);
    else if (!strcmp(key, "norm_outer_border") || !strcmp(key, "unfocused_border_colour"))
        assign_color(norm_outer_border, value);
    else if (!strcmp(key, "norm_inner_border"))
        assign_color(norm_inner_border, value);
    else if (!strcmp(key, "sel_bg") || !strcmp(key, "focused_bg"))
        assign_color(sel_bg, value);
    else if (!strcmp(key, "sel_outer_border") || !strcmp(key, "focused_border_colour"))
        assign_color(sel_outer_border, value);
    else if (!strcmp(key, "sel_inner_border"))
        assign_color(sel_inner_border, value);
    else if (!strcmp(key, "urgent_color") || !strcmp(key, "urgent_colour"))
        assign_color(urgent_color, value);
    else if (!strcmp(key, "title_active_bg"))
        assign_color(title_active_bg, value);
    else if (!strcmp(key, "title_active_fg"))
        assign_color(title_active_fg, value);
    else if (!strcmp(key, "title_inactive_bg"))
        assign_color(title_inactive_bg, value);
    else if (!strcmp(key, "title_inactive_fg"))
        assign_color(title_inactive_fg, value);
    else if (!strcmp(key, "title_font"))
        assign_color(title_font, value);
    else if (!strcmp(key, "inner_gap") || !strcmp(key, "gaps"))
        inner_gaps = atoi(value);
    else if (!strcmp(key, "outer_gap") || !strcmp(key, "outer_gaps"))
        outer_gaps = atoi(value);
    else if (!strcmp(key, "snap") || !strcmp(key, "snap_distance"))
        snap_val = atoi(value);
    else if (!strcmp(key, "outer_border_width"))
        outer_border_width = atoi(value);
    else if (!strcmp(key, "inner_border_width"))
        inner_border_width = atoi(value);
    else if (!strcmp(key, "border_width")) {
        outer_border_width = atoi(value);
        inner_border_width = 0;
    }
    else if (!strcmp(key, "total_border_width") || !strcmp(key, "border"))
        border_width = atoi(value);
    else if (!strcmp(key, "show_titlebar") || !strcmp(key, "titlebar"))
        show_titlebar = atoi(value);
    else if (!strcmp(key, "show_title"))
        show_title = atoi(value);
    else if (!strcmp(key, "show_buttons"))
        show_buttons = atoi(value);
    else if (!strcmp(key, "insert_end"))
        insert_end = atoi(value);
    else if (!strcmp(key, "strip_align"))
        strip_align = atoi(value);
    else if (!strcmp(key, "show_move_indicator"))
        show_move_indicator = atoi(value);
    else if (!strcmp(key, "move_indicator_color"))
        assign_color(move_indicator_color, value);
    else if (!strcmp(key, "mod_key") || !strcmp(key, "modkey")) {
        if (!strcmp(value, "super")) default_modkey = Mod4Mask;
        else if (!strcmp(value, "alt")) default_modkey = Mod1Mask;
        else if (!strcmp(value, "ctrl") || !strcmp(value, "control")) default_modkey = ControlMask;
        else if (!strcmp(value, "shift")) default_modkey = ShiftMask;
    }
}

static void handle_autostart(char *value) {
    unquote(value);
    if (*value && nautostart < 256)
        autostart_cmds[nautostart++] = strdup(value);
}

static void handle_bind(char *value) {
    char *sep = strchr(value, ':');
    KeySym ks;
    unsigned int mods;
    if (!sep) return;
    *sep = '\0';
    mods = parse_chord(value, &ks);
    trim(sep + 1);
    unquote(sep + 1);
    if (ks != NoSymbol && *(sep + 1))
        register_key(mods, ks, spawn, strdup(sep + 1));
}

static void handle_call(char *value) {
    char *sep = strchr(value, ':');
    KeySym ks;
    unsigned int mods;
    if (!sep) return;
    *sep = '\0';
    mods = parse_chord(value, &ks);
    trim(sep + 1);
    if (ks != NoSymbol) {
        for (int i = 0; func_table[i].name; i++) {
            if (!strcmp(sep + 1, func_table[i].name)) {
                register_key(mods, ks, func_table[i].func, func_table[i].arg);
                break;
            }
        }
    }
}

static void handle_mousebind(char *value) {
    char *sep = strchr(value, ':');
    unsigned int mods;
    unsigned int button;
    if (!sep) return;
    *sep = '\0';
    mods = parse_button_chord(value, &button);
    trim(sep + 1);
    if (button != 0) {
        for (int i = 0; func_table[i].name; i++) {
            if (!strcmp(sep + 1, func_table[i].name)) {
                register_button(mods, button, func_table[i].func, func_table[i].arg);
                break;
            }
        }
    }
}

static void handle_workspace(char *value) {
    char *sep = strchr(value, ':');
    KeySym ks;
    unsigned int mods;
    char action[64];
    int num;
    if (!sep) return;
    *sep = '\0';
    mods = parse_chord(value, &ks);
    trim(sep + 1);
    if (ks == NoSymbol) return;
    if (sscanf(sep + 1, "%63s %d", action, &num) == 2 && num >= 1 && num <= 9) {
        const char *ws_arg = ws_args[num - 1];
        if (!strcmp(action, "view") || !strcmp(action, "move"))
            register_key(mods, ks, view, ws_arg);
        else if (!strcmp(action, "tag") || !strcmp(action, "swap"))
            register_key(mods, ks, tag, ws_arg);
        else if (!strcmp(action, "toggleview"))
            register_key(mods, ks, toggleview, ws_arg);
        else if (!strcmp(action, "toggletag"))
            register_key(mods, ks, toggletag, ws_arg);
    }
}

int parse_config(void) {
    char path[4096];
    char line[512];
    FILE *f;

    ensure_config_files_exist();
    f = resolve_config(path, sizeof(path));
    if (!f) return -1;

    nkeys = 0;
    nautostart = 0;

    while (fgets(line, sizeof(line), f)) {
        char *s = line;
        char *sep;

        trim(s);
        eat_comment(s);
        if (!*s) continue;

        sep = strchr(s, ':');
        if (!sep) continue;
        *sep = '\0';

        char *key = s;
        char *value = sep + 1;
        trim(key);
        trim(value);

        if (!strcmp(key, "autostart")) {
            handle_autostart(value);
        } else if (!strcmp(key, "bind")) {
            handle_bind(value);
        } else if (!strcmp(key, "call")) {
            handle_call(value);
        } else if (!strcmp(key, "workspace")) {
            handle_workspace(value);
        } else if (!strcmp(key, "mousebind") || !strcmp(key, "mouse")) {
            handle_mousebind(value);
        } else {
            handle_setting(key, value);
        }
    }

    fclose(f);
    printf("mriya: config parsed from %s\n", path);
    return 0;
}
