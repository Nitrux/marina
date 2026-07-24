# Marina | [![License](https://img.shields.io/badge/License-BSD_3--Clause-blue.svg)](https://opensource.org/licenses/BSD-3-Clause)

Marina is a QML-based workspace dock designed for Nitrux. Built with MauiKit and LayerShell-Qt.

## Features

- A centered, translucent bottom dock rendered with QML and MauiKit.
- LayerShell-Qt placement with an exclusive zone that keeps workspaces clear.
- Fullscreen-aware visibility, with above-fullscreen placement available as an opt-in.
- Pinned launchers discovered from standard XDG desktop entries.
- Live running-application, active-window, and window-count indicators on Hyprland.
- Event-driven Hyprland updates with a coalesced refresh and periodic recovery fallback.
- All Hyprland clients from every workspace, including unmapped clients on inactive workspaces, grouped by application with cross-workspace counts and click-to-cycle activation.
- Drag-to-reorder persistence for pinned applications.
- Horizontal scrolling when launchers exceed the available output width.
- Repeated clicks cycle through an application's open windows.
- Optional edge-triggered auto-hide with a configurable delay.
- Click to focus or launch, middle-click to open a new instance, and right-click to pin or unpin.
- One dock per output by default, with active-output placement available through configuration.

## Requirements

- Nitrux 7.0.0 and newer.

### Runtime Requirements

```
mauikit (>= 4.0.4)
qt6 (>= 6.9.2)
layer-shell-qt (>= 6.6.4)
```

### Build Requirements

```
cmake (>= 3.21)
extra-cmake-modules
kcoreaddons (>= 6.13.0)
ki18n (>= 6.13.0)
qt6-base-dev (>= 6.9.2)
qt6-declarative-dev (>= 6.9.2)
```

## Building

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build
```

## Configuration

Marina creates `~/.config/marina/marina.conf` on first launch.

```ini
[Appearance]
edgeMargin=8
iconSize=48

[Behavior]
autoHide=false
autoHideDelay=650

[Launchers]
pinned=org.kde.index, org.kde.fiery, org.kde.vvave

[Window]
height=0
screenPlacement=all
showAboveFullscreen=false
width=0
```

`screenPlacement` accepts `all` or `active`. Marina watches the configuration
file and applies saved changes while it is running. With
`showAboveFullscreen=false`, Marina unmaps while the focused client is
fullscreen. Setting it to `true` selects the LayerShell overlay layer and keeps
the dock available above fullscreen clients.

`height=0` derives the dock height from `iconSize`; a positive value requests
an explicit height and is clamped between `iconSize + 8` and 256 pixels.
`width=0` keeps the dock content-sized. A positive width sets its minimum width
and is clamped between 96 and 4096 pixels; the actual surface is additionally
capped to the available output width. If the launcher row is wider than that
surface, it scrolls horizontally and launcher icons retain their configured
size.

# Licensing

The license for this repository and its contents is **BSD-3-Clause**.

# Issues

If you find problems with the contents of this repository, please create an issue and use the **🐞 Bug report** template.

## Submitting a bug report

Before submitting a bug, you should look at the [existing bug reports](https://github.com/Nitrux/marina/issues) to verify that no one has reported the bug already.

©2026 Nitrux Latinoamericana S.C.
