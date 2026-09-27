# Building from a fresh checkout

The supported development path is the standalone Windows x64 Debug server.
The APPX proxy is a different target. A compiler build does not require assets.zip,
a client installation, an existing save, or the maintainer's ignored tools.

## Windows prerequisites

Install Visual Studio or Build Tools with **Desktop development with C++**, an
x64 MSVC toolchain, a Windows SDK, and **C++ CMake tools for Windows** (includes
Ninja). Install Git, CMake **4.0+**, and Rust's stable **MSVC** toolchain. The
generator declares Rust 1.87 as its minimum; use current stable for its lockfile.

These commands install the separate command-line tools if needed:

```powershell
winget install --exact --id Git.Git
winget install --exact --id Kitware.CMake
winget install --exact --id Rustlang.Rustup
```

Reopen PowerShell after installation. Visual Studio's workload is configured in
Visual Studio Installer. See the official [Rust Windows setup](https://learn.microsoft.com/en-us/windows/dev-environment/rust/setup)
and [vcpkg/CMake setup](https://learn.microsoft.com/en-us/vcpkg/get_started/get-started-vs).

## First build

```powershell
git clone --recurse-submodules https://github.com/Seltraeh/server.git
cd server
.\rebuild.bat -BootstrapVcpkg -VcpkgRoot .tools\vcpkg
```

This locates Visual Studio, imports the x64 environment, finds Ninja, bootstraps
a repository-local vcpkg at the manifest registry baseline, configures CMake,
generates both C++ schema headers using Cargo's lockfile, and builds Debug.
It defaults to two compiler jobs for laptops. Downloads/dependency compilation
make the first build substantially slower than later builds.

To use an existing vcpkg checkout instead:

```powershell
.\rebuild.bat -VcpkgRoot C:\dev\vcpkg -Jobs 2
```

No desktop-specific path is embedded in the script. An explicitly set but invalid
VCPKG_ROOT produces an actionable error; pass -VcpkgRoot to override it.
The script creates deploy/config.json from packaging/config.json only if absent.
It never starts a server, deletes build directories, overwrites a save, or replaces
an existing local configuration.

## After pulling changes

Finish or preserve your local submodule edits before updating its revision:

```powershell
git pull
git submodule update --init --recursive
.\rebuild.bat -VcpkgRoot .tools\vcpkg
```

The initialized submodule is not silently switched by the build script. The
parent repository records its required commit. Schema changes must be published
in packet-generator first, followed by the parent submodule pointer.

Generated build.ninja, build-Debug.ninja, CMakeCache.txt, PCH files, and generated
packet headers are intentionally untracked. CMake's configure step creates the
build files for the local machine; `cmake --build` alone cannot create an absent
build tree. Shared [presets](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html)
and source CMakeLists.txt files are the portable inputs.

## Run and debug

```powershell
& .\out\build\debug-win64\standalone_frontend\Debug\gimuserverw.exe
```

Debug builds default to deploy/config.json. All config-relative paths resolve
from the configuration directory. The server listens on 127.0.0.1:9960 by default.
Running it creates/migrates a save; compilation alone does not. Extract game
assets into deploy/game_content for client play. The optional developer console
is included in source and enabled in development builds. Portable release
presets disable it with GIMU_ENABLE_DEBUG_CLI=OFF.

After the first build, open the repository folder in Visual Studio, select
debug-win64 and Debug, choose gimuserverw Debug as the startup target, and press
F5. Build-Dev.ps1 installs the shared launch.vs.json into .vs only if absent;
existing personal debugger settings are preserved. The launch target includes
the Ninja Multi-Config Debug/Release subdirectory. Direct CMake configuration
also creates deploy/config.json if absent, so F5 does not depend on a private
configuration file. If you have an older .vs/launch.vs.json, update its target
path using the shared file. Command-line users can use these equivalent commands from an x64 Native
Tools shell with VCPKG_ROOT set:

```powershell
cmake --preset debug-win64
cmake --build --preset debug-win64-debug --target gimuserverw --parallel 2
```

Verify symbols, fresh-save startup and the developer console without game assets:

```powershell
python scripts/test_debug_startup.py out/build/debug-win64/standalone_frontend/Debug/gimuserverw.exe
```

This uses a temporary save under out and terminates only its own test process.

## Troubleshooting

| Failure | Recovery |
|---|---|
| Missing build.ninja / build-Debug.ninja | Run rebuild.bat; it always configures before building. |
| Missing packaging/Install.cmake | Incomplete source checkout; pull the shared packaging directory. |
| Missing packet-generator runtime or Cargo.lock | Initialize submodules; do not download GitHub's source ZIP alone. |
| Missing unitSelectorGacha/eventTokenMst members | Pull the portable ServerCache sources; ignored local fragments are no longer required. |
| CMake compiler/Ninja not found | Install the C++ workload/CMake tools, then use rebuild.bat. |
| Cache points at a different laptop/path/compiler | Use rebuild.bat -Fresh with the correct -VcpkgRoot, or choose a new -BuildDirectory. |
| EXE cannot be linked | Stop the specific server running that EXE, or build to another directory. |
| New schema member absent despite regenerated header | Inspect the compiler's header path; use a fresh build directory to rule out a stale PCH. |
| Out of memory / compiler killed | Use -Jobs 1 or 2. |

`-ConfigureOnly` validates configuration without compiling. `-BuildDirectory`
supports a separate output tree; `-InstalledDependencies` can reuse an existing
vcpkg dependency tree with the same toolchain/triplet for local validation.
Avoid simultaneous builds generating headers in the same source checkout.
For an isolated configuration, use relative paths or forward slashes in the
SQLite filename; backslashes can be interpreted by the database connection parser.

## Linux

The existing debug-lnx64 preset remains available. Install CMake 4+, Ninja,
Rust, vcpkg, and a C++23-capable compiler; initialize submodules, set VCPKG_ROOT,
then run `cmake --preset debug-lnx64` and `cmake --build --preset debug-lnx64`.
This development repair is validated on Windows; it is not a Linux/APPX build
certification.

## Maintainer release check

Windows Debug CI builds a recursive checkout without private files. Before
publishing, verify that all new build/runtime inputs are committed, including
packaging, scripts, authored JSON and the matching packet-generator revision.
Do not add build outputs or live saves to fix missing source files.

## Validation performed on 2026-09-25

Exported tracked source plus the pending shared files into a separate directory,
excluding ignored ServerCacheLocal fragments, DebugCli, generated headers, old
build files and game assets. With MSVC 19.51, a new CMake/Ninja tree and a fresh
Cargo target directory, the standalone Debug build completed successfully.
The vcpkg installation/binary cache was reused; this was not a blank Windows VM.
The resulting executable answered HTTP on port 19961 and migrated a new isolated
SQLite save. The live server/save were not used for this startup test.

The final Build-Dev.ps1 also passed a separate configure-only run. The CI workflow
has been added locally, but its hosted run awaits publication of the changes.

## Packet schema changes and precompiled headers

KDL edits regenerate packet headers. The server PCH and server/frontend objects
have explicit dependencies on those headers so a schema change cannot leave the
old packet layout cached in a Debug build. This addresses an observed MSVC/Ninja
incremental-build failure where new packet types existed in all.hpp but were
unknown to PCH-using handlers. Do not copy PCH or Ninja files between machines.

## Validation performed on 2026-09-27

Exported only the staged parent/submodule source into a new directory with no
existing build tree, generated headers, Cargo target directory or game assets.
The complete 205-step Windows Debug build passed with the shared console enabled.
The installed vcpkg dependency tree was reused; dependencies were not rebuilt
on a blank Windows machine.

The new executable passed test_debug_startup.py: PDB present, HTTP ready, new
SQLite schema migrated and named-pipe developer console connected. The five
reported addunit IDs, duplicate grants, dictionary/counter updates, fresh BB/SBB
defaults and a forced rollback passed using test_debug_cli.py on a copied save.
A no-argument launch from C:/Windows/System32 also found the checkout's default
configuration and relative runtime data. Missing-config regeneration passed.
The tests did not modify the live save or launch the interactive VS debugger.

Packet-generator tests passed (42 library, 2 CLI and 2 doctests). The Research
Lab AI model reported zero failures, authored data passed regeneration --check,
and the handler registration audit found zero structural errors. Hosted CI
will independently exercise the committed recursive checkout after publication.
