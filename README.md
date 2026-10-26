# Alienware Control

An Omarchy 4 shell plugin for Alienware and Dell G-series fans, lighting and power.
This fork keeps xyzlab's themed panel and integrates pinned tr1xem AWCC and
AlienFan/AlienFX backends. Read [the backend guide](docs/awcc.md) for capabilities,
device mapping, dependencies, safety and migration.

Two parts: a Quickshell plugin that runs unprivileged, and `alienwarectl`, a Go binary that is both
a root D-Bus daemon and a user-side CLI.

- Fan speeds, temperatures, GPU draw and power limits in the bar
- Thermal profiles, CPU turbo, RAPL limits and an additive fan curve
- Chassis RGB, four regions, and per key keyboard lighting
- Follows the Omarchy theme, shows mute state on the mute keys, and restores itself after a suspend

The original UI was tested on an x15 R2. This fork's wider support follows the
AWCC database and detected kernel/SDK capabilities, not a claim that every
feature has been tested on every device. Positional lighting maps remain
model-specific; unsupported controls are not guessed.

## Install

The plugin and the helper install separately.

```
omarchy plugin add https://github.com/CommanderTurtle/omarchy-alienware.git --enable

cd ~/.config/omarchy/plugins/xyzlab.alienware
bash scripts/install-helper.sh
```

Or open the panel and press "Install the helper", which runs the same commands in a terminal so
you can see them before they run.

The installer displays its commands and checkout commit, then builds the committed
local checkout through `packaging/PKGBUILD`. It does not fetch the old upstream
prebuilt helper. Build dependencies and optional ACPI setup are documented in the
backend guide. It refuses to start over a known active competing fan service.

The plugin must be a real directory. The shell's inotify watcher does not follow symlinks, which is
why the package does not ship the plugin itself.

## Update

```
omarchy plugin update xyzlab.alienware

cd ~/.config/omarchy/plugins/xyzlab.alienware
bash scripts/install-helper.sh
sudo systemctl restart alienwarectl.service
```

The QML side hot-reloads on save, so a plugin update needs nothing more than `omarchy restart shell`.
The helper does not. Rebuild after updates; uncommitted changes are not included.
Check what is actually running with `alienwarectl version`, `alienwarectl status`
and `alienwarectl backend status`.

## Uninstall

Optionally blank mapped lighting before removing its controller. No lighting
change is imposed by uninstall. Stop/disable the plugin and helper, then remove
the package and plugin through their normal managers.

```
alienwarectl kbd off
alienwarectl rgb off

omarchy plugin disable xyzlab.alienware
sudo systemctl disable --now alienwarectl.service
sudo pacman -Rns omarchy-alienware

omarchy plugin remove xyzlab.alienware
```

This fork installs no device-permission udev rule or kernel module configuration.
Your saved colours and settings stay behind in
`~/.local/state/omarchy/alienware/state.json`.

## Keybind

Add to `~/.config/hypr/bindings.lua`:

```lua
o.bind("SUPER SHIFT", "A", "Alienware", "omarchy-shell shell summon xyzlab.alienware '{}'")
```

## If the panel will not close

It takes exclusive keyboard focus while it is open, so a wedged panel can look like a wedged
machine. It is not. Either of these closes it, and neither needs the keyboard focus back:

```
omarchy-shell alienware close
omarchy-shell shell hide xyzlab.alienware
```

Clicking anywhere outside the panel closes it too, and `esc` closes it unless a text field has
focus, in which case the first `esc` leaves the field and the second closes the panel.

## Docs

| | |
|---|---|
| [AWCC integration](docs/awcc.md) | Current backend architecture, setup, device support and audit |
| [Roadmap](docs/roadmap.md) | What works and what is planned |
| [Hardware](docs/hardware.md) | The two lighting controllers, the region map, the privilege model |
| [Behaviour](docs/behaviour.md) | Suspend and resume, mute indicator, thermal profiles, G-Mode |
| [CLI](docs/cli.md) | Every `alienwarectl` verb |
| [Key map](docs/KEYMAP.txt) | The measured keyboard index map |
| [Development](docs/development.md) | Tests and releases |
