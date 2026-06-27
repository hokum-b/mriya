/* mriyawm config.h - user settings */

#ifndef CONFIG_H
#define CONFIG_H

/* window borders */
#define BORDER_WIDTH 2

/* inner gap between windows */
#define INNER_GAP 15

/* outer gap - screen edge padding */
#define OUTER_GAP 50

/* snap distance for mouse resize */
#define SNAP 32

/* colors - use x11 names or #rrggbb */
#define NORM_BG "#222222"
#define NORM_BORDER "#444444"
#define SEL_BG "#005577"
#define SEL_BORDER "#005577"
#define URGENT_COLOR "#ff0000"

/* modifier key: mod4mask = super/win, mod1mask = alt */
#define MODKEY Mod4Mask

/* mouse events mask */
#define MOUSEMASK (ButtonPressMask|ButtonReleaseMask|PointerMotionMask)

/* default programs */
#define TERM "st"
#define DMENU "dmenu_run"
#define BROWSER "firefox"
#define FILEMANAGER "pcmanfm"

/* volume and brightness controls
 * requires: alsa-utils (amixer) and brightnessctl
 * alternatives: pulseaudio/pipewire (pactl), xbacklight
 */
#define VOL_UP    "amixer -q set Master 5%+"
#define VOL_DOWN  "amixer -q set Master 5%-"
#define VOL_MUTE  "amixer -q set Master toggle"
#define BRI_UP    "brightnessctl -q set +5%"
#define BRI_DOWN  "brightnessctl -q set 5%-"

/* scroll wheel switches workspaces. 1 = on, 0 = off */
#define SCROLL_WHEEL_WS 1

/* commands to run on startup */
static const char *autostart_cmds[] = {
    "xsetroot -solid '#222222'",
    "xrdb -merge ~/.Xresources",
    "picom --backend glx &",
    "dunst &",
    "feh --bg-scale ~/wallpaper.jpg",
    "polybar example &",
    "nm-applet &",
    "volumeicon &",
};

#define AUTOSTART_LEN (sizeof autostart_cmds / sizeof autostart_cmds[0])

/* workspace names */
static const char *tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };

/* window rules: class, instance, title, tags, floating, monitor */
static const Rule rules[] = {
    { "Gimp", NULL, NULL, 0, 1, -1 },
    { "Firefox", NULL, NULL, 1 << 8, 0, -1 },
    { "St", NULL, NULL, 0, 0, -1 },
    { "Alacritty", NULL, NULL, 0, 0, -1 },
};

/* layout modes */
static const Layout layouts[] = {
    { "[=]", tile },
    { "><>", NULL },
    { "[M]", monocle },
};

/* tag key bindings macro */
#define TAGKEYS(KEY,TAG) \
    { MODKEY, KEY, view, #TAG }, \
    { MODKEY|ControlMask, KEY, toggleview, #TAG }, \
    { MODKEY|ShiftMask, KEY, tag, #TAG }, \
    { MODKEY|ControlMask|ShiftMask, KEY, toggletag, #TAG },

/* 1 to attach new windows at the end of the stack, 0 to attach at the beginning */
#define INSERT_END 1

#endif
