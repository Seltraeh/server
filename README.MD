# Decompfrontier Server
This is an unofficial, **EXPERIMENTAL** C++ game server that aims to emulate the official server used for Brave Frontier (v2.19.6.0)

The source code will eventually be part of a "Brave Frontier Offline Mod" release started
in late 2022 which took inspiration from the original GimuFrontier C# code made back in 2022 as a PoC to demonstrate
the feasibility of a server emulator. 

For more information about the strategy of the offline mod/purpose of this server, please check [How the offline mod works](https://decompfrontier.github.io/pages/Architecture/Offlinemod-architecture.html) and [Emulator (Server) architecture](https://decompfrontier.github.io/pages/Architecture/Emulator-architecture.html)

## Portable Windows release
The standalone server ZIP and the game-content downloads are published as releases of this repository. Players run `Start Server.bat`; they do not need the packet-generator or a development toolchain.

## Requirements
- Visual Studio 2022 (Windows)
- Ninja, GCC/Clang (Linux)
- CMake 4.0+
- Rust 1.87+ (build-time packet generator)
- Initialize the `packet-generator` Git submodule (`git submodule update --init --recursive`)
- vcpkg

## Building
On Windows, `rebuild.bat` (which runs `Build-Dev.ps1`) configures CMake and builds Debug; generated Ninja files do
not need to be copied from another PC. Pass `-VcpkgRoot <path>` (or set `VCPKG_ROOT`), or `-BootstrapVcpkg` to
install a local vcpkg. The first build creates `deploy/config.json` from the `config.json` template.

## Legal
The code of this project is released under the Affero GNU Public License v3.0+.

This project is not associated, affiliated or sponsored with/by Alim Co., Ltd or gumi Inc.
