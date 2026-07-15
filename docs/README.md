# mriya

is a scrolling x11 window manager.

![License](https://img.shields.io/badge/license-ISC-blue.svg)
![Platform](https://img.shields.io/badge/platform-linux%20%7C%20bsd%20%7C%20openindiana-lightgrey.svg)

<p align="center">
  <img src="/assets/img.png" alt="img" width="700">
</p>

## to go more in-depth

scrolling means that windows are arranged on a ribbon-style horizontal strip that the user can scroll through.

mriya is heavily inspired by [niri](https://github.com/niri-wm/niri) and [i3](https://i3wm.org/)

## mriya has been tested on

- generic [linux](https://www.linux.org) x86_64
- [freebsd](https://www.freebsd.org/) 15.1 x86_64
- [netbsd](https://www.netbsd.org/) 10.1 x86_64
- [openindiana](https://www.openindiana.org) hipster 2026.04 x86_64

## compile

```bash
git clone https://github.com/hokum-b/mriya
cd mriya/
make
```

(the binary is produced at `./mriya`.)

### deps

- libX11-devel (xlib)
- libxkbfile-devel
- libxkbcommon-devel
- alsamixer (optional, for volume keys)
- brightnessctl (optional, for brightness keys)

### install

```bash
sudo make install
```

this copies `mriya` to `/usr/local/bin/mriya`.

or you can `sudo cp mriya` to `/usr/local/bin`

### run 

```startx ./mriya```

or ```startx ./mriya``` (refer to the install section above.)

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

### notes

- some distros don't have `/usr/local/bin` in `$PATH` by default.
- recompilation is required after editing `src/config.h`.

# detailed documentation

refer to [the website](https://hokum-b.github.io/mriya)

# 1.0 release goals

- [] stable with little to no crashes.
- [] gui config manager

# license

mriya is licensed under the [ISC license](LICENSE)
