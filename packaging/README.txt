BRAVE FRONTIER - PORTABLE STANDALONE SERVER (Windows x64)

1. Extract server.zip into a writable folder, for example C:\Games\BF Server.
2. Extract ALL game-content-*.zip files from the SAME release into that folder.
   Merge the folders. You should have config.json, gimuserverw.exe, archive,
   mst, system and game_content beside Start Server.bat. Do not run inside a ZIP.
3. Install the separately provided patched Brave Frontier 2.19.6.0 APPX client.
   It must target HTTP 127.0.0.1:9960 (the no-SSL standalone proxy build).
   An embedded-server proxy or an HTTPS/different-port patch is not interchangeable.
4. Enable Windows loopback access once for the installed client. From an
   administrator PowerShell window, run:
     powershell -NoProfile -ExecutionPolicy Bypass -File ".\Enable Client Loopback.ps1"
   This selects the package family of your installed gumi.BraveFrontier client.
5. Double-click Start Server.bat; leave it open, then launch the patched game.
   Normal play does not require administrator rights, CMake, Rust, Git or VS.

The server includes its application DLLs and Visual C++ runtime DLLs. It is x64;
the patched game remains x86 and communicates with it over HTTP. Windows 10/11
x64 is the intended platform. This bundle does not install or modify the client.

SAVES AND UPDATES
gme.sqlite is created locally on first launch. No developer save is included.
Before upgrading, stop the game and server and back up the whole server folder.
Extract a new release into a NEW folder, add its matching content ZIPs, then copy
your stopped server's gme.sqlite into it. If -wal/-shm files remain, keep the whole
save set together. Never overwrite an active SQLite save. Use the new config.json
and reapply only intentional settings. Do not delete the client's LocalState.
The server migrates its SQLite schema at startup; very old saves may need separate
migration testing. The release builder does not certify every historical save.

TROUBLESHOOTING
- Missing assets: extract every game-content ZIP; do not nest an extra server folder.
- Port already in use: close your previous/debug server before starting this one.
- Client cannot connect: check the HTTP/port patch and loopback exemption above.
- Missing DLL: extract server.zip again; do not copy only gimuserverw.exe.
- Verify server files: powershell -NoProfile -ExecutionPolicy Bypass -File
    .\Check-Package.ps1 -VerifyHashes -SkipPortCheck
- Config uses paths relative to this folder; no developer-machine paths are needed.

This is an experimental offline emulator with incomplete game modes/content.
See release-build.json for the exact server/submodule revision and dirty-tree state.
The corresponding server source and license are linked in the GitHub release;
the packet-generator is a BUILD dependency, not something players install.
