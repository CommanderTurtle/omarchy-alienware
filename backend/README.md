# Native backend

The bridge and its build adapters are GPL-3.0-only (see LICENSE). The original
Omarchy QML and Go program retain their MIT license; they communicate with this
separate executable over a bounded JSON request, not a linked Go library.

Pinned upstream AWCC, AlienFan and AlienFX code is fetched only at build time.
The build uses distro-managed libusb/hidapi/nlohmann-json and pinned loguru,
without invoking any upstream GUI/dependency-fetching build. See docs/awcc.md
at the project root for API boundaries, transport adapters and limitations.
