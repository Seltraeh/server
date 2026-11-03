"""Make one isolated run folder: a copied save and its server config.

    python scripts/qa/prep_run.py --qa-dir DIR --run NAME --save SOURCE.sqlite --content-root DIR
                                  [--mst-root DIR] [--archive-root DIR] [--system-root DIR] [--port 29961]
                                  [--set COLUMN=INT ...]

Writes <qa dir>/visual/saves/<NAME>/gme.sqlite and config.json, and nothing else.  Refuses, before writing:
  * a run name that is not one identifier, or whose folder would not be a direct child of the saves root,
    or a folder that already exists (runs are never overwritten);
  * the live save as the source, or a source whose user_info does not hold exactly one account (the copy
    is opened read-only through the SQLite backup API);
  * an MST or archive root missing any json deploy/mst or deploy/archive has, or a system root missing any
    json deploy/system has (a missing file at boot leaves an unkillable server);
  * a --set column outside SETTABLE or not in user_info, or a non-integer value.
Every config path is absolute with forward slashes.  Prints the copied account row.
"""
from __future__ import annotations

import argparse
import json
import sqlite3
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import qa_run as q  # noqa: E402

TEMPLATE = q.REPO / 'out/qa/current/session19/content_client_s19/saves/s19_first_launch_blank/config_relay_29961.json'
SETTABLE = {'level', 'exp', 'energy', 'energy_full_ts', 'gems', 'zel', 'karma', 'friend_points',
            'tutorial_status', 'tutorial_end_flag'}
ROW = 'SELECT id, gumi_user_id, level, exp, energy, energy_full_ts, gems, tutorial_status, tutorial_end_flag FROM user_info'


def parse_sets(items: list[str], columns: set[str]) -> list[tuple[str, int]]:
    out = []
    for item in items:
        col, sep, val = item.partition('=')
        if not sep or col not in SETTABLE or col not in columns:
            raise q.QaError(f'--set {item!r}: the column must be one of {sorted(SETTABLE & columns)}')
        try:
            out.append((col, int(val)))
        except ValueError:
            raise q.QaError(f'--set {item!r}: not an integer') from None
    return out


def account_count(db: sqlite3.Connection) -> int:
    return db.execute('SELECT COUNT(*) FROM user_info').fetchone()[0]


def prepare(qa, name: str, save, content_root, mst_root=None, system_root=None, port: int = 29961,
            sets: list[str] = (), archive_root=None) -> Path:
    qa = q.qa_dir(qa)
    run = q.run_dir(qa, name, must_exist=False)
    if run.exists():
        raise q.QaError(f'{run} already exists; runs are never overwritten')
    source = Path(save).resolve()
    if source == q.LIVE_SAVE:
        raise q.QaError('refusing the live save as a source')
    if not source.is_file():
        raise q.QaError(f'no save {source}')
    content = Path(content_root).resolve()
    if not (content / 'content').is_dir():
        raise q.QaError(f'{content} has no content/ folder')
    mst = Path(mst_root).resolve() if mst_root else (q.REPO / 'deploy' / 'mst').resolve()
    missing = sorted(p.name for p in (q.REPO / 'deploy' / 'mst').glob('*.json') if not (mst / p.name).is_file())
    if missing:
        raise q.QaError(f'{mst} lacks {missing[:5]}: a missing table at boot leaves an unkillable server')
    archive = Path(archive_root).resolve() if archive_root else (q.REPO / 'deploy' / 'archive').resolve()
    missing = sorted(p.name for p in (q.REPO / 'deploy' / 'archive').glob('*.json') if not (archive / p.name).is_file())
    if missing:
        raise q.QaError(f'{archive} lacks {missing[:5]}: a missing archive at boot leaves an unkillable server')
    system = Path(system_root).resolve() if system_root else qa / 'visual' / 'system'
    missing = sorted(p.name for p in (q.REPO / 'deploy' / 'system').glob('*.json') if not (system / p.name).is_file())
    if missing:
        raise q.QaError(f'{system} lacks {missing}: a missing system json at boot leaves an unkillable server')

    src = sqlite3.connect(f'file:{source.as_posix()}?mode=ro', uri=True)
    try:
        if account_count(src) != 1:
            raise q.QaError(f'{source} holds {account_count(src)} accounts; a run copy needs exactly one')
        columns = {r[1] for r in src.execute('PRAGMA table_info(user_info)')}
        changes = parse_sets(list(sets), columns)
        run.mkdir(parents=True)
        run = q.run_dir(qa, name, must_exist=True)       # re-checked now that the folders exist
        dst = sqlite3.connect(q.run_file(run, 'gme.sqlite'))
        src.backup(dst)
    finally:
        src.close()
    for col, val in changes:
        dst.execute(f'UPDATE user_info SET {col} = ?', (val,))      # col is from SETTABLE, checked above
    dst.commit()
    row = dst.execute(ROW).fetchall()
    dst.close()

    config = json.loads(TEMPLATE.read_text(encoding='utf-8'))
    config['listeners'][0]['port'] = int(port)
    config['app']['document_root'] = q.posix_abs(content)
    config['db_clients'][0]['filename'] = q.posix_abs(run / 'gme.sqlite')
    server = config['plugins'][0]['config']['server']
    server['system_root'] = q.posix_abs(system)
    server['mst_root'] = q.posix_abs(mst)
    server['archive_root'] = q.posix_abs(archive)
    q.run_file(run, 'config.json').write_text(json.dumps(config, indent=1), encoding='utf-8')
    print(f'{q.posix_abs(run / "gme.sqlite")} from {source.name}: {row}')
    print(f'{q.posix_abs(run / "config.json")}: port {port}, mst {server["mst_root"]}, archive {server["archive_root"]}, '
          f'content {q.posix_abs(content)}')
    return run


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('--qa-dir', required=True)
    ap.add_argument('--run', required=True)
    ap.add_argument('--save', required=True)
    ap.add_argument('--content-root', required=True)
    ap.add_argument('--mst-root')
    ap.add_argument('--archive-root')
    ap.add_argument('--system-root')
    ap.add_argument('--port', type=int, default=29961)
    ap.add_argument('--set', dest='sets', nargs='*', default=[])
    a = ap.parse_args(argv)
    try:
        prepare(a.qa_dir, a.run, a.save, a.content_root, a.mst_root, a.system_root, a.port, a.sets, a.archive_root)
        return 0
    except q.QaError as ex:
        print(f'refused: {ex}', file=sys.stderr)
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
