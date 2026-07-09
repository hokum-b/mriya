#ifndef CONFIG_H
#define CONFIG_H

// double border settings
#define OUTER_BORDER_WIDTH 0
#define INNER_BORDER_WIDTH 0
#define BORDER_WIDTH (OUTER_BORDER_WIDTH + INNER_BORDER_WIDTH)

// gap and snap settings
#define INNER_GAP 15
#define OUTER_GAP 50
#define SNAP 32

// colors
#define NORM_BG "#222222"
#define NORM_OUTER_BORDER "#ede5d4"
#define NORM_INNER_BORDER "#111111"
#define SEL_BG "#111111"
#define SEL_OUTER_BORDER "#ede5d4"
#define SEL_INNER_BORDER "#ffffff"
#define URGENT_COLOR "#ede5d4"

// titlebar colors
#define TITLE_ACTIVE_BG   SEL_BG
#define TITLE_ACTIVE_FG   "#ffffff"
#define TITLE_INACTIVE_BG NORM_BG
#define TITLE_INACTIVE_FG NORM_OUTER_BORDER

// titlebar toggles
#define SHOW_TITLEBAR 1
#define SHOW_TITLE   1
#define SHOW_BUTTONS 0

// modifier key
#define MODKEY Mod4Mask

#define MOUSEMASK (ButtonPressMask|ButtonReleaseMask|PointerMotionMask)

// programs
#define TERM "alacritty"
#define DMENU "dmenu_run"
#define BROWSER "firefox"
#define FILEMANAGER "pcmanfm"

// audio and brightness commands
#define VOL_UP    "amixer -q set Master 5%+"
#define VOL_DOWN  "amixer -q set Master 5%-"
#define VOL_MUTE  "amixer -q set Master toggle"
#define BRI_UP    "brightnessctl -q set +5%"
#define BRI_DOWN  "brightnessctl -q set 5%-"

#define SCROLL_WHEEL_WS 1

// autostart commands
static const char *autostart_cmds[] = {
    "xsetroot -solid '#222222'",
    "xrdb -merge ~/.Xresources",
    "picom --backend glx &",
    "dunst &",
    "sheet --restore",
    "polybar example &",
    "nm-applet &",
    "volumeicon &",
};

#define AUTOSTART_LEN (sizeof autostart_cmds / sizeof autostart_cmds[0])

// workspace tags
static const char *tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };

// window rules
static const Rule rules[] = {
    { "Gimp", NULL, NULL, 0, 1, -1 },
    { "Firefox", NULL, NULL, 1 << 8, 0, -1 },
    { "St", NULL, NULL, 0, 0, -1 },
    { "Alacritty", NULL, NULL, 0, 0, -1 },
};

// layout symbols
static const Layout layouts[] = {
    { "[=]", tile },
    { "><>", NULL },
    { "[M]", monocle },
};

// tag keybind generator
#define TAGKEYS(KEY,TAG) \
    { MODKEY, KEY, view, #TAG }, \
    { MODKEY|ControlMask, KEY, toggleview, #TAG }, \
    { MODKEY|ShiftMask, KEY, tag, #TAG }, \
    { MODKEY|ControlMask|ShiftMask, KEY, toggletag, #TAG },

// keybind shorthand
#define KEY(MOD, KEY, FUNC, ARG) { MOD, KEY, FUNC, ARG },

// forward declarations for keybind callbacks
static void spawn(const char *arg);
static void killclient(const char *arg);
static void quit(const char *arg);
static void restartwm(const char *arg);
static void focusleft(const char *arg);
static void focusright(const char *arg);
static void setgaps(const char *arg);
static void zoom(const char *arg);
static void togglefloating(const char *arg);
static void togglemaximize(const char *arg);
static void togglefullscreen(const char *arg);
static void view(const char *arg);
static void toggleview(const char *arg);
static void tag(const char *arg);
static void toggletag(const char *arg);
static void ws_up(const char *arg);
static void ws_down(const char *arg);
static void movemouse(const char *arg);
static void resizemouse(const char *arg);

// keybindings
static Key keys[] = {
    // launch programs
    KEY(MODKEY, XK_Return, spawn, TERM)
    KEY(MODKEY|ShiftMask, XK_Return, spawn, TERM)
    KEY(MODKEY, XK_d, spawn, DMENU)
    // close and quit
    KEY(MODKEY, XK_w, killclient, NULL)
    KEY(MODKEY|ShiftMask, XK_q, killclient, NULL)
    KEY(MODKEY|ShiftMask, XK_e, quit, NULL)
    KEY(MODKEY|ShiftMask, XK_r, restartwm, NULL)
    // focus navigation (vim style hjkl)
    KEY(MODKEY, XK_h, focusleft, NULL)
    KEY(MODKEY, XK_j, focusright, NULL)
    KEY(MODKEY, XK_k, focusleft, NULL)
    KEY(MODKEY, XK_l, focusright, NULL)
    KEY(MODKEY, XK_Left, focusleft, NULL)
    KEY(MODKEY, XK_Right, focusright, NULL)
    // workspace navigation
    KEY(MODKEY, XK_Up, ws_up, NULL)
    KEY(MODKEY, XK_Down, ws_down, NULL)
    // gap controls
    KEY(MODKEY|ShiftMask, XK_j, setgaps, "-2")
    KEY(MODKEY|ShiftMask, XK_k, setgaps, "+2")
    KEY(MODKEY|ControlMask, XK_j, setgaps, "0")
    // window state
    KEY(MODKEY, XK_space, zoom, NULL)
    KEY(MODKEY|ShiftMask, XK_space, togglefloating, NULL)
    KEY(MODKEY, XK_f, togglemaximize, NULL)
    KEY(MODKEY|ShiftMask, XK_f, togglefullscreen, NULL)
    // tab to cycle workspaces
    KEY(MODKEY, XK_Tab, view, NULL)
    // media keys
    KEY(0, XF86XK_AudioMute, spawn, VOL_MUTE)
    KEY(0, XF86XK_AudioLowerVolume, spawn, VOL_DOWN)
    KEY(0, XF86XK_AudioRaiseVolume, spawn, VOL_UP)
    KEY(0, XF86XK_MonBrightnessDown, spawn, BRI_DOWN)
    KEY(0, XF86XK_MonBrightnessUp, spawn, BRI_UP)
    // workspace tag keys
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

// mouse bindings
static Button buttons[] = {
    { MODKEY, Button1, movemouse, NULL },
    { MODKEY, Button3, resizemouse, NULL },
};

// misc settings
#define INSERT_END 1

#define STRIP_ALIGN 0

#endif
