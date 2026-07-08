#ifndef CONFIG_H
#define CONFIG_H

#define BORDER_WIDTH 0

#define INNER_GAP 15
#define OUTER_GAP 50
#define SNAP 32

#define NORM_BG "#222222"
#define NORM_BORDER "#ede5d4"
#define SEL_BG "#111111"
#define SEL_BORDER "#ede5d4"
#define URGENT_COLOR "#ede5d4"

#define TITLE_ACTIVE_BG   SEL_BG
#define TITLE_ACTIVE_FG   "#ffffff"
#define TITLE_INACTIVE_BG NORM_BG
#define TITLE_INACTIVE_FG NORM_BORDER

#define SHOW_TITLEBAR 1
#define SHOW_TITLE   1
#define SHOW_BUTTONS 0

#define MODKEY Mod4Mask

#define MOUSEMASK (ButtonPressMask|ButtonReleaseMask|PointerMotionMask)

#define TERM "alacritty"
#define DMENU "dmenu_run"
#define BROWSER "firefox"
#define FILEMANAGER "pcmanfm"

#define VOL_UP    "amixer -q set Master 5%+"
#define VOL_DOWN  "amixer -q set Master 5%-"
#define VOL_MUTE  "amixer -q set Master toggle"
#define BRI_UP    "brightnessctl -q set +5%"
#define BRI_DOWN  "brightnessctl -q set 5%-"

#define SCROLL_WHEEL_WS 1

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

static const char *tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };

static const Rule rules[] = {
    { "Gimp", NULL, NULL, 0, 1, -1 },
    { "Firefox", NULL, NULL, 1 << 8, 0, -1 },
    { "St", NULL, NULL, 0, 0, -1 },
    { "Alacritty", NULL, NULL, 0, 0, -1 },
};

static const Layout layouts[] = {
    { "[=]", tile },
    { "><>", NULL },
    { "[M]", monocle },
};

#define TAGKEYS(KEY,TAG) \
    { MODKEY, KEY, view, #TAG }, \
    { MODKEY|ControlMask, KEY, toggleview, #TAG }, \
    { MODKEY|ShiftMask, KEY, tag, #TAG }, \
    { MODKEY|ControlMask|ShiftMask, KEY, toggletag, #TAG },

#define INSERT_END 1

#define STRIP_ALIGN 0

#endif
