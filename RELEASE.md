# Build the Windows standalone release

Players do not need the packet-generator, Rust, CMake, vcpkg or Visual Studio.
Those are build dependencies; the ZIP contains generated/compiled packet code,
the server's DLLs, the MSVC runtime, `archive/`, `mst/`, `system/` and a portable
HTTP configuration. The patched x86 APPX client connects to the x64 server over
`127.0.0.1:9960`. Use the standalone/no-SSL proxy, not an embedded server build.

## Build machine

- Windows x64; Visual Studio/Build Tools with Desktop development with C++.
- CMake **4.0+**, Ninja, Git, Rust **1.87+**, bootstrapped vcpkg.
- Clone this fork with `git clone --recurse-submodules ...`, or initialize an
  existing checkout with `git submodule sync --recursive` followed by
  `git submodule update --init --recursive`.
- Supply a populated game-content folder. Generated MST downloads, if used,
  belong inside that content tree together with their manifests and parts.

Run from the checkout (an ordinary terminal works):

```powershell
.\rebuild_release.bat -VcpkgRoot C:\path\to\vcpkg
```

The script finds the C++ toolchain, always configures the isolated
`standalone-release-win64` Release preset, regenerates packet/archive headers
when schemas or generator inputs change, builds, installs and packages.
It does not rebuild/restart the running Debug server. The optional developer
console is excluded from the release build. MSVC 19.51 hits C4737 in Mission, Present and
UserInfo coroutines with optimization enabled; those three files use /Od and skip the shared precompiled header while the
rest of Release remains optimized. Recheck this workaround when upgrading MSVC.

Defaults: content from `deploy/game_content`, output in `out/releases/`, two
compiler jobs. Override with `-ContentRoot`, `-OutputRoot` and `-Jobs`.
`-InstalledDependencies` may point at an existing compatible vcpkg installed
directory; otherwise the new build tree manages its own dependencies. Avoid
sharing that directory with a concurrent Debug build: dependency reinstalls can
invalidate headers and force a full recompilation.

Each invocation creates a NEW output directory with:

- `server.zip`: portable server, launch/check scripts, data and license.
- `game-content-001.zip`, `002`, etc.: separate asset downloads. Each independent
  ZIP extracts into the SAME server directory and contains a `game_content/`
  prefix. Parts use at most 1 GiB of uncompressed input to stay below GitHub's
  [2 GiB asset limit](https://docs.github.com/en/repositories/releasing-projects-on-github/about-releases#storage-and-bandwidth-quotas). No proprietary client executable is included.
- `SHA256SUMS.txt`: checksums of all ZIPs.
- `server/`: staging directory; publish the ZIP, not a test-mutated folder.

`-ContentMode None` omits content ZIP creation for a server-only update. It still
records the required content inventory and requires the source assets to exist.
Only reuse older content downloads if `Check-Package.ps1` passes with them;
new MST files or authored asset changes may require new content downloads.

The release config is `packaging/config.json`, never your live deploy config.
Only JSON data is copied from archive/MST/system. SQLite saves/backups, logs,
debug symbols, build tools, packet-generator sources and upload directories are
not copied. Starter resources are provisioned by the server's CreateUser handler; obsolete
initial_* config fields are intentionally omitted because this tree does not read them.

## Verify the actual extracted ZIPs

Extract server.zip and every matching content ZIP into a fresh writable folder
with spaces in its name. Then run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File packaging\Test-Release.ps1 `
  -PackageDirectory 'C:\Temp\BF Release Test'
```

This requires NO existing save and creates a disposable one. It uses a separate
port, removes developer directories from PATH, launches the server from a foreign
working directory without a config argument, and checks migrations/guest login,
encrypted Initialize, features, proxy FPS configuration and downloaded asset
bytes. All flat and staged MST download parts are compared byte-for-byte, and the
Initialize MST version baseline is checked. It stops its own
process and restores the original config. It also restarts the server and verifies
that the new account and inventory persist. A protocol smoke test is not a full
APPX playthrough: also test the separately distributed patched client on a clean
Windows account/VM, including loopback setup, first login, starter selection,
mission start/finish and relaunch. Test older saves on COPIES before claiming
compatibility with a previous release.

## Publish

Commit/push the packet-generator changes to the fork FIRST, then commit the
server's submodule pointer and server changes. Verify the pinned submodule commit
is publicly reachable. `.gitmodules` points at Seltraeh's generator fork because
this server depends on its custom KDL. A GitHub source ZIP does not include
submodule contents; source builders must clone recursively.

Dirty local builds are allowed for testing and explicitly identified in
`release-build.json`. They are not reproducible from the recorded HEAD alone.
Publish matching source revisions (including runtime data and custom KDL) before
calling the binary a tagged reproducible release.
Required selector, mission and event-token caches now live in the tracked
ServerCache sources. Historical `ServerCacheLocal*.inl` files are no longer
included. Fresh source builds do not need those ignored maintainer files.


Upload `server.zip`, all matching content ZIPs and `SHA256SUMS.txt` to a new GitHub
release. Include the patched-client download/setup link and source revision.
The builder refuses to package if source/data inputs change during compilation,
and records their hashes in source-inputs.json. Use a fixed checkout when other
agents are developing concurrently. It never uploads, tags, commits, resets
submodules or copies a live save.
The bundled README explains extraction, loopback, launch and save migration.
