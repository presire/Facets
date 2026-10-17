# Facets

**English** | [日本語](README_JP.md)

A virtual desktop switching animation for KDE Plasma 6.  
Instead of a plain slide, your desktops move through a 3D scene when you switch between them.  

## What it is

When you change virtual desktops, KWin normally slides the old desktop out and the new one in.  
Facets replaces that with a short 3D transition.  

It runs as a KWin desktop effect, so it works everywhere you switch desktops:  
keyboard shortcuts, the pager, and touchpad gestures.  

## The three styles

You can pick the transition that suits you.  

- **Cube**  
  your desktops sit on the faces of a rotating 3D cube.  
  Switching turns the cube to the next face.  
- **Flip**  
  the current desktop flips away like a card turning over, revealing the next one.  
- **Deck**  
  the old desktop sinks back and the new one slides in from the front, like pulling a card off a deck.  

## Features

- Follows your system animation speed by default, so it feels consistent with the rest of Plasma.  
- Works with touchpad gestures, keyboard shortcuts, and the desktop pager.  
- Option to keep panels, docks, and on-screen displays fixed while the desktop moves behind them.  
- Background dimming behind the 3D scene.  
- Adjustable zoom-out and tilt for the camera.  
- Directional shading of the faces turned away from the viewer.  
- Smooth animation that syncs with the display refresh rate.  
  (C++ version measured on X11 with a 4K 60 Hz display: 60 fps, the same as the stock slide effect)  
- Works on both X11 and Wayland.  

## Two versions

There are two ways to install Facets.  
Both offer Cube, Flip, and Deck styles, but their motion and rendering differ,  
along with how smooth they feel and what you need to build them.  

| | C++ version | QML version |
| --- | --- | --- |
| Quality | Smooth, synced to the display refresh rate<br>(C++ version measured on X11 with a 4K 60 Hz display:<br>60 fps, the same as the stock slide effect) | Performance depends on the environment;<br>roughly 30 fps in the tested setup |
| Install | Built from source with a compiler | Simple copy, no compiler needed |
| Recommended | Yes | For a quick try |

Both versions use the same effect id, so only one of them can be installed at a time.  
If you try to install the C++ version while the QML version is present, they would collide.  

The C++ installer warns you about this and can move the old QML version out of the way. (see below)  

## Requirements

- KDE Plasma 6.x with KWin, using OpenGL compositing.  
- For the QML version:
  no compiler is needed, but the Qt Quick 3D and Kirigami runtime modules must be installed.  
- For the C++ version, in addition:  
  - CMake 3.16 or newer and a C++20 compiler.  
  - Qt 6.6 or newer, KDE Frameworks (KF6) 6.0 or newer, and the ECM (KDE Extra CMake Modules) development package.  
  - Development packages matching the running KWin. (6.4 or newer)  

Package names differ between distributions.  
On openSUSE the KWin development package is named in the `kwin6-devel` style;  
other distributions use their own names for the same thing.  

Look for the development packages for KWin, Qt 6, and KF6.  

On openSUSE Leap 16.0, installing the following packages is enough.  
The packages they depend on are pulled in automatically.  
(Qt Quick is also required, because KWin itself depends on it.)  

```
sudo zypper install gcc-c++ cmake kwin6-devel kwin6-x11-devel qt6-quick-devel \
  kf6-kcmutils-devel kf6-kconfigwidgets-devel kf6-ki18n-devel kf6-kwindowsystem-devel \
  kf6-kcoreaddons-devel kf6-kconfig-devel kf6-kwidgetsaddons-devel
```

`kwin6-x11-devel` is only needed for the X11 session, and `kwin6-devel` only for the Wayland session.  
The installer builds whichever ones it can and skips the other.  

On Fedora 44:  

```
sudo dnf install gcc-c++ cmake extra-cmake-modules qt6-qtbase-devel qt6-qtdeclarative-devel \
  kf6-kconfig-devel kf6-kcoreaddons-devel kf6-ki18n-devel kf6-kcmutils-devel \
  kf6-kwidgetsaddons-devel kf6-kconfigwidgets-devel kf6-kwindowsystem-devel \
  kdecoration-devel libepoxy-devel libdrm-devel wayland-devel kwin-devel kwin-x11-devel
```

On Debian:  

```
sudo apt install build-essential cmake extra-cmake-modules qt6-base-dev qt6-declarative-dev \
  libkf6config-dev libkf6coreaddons-dev libkf6i18n-dev libkf6kcmutils-dev \
  libkf6widgetsaddons-dev libkf6configwidgets-dev libkf6windowsystem-dev \
  libkdecorations3-dev libepoxy-dev libdrm-dev libwayland-dev kwin-dev kwin-x11-dev
```

Debian 13 (trixie) ships KWin 6.3.6, which is older than the required 6.4,  
and it has no `kwin-x11-dev` package.  
The build therefore cannot work there, and the list above is for Debian 14 (forky) or newer, which ships KWin 6.7.  
On Fedora, `kwin-x11-devel` is only needed for the X11 session, just like on openSUSE.  

Facets's C++ version has been tested on openSUSE Leap 16.0 with Plasma 6.4.2, on both X11 and Wayland.  

One important note: a compiled KWin effect must be built against the same KWin version that is running.  
After a Plasma update that changes KWin, rebuild and reinstall the C++ version.  

## Installation

### QML version (quick and simple)

Download or clone this repository, then from the project folder run:  

```sh
./install.sh
```

