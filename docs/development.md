# Development

## Tests

```sh
npm test
(cd helper && go vet ./... && go test -race ./...)
cmake -S backend -B backend/build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build backend/build
ctest --test-dir backend/build --output-on-failure
```

Use Linux for the complete helper suite: its sysfs fixtures deliberately use
Linux filenames. Native contract and version tests do not touch hardware or
require root. CI compiles the pinned SDKs on Arch and tests the Go helper on
Linux. Hardware writes are never part of the automated suite.

## Packaging

There is one supported source PKGBUILD, `packaging/PKGBUILD`. Run it from a Git
checkout. It captures that checkout's committed HEAD and builds both the Go
helper and native SDK bridge, plus their database, service, policy and licenses.
Uncommitted changes are not silently incorporated. Native dependencies are
managed by pacman; no CMake master-branch GUI downloads are used.

The old helper-only binary package is intentionally removed. A standalone Go
binary is no longer a complete installation. The release workflow publishes
complete source with checksums after the model/helper/native checks pass.
Clone the fork at that release tag and use the installer; the source archive is
for review, not a substitute for the installer's Git checkout.

Version strings in manifest.json, package.json, cli.go, backend/CMakeLists.txt,
backend/main.cpp and packaging/PKGBUILD must agree. Add release notes, commit,
then tag only when ready to publish. Nothing in this fork submits marketplace
verification issues under xyzlab's plugin ID. Listing a dedicated fork remains
an explicit maintainer action.

## Updating device support

Review upstream AWCC and alienfx-linux changes, update `upstream.lock.json`,
then run `python scripts/sync-backend.py --awcc /path/to/AWCC` and `--check`.
The database is imported byte-for-byte from the pinned Git object. The native
build checks that exact database against its fetched source. Build adapters
assert every source anchor they replace; upstream drift fails the build.

See [the backend guide](awcc.md) for capability, mapping and safety boundaries.
