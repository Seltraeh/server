"""Does a client hold the files a startup manifest was derived from?  Read only.

    python scripts/check_client_inputs.py --manifest MANIFEST.json --localstate DIR [--package DIR] [--server-mst DIR]

The rule the Session 18 DLL applies before a complete coverage report may leave the LocalState census
or the background-download prompt out (offline-proxy cp_core.cpp VerifyClientInputs): every
inputs.client_files entry present under its root with that SHA-256, and LocalState holding no other
version or part of a named master table (Ver<v>_<key>[_<n>].dat, any case).  --package also compares
the package's client build with the manifest's; inputs.server_files are checked against the response
tables in --server-mst (deploy/mst by default) with the broker's own rule (startup_coverage
server_binding), and the area table that server synthesizes (served:3SG2wX0R; Session 19) -- it depends on
the server's banner folder: --no-banner-dir for an isolated test or harness server, which has none.  That
is a preview only: a running broker binds them to what ITS cache parsed and synthesized at boot (GET
/offline_mod/content/v1/status, T rows), which a later edit of the folder does not change.

Exit 0 when everything matches, 1 otherwise.  Nothing is written.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import startup_coverage as sc  # noqa: E402

TABLE = re.compile(r'ver(\d+)_([0-9a-z]{8})(?:_(\d+))?\.dat', re.I)


def digest(path: Path) -> str | None:
    try:
        return hashlib.sha256(path.read_bytes()).hexdigest()
    except OSError:
        return None


def check(manifest: dict, localstate: Path | None, package: Path | None, server_mst: Path | None,
          banner_dir: Path | None = sc.BANNER_DIR) -> tuple[bool, list[str]]:
    lines, ok = [], True
    inputs = manifest.get('inputs') or {}
    files = inputs.get('client_files') or []
    if not files:
        return False, ['the manifest names no client file (inputs.client_files): nothing ties it to a client']
    if package is not None:
        exe = package / 'BraveFrontier.Windows.exe'
        build = sc.client_build(exe) if exe.is_file() else None
        same = build == manifest.get('build')
        ok &= same
        lines.append(f'{"OK  " if same else "DIFF"} client build {build or "(no EXE)"}; the manifest was derived for '
                     f'{manifest.get("build")}')
    named = {}
    for f in files:
        root = localstate if f['root'] == 'localstate' else package
        if root is None:
            ok = False
            lines.append(f'SKIP {f["root"]} {f["path"]}: no --{f["root"]} folder given (not verified)')
            continue
        got = digest(root / f['path'])
        if got is None:
            ok = False
            lines.append(f'MISS {f["root"]} {f["path"]}: not in {root}')
        elif got != f['sha256']:
            ok = False
            lines.append(f'DIFF {f["root"]} {f["path"]}: sha256 {got[:16]}..., the manifest\'s {f["sha256"][:16]}...')
        else:
            lines.append(f'OK   {f["root"]} {f["path"]}')
        m = TABLE.fullmatch(f['path']) if f['root'] == 'localstate' else None
        if m:
            named.setdefault(m.group(2).lower(), set()).add(f['path'].lower())
    if localstate is not None and localstate.is_dir():
        for p in sorted(localstate.iterdir()):
            m = TABLE.fullmatch(p.name)
            if m and m.group(2).lower() in named and p.name.lower() not in named[m.group(2).lower()]:
                ok = False
                lines.append(f'EXTRA localstate {p.name}: another version or part of a table the manifest names '
                             f'({", ".join(sorted(named[m.group(2).lower()]))})')
    if server_mst is not None:
        served = sc.served_tables(server_mst, banner_dir)
        for f in inputs.get('server_files') or []:
            name = sc.server_input_name(f['root'], f['path'])
            got = served.get(name)
            same = got == f['sha256']
            lines.append(f'{"OK  " if same else ("MISS" if got is None else "DIFF")} server {name}'
                         + ('' if same else f' (sha256 {str(got)[:16]}..., the manifest\'s {f["sha256"][:16]}...)'))
        binding = sc.server_binding(manifest, served)
        ok &= not binding
        lines.append(f'{"OK  " if not binding else "DIFF"} server tables: '
                     + (f'bound to the {len(served)} server inputs in {server_mst}'
                        + (f' (banner folder {banner_dir})' if banner_dir is not None else ' (no banner folder: an isolated server)')
                        if not binding else binding))
    return ok, lines


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--manifest', required=True, type=Path)
    ap.add_argument('--localstate', type=Path, help="the client's LocalState folder (read only)")
    ap.add_argument('--package', type=Path, help="the folder holding the client's BraveFrontier.Windows.exe")
    ap.add_argument('--server-mst', type=Path, default=sc.RESPONSE_MST, help='the server\'s response tables')
    ap.add_argument('--banner-dir', type=Path, default=sc.BANNER_DIR,
                    help="the folder the server probes for translated area banners (its ./game_content/content/dungeon)")
    ap.add_argument('--no-banner-dir', action='store_true',
                    help='the server runs from a folder without game_content (an isolated test or harness server)')
    args = ap.parse_args()
    if args.no_banner_dir:
        args.banner_dir = None
    manifest = json.loads(args.manifest.read_text(encoding='utf-8'))
    errors = sc.validate_manifest(manifest)
    if errors:
        print(f'{args.manifest}: not a manifest the broker would load: {errors[:3]}')
        return 1
    ok, lines = check(manifest, args.localstate, args.package, args.server_mst, args.banner_dir)
    for line in lines:
        print(line)
    print(f'{args.manifest.name} ({sc.sha256_file(args.manifest)[:16]}...): '
          + ('this client holds exactly the files the manifest was derived from' if ok
             else 'NOT this client\'s inputs: a complete report from this manifest would not be used'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
