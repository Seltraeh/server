# Current checkout server activation — October 1, 2026

The QC output directory `out/build/bugfix-0929` was inside this checkout, but
the normal launcher still used the older `out/build/debug-win64` executable.
Source changes were already in this repository and its packet-generator submodule.

Built the current working tree successfully into the normal launcher target:
`out/build/debug-win64/standalone_frontend/Debug/gimuserverw.exe`.
Build evidence: `out/activate_current_2026-10-01/build.log`.

An isolated-build server started during compilation and migrated the live save.
Activity continued during compilation, including Vargas reaching 100 SP. Before
switching executables, backed up that latest save as well. Restarted the server
through `tools/bin/bf_ctl.ps1`; verified the normal executable owns port 9960
and uses the existing deploy save. No database table changed across this switch.
SQLite quick_check passed. Vargas instance 1034 remains level 150, BB/SBB 10,
SP 100, used SP 0, maximum SP 100. Deemo instances 1074–1076 remain present.

Recovery files (local, ignored output directory):
- `out/activate_current_2026-10-01/before_activation.sqlite`: before compilation.
- `out/activate_current_2026-10-01/before_switch.sqlite`: latest save before restart.
- `out/activate_current_2026-10-01/gimuserverw_before.exe`: previous launcher binary.
- `out/activate_current_2026-10-01/switch_verification.json`: unchanged-table check.

Reconnect the client to refresh its unit data. At the current 100/100 SP cap,
Vargas cannot demonstrate another SP gain; the previous 0/0 issue should no
longer appear. Purchase and reset handlers remain unfinished, so spending SP
is still blocked. Retest mission 10 twice and after reconnecting for the Farm
unlock replay. Keep the existing client checklist for other feature coverage.

Both repositories remain on audit-campaign with uncommitted work. Parent HEAD
matches the local mine/main reference at 7851dba8ebcc97b4d4793aae358b3d18b47a6ad2.
After client acceptance, commit/publish the packet-generator changes first,
then commit the parent changes including its updated submodule reference.
No commit or push was performed during activation. Build outputs and saves are
local runtime artifacts; source/data changes and the submodule commit belong
in the eventual mainline commits.
