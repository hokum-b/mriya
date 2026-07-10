# mriya
is a scrolling x11 window manager.

<p align="center">
  <img src="assets/img.png" alt="img" width="700">
</p>

## to go more in-depth

scrolling means that windows are arranged on a ribbon-style horizontal strip that the user can scroll through.

mriya is heavily inspired by [niri](https://github.com/niri-wm/niri) and [i3](https://i3wm.org/)

## mriya has been tested on

- generic [linux](https://www.linux.org) x86_64
- [freebsd](https://www.freebsd.org/) 15.1 x86_64
- [netbsd](https://www.netbsd.org/) 10.1 x86_64
- [openindiana](https://www.indiana.org) hipster 2026.04 x86_64

## deps

- libX11-devel
- libxkbfile-devel
- libxkbcommon-devel
- alsamixer
- brightnessctl

## compile

```bash
git clone https://github.com/hokum-b/mriya
cd mriya/
make
```

### run 

```startx ./mriya```

or move ```mriya``` to ```/usr/local/bin``` and run ```startx mriya```

(some distro's dont have ```/usr/local/bin``` in their ```$PATH``` by default, run echo $PATH to make sure.)

### set-up with display managers

- create a ```/usr/share/xsessions/mriya.desktop```
- paste

```desktop
[Desktop Entry]
Name=mriya
Comment=scrolling x11 window manager
Exec=mriya
Type=Application
DesktopNames=mriya
```


# keybinds

- mod + return/enter = open TERM
- mod + shift + return/enter = open TERM
- mod + d = open DMENU
- mod + shift + q = close focused window
- mod + shift + e = kill wm
- mod + shift + r = restart wm
- mod + h / left = focus left
- mod + l / right = focus right
- mod + j = decrease inner gap
- mod + k = increase inner gap
- mod + shift + j = reset gaps
- mod + space = zoom (swap with master)
- mod + shift + space = toggle floating
- mod + f = toggle maximize
- mod + shift + f = toggle fullscreen
- mod + tab = switch to last workspace
- mod + up = workspace up
- mod + down = workspace down
- mod + 1-9 = switch to workspace 1-9
- mod + shift + 1-9 = move window to workspace 1-9
- mod + ctrl + 1-9 = toggle workspace 1-9 visibility
- mod + ctrl + shift + 1-9 = toggle workspace 1-9 on window
- mod + lmb (drag) = move window 
- mod + rmb (drag) = resize window 

# applications

edit `config.h`:

```c
#define TERM "st"
#define DMENU "dmenu_run"
#define BROWSER "firefox"
#define FILEMANAGER "pcmanfm"
```

# license

mriya is licensed under the [ISC license](LICENSE)
