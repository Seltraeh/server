#include <drogon/drogon.h>

#include <gimuserver/App.hpp>
#include <gimuserver/db/MigrationManager.hpp>
#include <gimuserver/drogon/GimuServer.hpp>

// The optional developer console is enabled by GIMU_ENABLE_DEBUG_CLI.
// Portable release presets disable it.
#ifdef BF_DEBUG_CLI
#include "DebugCli.hpp"
#endif

#include <filesystem>

int main(int argc, char** argv)
{
    // Unbuffered stdout so log lines reach redirected files immediately -
    // otherwise a wedged/killed headless run loses everything still sitting
    // in the CRT block buffer and boot failures become undiagnosable.
    setvbuf(stdout, nullptr, _IONBF, 0);

#ifdef BF_DEBUG_CLI
    // CLI-client mode: launched by the server to host the debug console window.
    // Must be checked before any Drogon / config setup so this process never
    // starts the web server or runs migrations.
    if (argc >= 3 && std::string(argv[1]) == "--debug-cli")
    {
        RunDebugCliClient(argv[2]);
        return 0;
    }
#endif

#ifdef _WIN32
    SetConsoleTitleW(L"GimuFrontier standalone server");
#endif

    // Config path resolution priority:
    //   1. Command-line argument (argv[1]) — explicit always wins.
    //   2. GIMU_DEFAULT_CONFIG_PATH — baked in at compile time for Debug
    //      builds so F5 from VS works without touching the launch config.
    //   3. Windows Release: config.json beside this executable.
    //      Other platforms: ./config.json in the caller's working directory.
#ifndef GIMU_DEFAULT_CONFIG_PATH
#define GIMU_DEFAULT_CONFIG_PATH "./config.json"
#endif
    std::filesystem::path configArg = (argc > 1) ? argv[1] : GIMU_DEFAULT_CONFIG_PATH;
#ifdef _WIN32
#ifndef _DEBUG
    if (argc <= 1)
    {
        wchar_t executable[32768];
        const auto length = GetModuleFileNameW(nullptr, executable, 32768);
        if (length == 0 || length >= 32768) return 1;
        configArg = std::filesystem::path(executable).parent_path() / "config.json";
    }
#endif
#endif

    try
    {
        // Resolve to an absolute path so the chdir below works regardless of
        // what the current directory was at launch.
        auto absConfig = std::filesystem::absolute(configArg);

        // Change into the config file's directory.  Every path in config.json
        // is relative to its own location (mst_root, document_root, etc.) so
        // this makes the server portable without editing config.json.
        std::filesystem::current_path(absConfig.parent_path());

        drogon::app()
            .loadConfigFile(absConfig.filename().string())
            // Idle keep-alive connections are never closed.  The client's
            // WinINet pools its connections to this server, and drogon's
            // default idle_connection_timeout (60 s) drops one under it: a
            // socket left idle for 90 s was aborted on its next request with
            // the default and answered with 0 (2026-10-23,
            // out/qa/current/session19/visual/keepalive_probe.py).
            // This is NOT what hung the title after the start tap.  That was
            // the proxy's debug console: a QuickEdit selection in it blocked
            // the login's console write on the game's main thread
            // (offline-proxy console_log.cpp).  With the console not held,
            // late logins succeeded on the old server too, so the client
            // recovers from a dropped idle socket and no observed failure
            // depends on this setting.  Kept as a defensive default (one local
            // client keeps few sockets open); change it only on evidence of
            // its own.  Set in code so a config cannot bring the timeout back.
            .setIdleConnectionTimeout(0)
            .registerBeginningAdvice([]() {
                auto db = drogon::app().getDbClient();
                // ⚠ WITHOUT A BUSY TIMEOUT ONE OUTSIDE READER WEDGES THE SERVER.
                // Drogon opens SQLite with none, queues a transaction's COMMIT
                // after the handler has already replied, and on SQLITE_BUSY
                // only logs "Transaction submission failed" -- no retry, no
                // rollback.  The connection then stays inside the open
                // transaction: the acknowledged action is never written and
                // every later request fails at "transaction begin" until a
                // restart (out/qa/current/session9/commit_race_probe_*.log).  A
                // backup-API copy of the save, a suite's direct read or a DB
                // browser overlapping one request was enough.  With a timeout
                // the COMMIT waits for such a reader instead.  The config uses
                // one connection (connection_number's default), so this one
                // statement reaches the connection every transaction runs on.
                db->execSqlSync("PRAGMA busy_timeout = 5000;");
                MigrationManager::RunMigrations(db);
                // Legacy SeedDefaultUnits/SeedDefaultTown removed: the
                // tutorial (CreateUser + UnitArchiver) provisions the user
                // under the fresh-DB model adopted from upstream dev.
#ifdef BF_DEBUG_CLI
                StartDebugCli();
#endif
            })
            .run();
    }
    catch (const std::exception& ex)
    {
        printf("Fatal exception during execution: %s\n", ex.what());
#ifdef _WIN32
        // Also write to the VS Output window so the message is visible
        // without needing to watch the console window.
        OutputDebugStringA("Fatal exception during execution: ");
        OutputDebugStringA(ex.what());
        OutputDebugStringA("\n");
#endif
        return 1;
    }

    drogon::HttpAppFramework::instance().getLoop()->queueInLoop([]()
        {
            drogon::HttpAppFramework::instance().quit();
    });

/*#ifdef _WIN32
    TerminateProcess(GetCurrentProcess(), 0);
#endif*/
    return 0;
}
