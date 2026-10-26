# AWCC backend

This fork retains xyzlab's Omarchy 4 shell UI and MIT Go helper. The separately
built native bridge uses tr1xem's AWCC thermal framework and AlienFan/AlienFX SDK
under GPL-3.0. Exact commits are in `upstream.lock.json`. `database.json` is the
unmodified database from that AWCC revision, not another support list to maintain.

## What runs

QML calls the unprivileged `alienwarectl` CLI. Its existing system D-Bus service
checks Polkit, validates the request, and invokes the fixed installed bridge at
`/usr/lib/omarchy-alienware/alienware-backend`. The bridge is not setuid and cannot
be used as an unprivileged shortcut around the service. It never accepts shell
commands, executable paths, user config paths, or arbitrary ACPI calls.

The bridge builds AWCC's thermal modes and device-specific ACPI-prefix corrections
without its GUI, hotkey listener, socket daemon, or shell executor. The adapter
replaces only the transport with bounded, allowlisted direct `/proc/acpi/call` I/O.
The upstream socket protocol and UID-1000 assumption are not used.

`auto` prefers AlienFan's discovered kernel controls. If no such controls exist,
AWCC ACPI is available only for an exact DMI model in the pinned database and an
already-loaded `acpi_call` module. Unknown models never get experimental ACPI
probes. `thermal: "kernel"` or `"awcc"` pins the choice explicitly.

Incomplete database entries fail closed: the m15 Ryzen Ed. R5 currently omits
feature flags, so its legacy ACPI path is disabled rather than assuming every
feature. Kernel discovery remains independent. Aurora R11 legacy GPU boost is
also disabled because upstream writes different fan addresses than it reads;
the plugin will not guess which fan should be controlled.

Kernel boosts are 0-255 and additive. AWCC legacy boosts are 0-100; they are not
advertised as additive curves. Curves require writable fan controls, identified
CPU/GPU temperature sensors and a custom profile. Missing RPM is not zero RPM.
CPU turbo supports Intel and AMD; RAPL stays conditional on actual availability.

## Install and update

The ordinary Omarchy clone/enable/disable/remove lifecycle is unchanged. This is
a drop-in fork retaining plugin ID `xyzlab.alienware`; do not enable both copies.
After forking, point your plugin checkout's `origin` at your fork, or remove the
old plugin and add the fork URL through `omarchy plugin add`.

```sh
omarchy plugin add https://github.com/CommanderTurtle/omarchy-alienware.git --enable
cd ~/.config/omarchy/plugins/xyzlab.alienware
bash scripts/install-helper.sh
```

The installer shows the checkout commit and runs `makepkg -si` in `packaging/`.
It builds the committed local checkout, not a stale published helper release.
Review/commit local changes before building. Nothing modifies Hyprland config,
loads a kernel module, installs a hotkey daemon, adds a user to a device group,
or edits a competing service. The helper is enabled only after the explicit
installer confirmation and normal sudo prompt.

Arch build requirements are `base-devel`, `go` (at least 1.27.3), `cmake`, `ninja`,
`git`, `python`, `pkgconf` and `nlohmann-json`. Runtime libraries are `libusb`,
`hidapi`, `dbus`, `polkit` and `gcc-libs`. `makepkg` reports missing dependencies;
install them with `sudo pacman -S --needed <names>`. No GLFW, OpenGL, X11, ImGui,
libevdev or separate AWCC daemon is needed. C++ source revisions are pinned;
normal Arch-managed libraries get security updates through pacman.

For legacy ACPI only, install `acpi_call-dkms` plus the headers for your actual
kernel (`linux-headers`, `linux-lts-headers`, etc.), then explicitly load
`acpi_call`. No modules-load file is created by this plugin. For kernel support,
use the kernel's `alienware_wmi` driver. The plugin does not install/replace it.

The service refuses to start alongside known competing thermal services and
checks again before writes. It does not stop them. Manually started vendor tools
and services under other names cannot be reliably coordinated; do not run them
against the same controller. The model database is not a promise of every
feature on every listed device.

After an Omarchy plugin update, rerun the installer to rebuild the privileged
helper. Restart the helper explicitly when updating a running installation.

## Lighting

`alienwarectl backend status` enumerates SDK-recognized devices without setting
brightness, resetting USB, saving mappings, or persisting colors to firmware.
Lighting is exposed separately from thermal support: AWCC's thermal database
does not contain reliable per-key/chassis maps for every model.

