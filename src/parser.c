#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
"\n"
"inner_gap : 30\n"
"outer_gap : 50\n"
"snap : 32\n"
"outer_border_width : 0\n"
"inner_border_width : 0\n"
"\n"
"mod_key : super\n"
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

static void mkdir_p(const char *path) {
    char temp[4096];
    char *p = NULL;
    size_t len;

    snprintf(temp, sizeof(temp), "%s", path);
    len = strlen(temp);
    if (len == 0) return;
    if (temp[len - 1] == '/') temp[len - 1] = '\0';
    for (p = temp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(temp, 0755);
            *p = '/';
        }
    }
    mkdir(temp, 0755);
}

static void create_file_if_missing(const char *filepath) {
    if (access(filepath, F_OK) != 0) {
        char dirpath[4096];
        snprintf(dirpath, sizeof(dirpath), "%s", filepath);
        char *last_slash = strrchr(dirpath, '/');
        if (last_slash) {
            *last_slash = '\0';
            mkdir_p(dirpath);
        }
        FILE *f = fopen(filepath, "w");
        if (f) {
            fputs(default_config_content, f);
            fclose(f);
        }
    }
}

void ensure_config_files_exist(void) {
    const char *home = getenv("HOME");
    const char *xdg = getenv("XDG_CONFIG_HOME");
    char user_cfg[4096];

    create_file_if_missing("/etc/mriya/mriya.rc");

    if (xdg && *xdg) {
        snprintf(user_cfg, sizeof(user_cfg), "%s/mriya/mriya.rc", xdg);
        create_file_if_missing(user_cfg);
    } else if (home && *home) {
        snprintf(user_cfg, sizeof(user_cfg), "%s/.config/mriya/mriya.rc", home);
        create_file_if_missing(user_cfg);
    }
}

static char *strip(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    if (!*s) return s;
    char *e = s + strlen(s) - 1;
    while (e > s && isspace((unsigned char)*e)) *e-- = '\0';
    return s;
}

static char *strip_comment(char *s) {
    char *c = strchr(s, '#');
    if (c) *c = '\0';
    return strip(s);
}

static char *strip_quotes(char *s) {
    size_t len = strlen(s);
    if (len > 0 && (s[0] == '"' || s[0] == '\'')) {
        s++;
        len--;
    }
    if (len > 0 && (s[len - 1] == '"' || s[len - 1] == '\''))
        s[len - 1] = '\0';
    return s;
}

static KeySym parse_keysym(const char *key) {
    KeySym ks = XStringToKeysym(key);
    if (ks != NoSymbol) return ks;
    char buf[64];
    size_t n = strlen(key);
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    buf[0] = toupper((unsigned char)key[0]);
    for (size_t i = 1; i < n; i++) buf[i] = tolower((unsigned char)key[i]);
    buf[n] = '\0';
    ks = XStringToKeysym(buf);
    if (ks != NoSymbol) return ks;
    for (size_t i = 0; i < n; i++) buf[i] = toupper((unsigned char)key[i]);
    buf[n] = '\0';
    ks = XStringToKeysym(buf);
    return ks;
}