This copies the effect into your home folder, by default `~/.local/share/kwin/effects/facets/`.  
No root rights are needed.  

### C++ version (recommended)

Download or clone this repository, then from the project folder run:  

```sh
./cpp/install.sh
```

The script builds and installs two variants:  
one for Wayland and one for X11, placed under `~/.local/lib64/qt6/plugins/`.  

No root rights are needed. If one variant cannot be built,  
for example because a development package is missing,  
the installation still finishes with the one that succeeded;  
only when both fail does the script stop.  
It also writes a small startup script, `~/.config/plasma-workspace/env/facets.sh`,  
that makes KWin find the plugin after your next login.  
`~/.local/lib64` is not one of the places Qt searches by default,  
so the script adds `~/.local/lib64/qt6/plugins` to the `QT_PLUGIN_PATH` environment variable.  
Until you log in again, KWin cannot find the effect.  

Options:  

```sh
./cpp/install.sh --remove-legacy   # also move an old QML install aside
./cpp/install.sh --uninstall       # remove the installed files and settings
./cpp/install.sh --help            # show usage
```

About `--remove-legacy`:  
if an older QML version is still installed, it uses the same effect id as the C++ version and they conflict.  

A plain install only warns you.  
Running with `--remove-legacy` moves the old QML version into a backup folder in your user data folder,  
normally `~/.local/share/facets-qml-backup-<timestamp>`,  
instead of deleting it, so you can restore it if you want.  

### Logging in again

For the C++ version, log out and log back in once.  
This login is what activates the plugin search path.  
You don't need to reboot.  

The QML version does not need a re-login.  
The installer reloads the effect if it is already active; otherwise, select Facets in System Settings.  

## How to enable it

1. Open System Settings.
2. Go to **Workspace Behavior > Desktop Effects**.  
   (in some Plasma versions this page is called **Animations**)  
3. In the **Virtual desktop switching** section, select **Facets**.  

Only one animation can be active at a time.  
Choosing Facets replaces whatever was selected before, and choosing a different animation turns Facets off.  

## Changing the settings

On the same page, click the configure button (the gear icon) next to Facets.  

The labels below are shown by the C++ version;  
where the QML version uses a different label, it is noted. You can change:  

- **Style**  
  the transition to use: Cube, Flip, or Deck. Default: Cube.  
- **Duration**  
  how long the animation lasts, in milliseconds.  
  `0` means Automatic, which follows your global animation speed.  
  Range 0 to 3000. Default: Automatic.  
- **Zoom out**  
  how far the camera pulls back at the middle of the animation, as a percentage.  
  Larger values show more of the 3D scene. Range 0 to 150. Default: 40.  
- **Tilt angle**  
  the peak tilt of the 3D scene, in degrees. In the QML version this setting is called **Tilt**.  
  Range 0 to 30.  
  Default: 10.  
- **Background dimming**  
  how dark the wallpaper behind the 3D scene appears, as a percentage.  
  Range 0 to 100.  
  Default: 65.  
- **Shade faces turned away from the viewer**  
  darkens the desktop faces that point away from you,  
  which adds depth. In the QML version this setting is called **Darken faces turning away**.  
  Default: on.  
- **Keep panels and on-screen displays fixed**  
  keeps panels, docks, and on-screen displays in place instead of moving them with the desktop.  
  In the QML version this setting is called **Keep panels and notifications in place**.  
  Default: on.  

## Uninstall

For the QML version, from the project folder:  

```sh
./install.sh --uninstall
```

For the C++ version:  

```sh
./cpp/install.sh --uninstall
```

Both remove the installed effect files.  
The QML uninstall removes `~/.local/share/kwin/effects/facets/` and tries to clear the enabled state.  

The C++ uninstall removes the files under `~/.local/lib64/qt6/plugins/`,  
deletes the startup script `~/.config/plasma-workspace/env/facets.sh`,  
and clears the enabled state and the effect settings when the KDE configuration tool is available.  

After uninstalling the C++ version, log out and back in once to finish.  
The QML version does not need a re-login.  

## Troubleshooting and FAQ

**The effect doesn't appear in the list.**  
If you installed the C++ version, make sure you logged out and back in once afterwards.  
The C++ version relies on a startup script that sets the plugin path, and that only takes effect at login.  

If it still doesn't show up,  
check that the startup script `~/.config/plasma-workspace/env/facets.sh` exists and is being read by your Plasma session.  

**It worked before, but a Plasma update broke it.**  
A compiled KWin effect has to match the KWin version that's running.  
After a Plasma update, run the C++ install again to rebuild against the new version.  

**The animation is too fast or too slow.**  
Open the settings and change **Duration**.  
Set it to Automatic to follow your global animation speed, or enter a value in milliseconds to set it yourself.  

**Only one animation is active.**  
That's expected.  
KWin allows just one virtual desktop switching animation at a time,  
so enabling Facets disables the previous one, and vice versa.  

**Can I keep both versions installed?**  
Do not keep both:  
they share the same effect identifier and conflict.  
The installer only warns and continues.  

Uninstall one before installing the other,  
or use `--remove-legacy` when installing the C++ version to move an old QML install aside.  

**Touchpad gestures don't switch desktops.**  
Facets only changes how the switch looks.  
The gesture itself is handled by Plasma.  
Check your touchpad gesture settings under Workspace Behavior in System Settings.  

## License

Facets is free software, released under the **GPL-2.0-or-later** license.  
See the source files for the license headers.  