Edit `/etc/omarchy-alienware/backend.json` as root, mode 0644 or stricter. Its
initial `devices` map is empty, intentionally. Add only measured/verified IDs:

```json
{
  "thermal": "auto",
  "devices": {
    "187c:0551": {
      "interface": 0,
      "regions": { "verified-region-a": [1], "verified-region-b": [2] }
    }
  }
}
```

The IDs above illustrate the format, not a universal device map. Use IDs from
your hardware's verified mapping. SDK device IDs are hexadecimal VID:PID pairs,
not enumeration positions. `interface` selects the HID interface reported by
status. Without it, a pair recognized on multiple interfaces is inventory-only:
no lighting write is permitted. Duplicate identical physical devices remain
unsupported if this selection cannot uniquely identify them. The Linux hidraw
transport avoids libusb's kernel-driver detachment path; libusb is used only
to read USB descriptors. The panel lists every discovered device and allows
coloring all its mapped regions/keys. Existing x15 R2 chassis/key diagrams are
only exposed for that model and a matching explicit map. Other keyboard sizes
are never treated as the x15 R2. A `keys` object maps UI indices to LED-ID arrays;
only `keyLayout: "x15-r2"` on the x15 R2 with every measured wire index mapped
enables the positional keyboard diagram. `docs/KEYMAP.txt` records those indices;
partial maps still support generic device color without claiming a complete
keyboard layout. API2/3 light IDs are limited to 0-23 and API6 to 0-7 because
those protocols use bit masks. Other supported SDK APIs accept 0-255. Standalone
brightness is disabled on API6/7, whose SDK brightness state is process-local.

```sh
alienwarectl backend color 187c:0551 7AA2F7
alienwarectl rgb set verified-region-a FF8800
alienwarectl rgb off
omarchy bar set xyzlab.alienware themeRgb true
omarchy bar set xyzlab.alienware muteSync true
```

Theme/mute/battery integration is opt-in. Fresh installs do not restore lights
until there is saved user lighting state. USB reset, zone probing and firmware
save commands are deliberately not exposed through the unprivileged API. Generic
effects and arbitrary positional keyboard editors are not claimed by this pass.

## Safety and removal

Every native request has a four-second deadline and a 256 KiB output ceiling.
There is one bounded helper gate, a cross-process lock, and no automatic retry
of a timed-out hardware write. QML remains outside native hardware code.
Only boosts set by this helper are recorded in its root-owned runtime directory
and reset on shutdown. Removing/disabling the plugin stops a curve if it was
active; removing the helper stops its service. Read-only polling never changes
profiles, turbo, power limits, brightness or fan boost.

```sh
# Optional: explicitly blank lights before removing their controller.
alienwarectl rgb off
alienwarectl kbd off

omarchy plugin disable xyzlab.alienware
sudo systemctl disable --now alienwarectl.service
sudo pacman -Rns omarchy-alienware
omarchy plugin remove xyzlab.alienware
```

No helper-owned user service, udev permission rule or kernel module configuration
remains. Polkit, D-Bus, systemd and native files belong to the pacman package.
User settings and pacman's `.pacsave` for a modified `backend.json` are preserved
for recovery, not deleted automatically. Shared dependencies are not forcibly
removed. The original 0.3.x udev rule is removed when its old package is replaced;
if migrating from it, reload udev rules yourself or reboot.

## Reproducibility and audit

To update backend support, review upstream changes, update `upstream.lock.json`,
and synchronize the exact database with
`python scripts/sync-backend.py --awcc /path/to/AWCC`. `--check` verifies without
writing. The build refuses a database that differs from the pinned source.
No runtime git pull or network polling is done by the plugin.

Build adapters also reject unknown firmware thermal modes instead of reporting
Manual, and move the AlienFX unknown-device check before its report-ID array
lookup. These small build-time patches assert their expected source anchors.
HID transport errors now fail instead of converting negative error codes to
success; packet bounds and status report initialization are checked as well.
Go dependencies were updated to godbus 5.2.4 and x/sys 0.49.0. Use current
Go 1.27 security patches for releases. Go 1.27.3's govulncheck audit on 2026-10-26
reported no known vulnerabilities in the helper. That does not certify native
firmware code or prove the absence of vulnerabilities. Hardware-free tests and a Linux native
build belong in CI; real fan/lighting behavior still needs model-specific
verification, including suspend, service shutdown and uninstall.
