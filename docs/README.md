# mriya

is a scrolling x11 window manager.

# fyi

mriya is highly experimental. it is at a point where it is usable with some tinkering. DO NOT EXPECT SOMETHING ON THE LEVEL OF NIRI AS IT IS A SOLO PROJECT 

![License](https://img.shields.io/badge/license-BSD3C-blue.svg)

<p align="center">
  <img src="/assets/img.png" alt="img" width="700">
</p>

# to go more in-depth

scrolling means that windows are arranged on a ribbon-style horizontal strip that the user can scroll through.

mriya is heavily inspired by [niri](https://github.com/niri-wm/niri) and [i3](https://i3wm.org/)

# mriya has been tested on

- generic [linux](https://www.linux.org) x86_64
- [freebsd](https://www.freebsd.org/) 15.1 x86_64
- [netbsd](https://www.netbsd.org/) 10.1 x86_64
- [openindiana](https://www.openindiana.org) hipster 2026.04 x86_64

# deps

- libX11-devel (xlib)
- libxkbfile-devel
- libxkbcommon-devel
- alsamixer (optional, for volume keys)
- brightnessctl (optional, for brightness keys)

# install

```bash
sudo make install
```

this copies `mriya` to `/usr/local/bin/mriya`.

# run 

```startx mriya```

# set-up with display managers

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

# note

- some distros don't have `/usr/local/bin` in `$PATH` by default.

# configure

edit ```/$HOME/.config/mriya/mriya.rc```

# detailed documentation

refer to [the website](https://hokum-b.github.io/mriya)

# license

mriya is licensed under the [3 Clause BSD License](LICENSE)
