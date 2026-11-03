# Session 19 client first-launch steps

Prepared October 23, 2026. This is a true fresh client identity and fresh isolated server save. No client launch or consent/age attestation was performed by setup.

## Exact candidate inputs

- Test package identity: `gumi.BraveFrontier.ContentTestS19Fresh`, package family `gumi.BraveFrontier.ContentTestS19Fresh_99p3jr0gh0z6w`, registered AppUserModelID `gumi.BraveFrontier.ContentTestS19Fresh_99p3jr0gh0z6w!App`.
- Package layout: `out/qa/current/session19/content_client_s19/package` (1,361 hard links to the extracted client, separate `AppxManifest.xml`, Session 18 isolated `libcurl.dll`).
- Session 19 server: `out/qa/current/session19/bin/gimuserverw.exe`, SHA-256 `02b95f90c98bcf921a247cc305c0244917c3a65274546ffc7eb23a074ade9272`.
- Client DLL: `out/qa/current/session18/proxy_29960_s18/libcurl.dll`, SHA-256 `cfb825da79de8ca7a68db12009e9a9c66f5181bdb0af38d6a7660d3b44aba545` (port 29960).
- Policy: `out/qa/current/session18/policies/family_policy_c8.json`, SHA-256 `388185d921265ca5197bc8e27671e36037e2d4703484116ec8cd2a7b118583ea`.
- Manifest: `out/qa/current/session19/coverage/content_startup_s19_final.json`, SHA-256 `9b3068e69b88e3408662bb51c6d0d9a4f0aa61a3c8452526a72d6a49ea51cd04`.

## Server and audit

The server uses the newly created blank save `out/qa/current/session19/content_client_s19/saves/s19_first_launch_blank/gme.sqlite`. Its config is `out/qa/current/session19/content_client_s19/saves/s19_first_launch_blank/config.json`; the database was 0 bytes before server migration and grew to 450,560 bytes. It does not copy or use the live save. The current server process is PID 2868 on `127.0.0.1:29960`.

The live bootstrap audit passed before client launch. Prediction JSON: `out/qa/current/session19/bootstrap/first_launch_prediction.json` (SHA-256 `049e24c1f2c25d0e7f8b0abb977727ec7323ba8d8e7b88fa000e1ba9196ba9bb`). Audit log: `out/qa/current/session19/bootstrap/first_launch_audit.log` (SHA-256 `26da5f7f43631f83de2fd22a8857ec497a6608ef1c2886293ad7f371e3c98b04`). Result: exit 0; 169 announced rows, every required delivery available, 29 response tables and 104 text dictionaries served; setInitialUnits and corrupted_mst_check pass.

A private wrapper under `out/qa/current/session19/content_provider_client_test_s19.py` adds this identity for the existing harness without changing the durable harness. To start the isolated server after it has been stopped:

```powershell
python out/qa/current/session19/content_provider_client_test_s19.py --identity s19 server --exe out/qa/current/session19/bin/gimuserverw.exe --policy out/qa/current/session18/policies/family_policy_c8.json --manifest out/qa/current/session19/coverage/content_startup_s19_final.json --save s19_first_launch_blank --timeout 420
```

Do not delete or recreate `s19_first_launch_blank`; using `--save` keeps the already initialized fresh save. Before a new server start, the harness `check` command should report 0 items outstanding while port 29960 is free:

```powershell
python out/qa/current/session19/content_provider_client_test_s19.py --identity s19 check --exe out/qa/current/session19/bin/gimuserverw.exe --policy out/qa/current/session18/policies/family_policy_c8.json --manifest out/qa/current/session19/coverage/content_startup_s19_final.json
```

## Registered AppX launch and observation

Activate the registered application by AppUserModelID `gumi.BraveFrontier.ContentTestS19Fresh_99p3jr0gh0z6w!App` through Computer Use (do not launch `BraveFrontier.Windows.exe` directly). The fresh LocalState is `C:\Users\Evan\AppData\Local\Packages\gumi.BraveFrontier.ContentTestS19Fresh_99p3jr0gh0z6w\LocalState`; at preparation it existed and contained no files.

Record the time and first visible state. Observe login, Initialize, the 142 `/mst/Ver...dat` parts, FILE_CHECK completion, any `REDOWNLOAD_MST_FILES` notice, and any network-error dialog. Do not submit age verification or consent attestations; stop at such a gate for the user. Once the first launch is finished, compare only the predicted filenames in LocalState and do not copy table contents out. The second launch should make no `/mst/` request.