static unsigned int parse_combo(const char *combo, KeySym *out_ks) {
    unsigned int m = 0;
    KeySym ks = NoSymbol;
    char buf[256];
    strncpy(buf, combo, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    for (char *p = buf; *p; p++) {
        if (*p == '+' || isspace((unsigned char)*p)) *p = '+';
    }
    for (char *tok = strtok(buf, "+"); tok; tok = strtok(NULL, "+")) {
        if (!strcmp(tok, "mod")) m |= default_modkey;
        else if (!strcmp(tok, "shift") || !strcmp(tok, "Shift")) m |= ShiftMask;
        else if (!strcmp(tok, "ctrl") || !strcmp(tok, "Control")) m |= ControlMask;
        else if (!strcmp(tok, "alt") || !strcmp(tok, "Alt")) m |= Mod1Mask;
        else if (!strcmp(tok, "super") || !strcmp(tok, "Super")) m |= Mod4Mask;
        else {
            ks = parse_keysym(tok);
        }
    }
    *out_ks = ks;
    return m;
}

static void add_key(unsigned int mod, KeySym ks, void (*func)(const char *), const char *arg) {
    if (nkeys >= 256 || ks == NoSymbol || !func) return;
    keys[nkeys].mod = mod;
    keys[nkeys].keysym = ks;
    keys[nkeys].func = func;
    keys[nkeys].arg = arg;
    nkeys++;
}

static FILE *open_config(char *path, size_t pathsz) {
    const char *home = getenv("HOME");
    const char *xdg = getenv("XDG_CONFIG_HOME");

    if (xdg && *xdg) {
        snprintf(path, pathsz, "%s/mriya/mriya.rc", xdg);
        if (access(path, R_OK) == 0) return fopen(path, "r");
    }
    if (home && *home) {
        snprintf(path, pathsz, "%s/.config/mriya/mriya.rc", home);
        if (access(path, R_OK) == 0) return fopen(path, "r");
    }
    snprintf(path, pathsz, "/etc/mriya/mriya.rc");
    if (access(path, R_OK) == 0) return fopen(path, "r");

    snprintf(path, pathsz, "/etc/mriya.rc");
    if (access(path, R_OK) == 0) return fopen(path, "r");

    return NULL;
}

int parse_config(void) {
    char path[4096];
    ensure_config_files_exist();
    FILE *f = open_config(path, sizeof(path));
    if (!f) return -1;

    nkeys = 0;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char *s = strip_comment(strip(line));
        if (!*s) continue;

        char *sep = strchr(s, ':');
        if (!sep) continue;
        *sep = '\0';
        char *key = strip(s);
        char *rest = strip(sep + 1);

        if (!strcmp(key, "norm_bg") || !strcmp(key, "normal_bg"))
            strncpy(norm_bg, rest, sizeof(norm_bg) - 1);
        else if (!strcmp(key, "norm_outer_border") || !strcmp(key, "unfocused_border_colour"))
            strncpy(norm_outer_border, rest, sizeof(norm_outer_border) - 1);
        else if (!strcmp(key, "norm_inner_border"))
            strncpy(norm_inner_border, rest, sizeof(norm_inner_border) - 1);
        else if (!strcmp(key, "sel_bg") || !strcmp(key, "focused_bg"))
            strncpy(sel_bg, rest, sizeof(sel_bg) - 1);
        else if (!strcmp(key, "sel_outer_border") || !strcmp(key, "focused_border_colour"))
            strncpy(sel_outer_border, rest, sizeof(sel_outer_border) - 1);
        else if (!strcmp(key, "sel_inner_border"))
            strncpy(sel_inner_border, rest, sizeof(sel_inner_border) - 1);
        else if (!strcmp(key, "urgent_color") || !strcmp(key, "urgent_colour"))
            strncpy(urgent_color, rest, sizeof(urgent_color) - 1);
        else if (!strcmp(key, "title_active_bg"))
            strncpy(title_active_bg, rest, sizeof(title_active_bg) - 1);
        else if (!strcmp(key, "title_active_fg"))
            strncpy(title_active_fg, rest, sizeof(title_active_fg) - 1);
        else if (!strcmp(key, "title_inactive_bg"))
            strncpy(title_inactive_bg, rest, sizeof(title_inactive_bg) - 1);
        else if (!strcmp(key, "title_inactive_fg"))
            strncpy(title_inactive_fg, rest, sizeof(title_inactive_fg) - 1);
        else if (!strcmp(key, "inner_gap") || !strcmp(key, "gaps"))
            inner_gaps = atoi(rest);
        else if (!strcmp(key, "outer_gap") || !strcmp(key, "outer_gaps"))
            outer_gaps = atoi(rest);
        else if (!strcmp(key, "snap") || !strcmp(key, "snap_distance"))
            snap_val = atoi(rest);
        else if (!strcmp(key, "outer_border_width"))
            outer_border_width = atoi(rest);
        else if (!strcmp(key, "inner_border_width"))
            inner_border_width = atoi(rest);
        else if (!strcmp(key, "border_width")) {
            outer_border_width = atoi(rest);
            inner_border_width = 0;
        }
        else if (!strcmp(key, "total_border_width") || !strcmp(key, "border"))
            border_width = atoi(rest);
        else if (!strcmp(key, "show_titlebar") || !strcmp(key, "titlebar"))
            show_titlebar = atoi(rest);
        else if (!strcmp(key, "show_title"))
            show_title = atoi(rest);
        else if (!strcmp(key, "show_buttons"))
            show_buttons = atoi(rest);
        else if (!strcmp(key, "insert_end"))
            insert_end = atoi(rest);
        else if (!strcmp(key, "strip_align"))
            strip_align = atoi(rest);
        else if (!strcmp(key, "show_move_indicator"))
            show_move_indicator = atoi(rest);
        else if (!strcmp(key, "move_indicator_color"))
            strncpy(move_indicator_color, rest, sizeof(move_indicator_color) - 1);
        else if (!strcmp(key, "mod_key") || !strcmp(key, "modkey")) {
            if (!strcmp(rest, "super") || !strcmp(rest, "Super")) default_modkey = Mod4Mask;
            else if (!strcmp(rest, "alt") || !strcmp(rest, "Alt")) default_modkey = Mod1Mask;
            else if (!strcmp(rest, "ctrl") || !strcmp(rest, "Control")) default_modkey = ControlMask;
            else if (!strcmp(rest, "shift") || !strcmp(rest, "Shift")) default_modkey = ShiftMask;
        }
        else if (!strcmp(key, "bind")) {
            char *mid = strchr(rest, ':');
            if (!mid) continue;
            *mid = '\0';
            char *combo = strip(rest);
            char *cmd = strip_quotes(strip(mid + 1));
            KeySym ks;
            unsigned int mods = parse_combo(combo, &ks);
            if (ks != NoSymbol && *cmd) {
                add_key(mods, ks, spawn, strdup(cmd));
            }
        }
        else if (!strcmp(key, "call")) {
            char *mid = strchr(rest, ':');
            if (!mid) continue;
            *mid = '\0';
            char *combo = strip(rest);
            char *fn_name = strip(mid + 1);
            KeySym ks;
            unsigned int mods = parse_combo(combo, &ks);
            if (ks != NoSymbol) {
                for (int i = 0; func_table[i].name; i++) {
                    if (!strcmp(fn_name, func_table[i].name)) {
                        add_key(mods, ks, func_table[i].func, func_table[i].arg);
                        break;
                    }
                }
            }
        }
        else if (!strcmp(key, "workspace")) {
            char *mid = strchr(rest, ':');
            if (!mid) continue;
            *mid = '\0';
            char *combo = strip(rest);
            char *act = strip(mid + 1);
            KeySym ks;
            unsigned int mods = parse_combo(combo, &ks);
            if (ks != NoSymbol) {
                char act_type[64];
                int ws_num = 0;
                if (sscanf(act, "%s %d", act_type, &ws_num) == 2 && ws_num >= 1 && ws_num <= 9) {
                    const char *ws_arg = ws_args[ws_num - 1];
                    if (!strcmp(act_type, "view") || !strcmp(act_type, "move"))
                        add_key(mods, ks, view, ws_arg);
                    else if (!strcmp(act_type, "tag") || !strcmp(act_type, "swap"))
                        add_key(mods, ks, tag, ws_arg);
                    else if (!strcmp(act_type, "toggleview"))
                        add_key(mods, ks, toggleview, ws_arg);
                    else if (!strcmp(act_type, "toggletag"))
                        add_key(mods, ks, toggletag, ws_arg);
                }
            }
        }
    }

    fclose(f);
    printf("mriya: config parsed from %s\n", path);
    return 0;
}
