"""Queue a documented client-test supply bundle in one player's Present Box -- once.

    python scripts/grant_test_bundle.py MANIFEST.json --save PATH_TO_SAVE --backup-dir DIR [--dry-run]

The manifest names the account, a unique tag and the presents:

    {"tag": "Client QA evolution bundle 2026-10-02", "user_id": "CLz1ad9x",
     "presents": [{"type": 6, "target": "60133", "count": 1, "why": "..."}, ...],
     "unit_box": {"base_slots": 100}}

Safety rules, in order:
  * the save must not be served right now -- the live server's port (9960) must be
    closed, so nothing races the insert;
  * the tag must not already exist for that account (a retry delivers nothing);
  * unit presents must fit the unit box (base slots + bought slots - owned units);
  * a SQLite backup-API copy of the save is written to --backup-dir first;
  * every present is inserted in ONE transaction, read back and checked;
  * a receipt (present ids, counts, backup path, before/after counts) is written
    next to the backup.
Nothing else in the save is touched: no progression, units, items or currency.
Presents are claimed in the client's Present Box (type 6 unit, 5 material,
4 item, 3 Zel, 11 Karma, 8 Gems -- the shared present vocabulary).
"""
import argparse
import json
import socket
import sqlite3
import sys
import time
from pathlib import Path


def port_open(port):
    with socket.socket() as s:
        s.settimeout(0.5)
        return s.connect_ex(('127.0.0.1', port)) == 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('manifest', type=Path)
    ap.add_argument('--save', type=Path, required=True)
    ap.add_argument('--backup-dir', type=Path, required=True)
    ap.add_argument('--live-port', type=int, default=9960)
    ap.add_argument('--dry-run', action='store_true')
    args = ap.parse_args()

    manifest = json.loads(args.manifest.read_text(encoding='utf-8'))
    tag, user, presents = manifest['tag'], manifest['user_id'], manifest['presents']
    assert tag and user and presents, 'manifest needs tag, user_id and presents'
    for p in presents:
        assert int(p['type']) in (3, 4, 5, 6, 8, 11), p
        assert int(p['count']) > 0, p
    if port_open(args.live_port):
        sys.exit(f'Refusing: something is listening on {args.live_port}; stop the live server first.')

    save = args.save.resolve()
    db = sqlite3.connect(save, timeout=10)
    db.row_factory = sqlite3.Row
    if not db.execute('SELECT 1 FROM user_info WHERE id=?', (user,)).fetchone():
        sys.exit(f'Refusing: account {user} not in {save}')
    existing = db.execute('SELECT present_id FROM user_presents WHERE user_id=? AND description LIKE ?',
                          (user, tag + '%')).fetchall()
    if existing:
        sys.exit(f'Already delivered: {len(existing)} present(s) tagged "{tag}" exist; nothing granted.')

    owned_units = db.execute('SELECT COUNT(*) FROM user_units WHERE user_id=?', (user,)).fetchone()[0]
    bought = db.execute('SELECT max_unit_count FROM user_info WHERE id=?', (user,)).fetchone()[0] or 0
    slots = int(manifest.get('unit_box', {}).get('base_slots', 100)) + int(bought)
    incoming = sum(int(p['count']) for p in presents if int(p['type']) == 6)
    if owned_units + incoming > slots:
        sys.exit(f'Refusing: {owned_units} units + {incoming} incoming exceeds the {slots}-slot unit box.')
    unclaimed_before = db.execute('SELECT COUNT(*) FROM user_presents WHERE user_id=? AND is_receipt=0',
                                  (user,)).fetchone()[0]
    print(f'{user}: {owned_units} units of {slots} slots; {incoming} unit presents; '
          f'{unclaimed_before} unclaimed presents already queued')
    if args.dry_run:
        print('Dry run: nothing written.')
        return 0

    args.backup_dir.mkdir(parents=True, exist_ok=True)
    stamp = time.strftime('%Y%m%d_%H%M%S')
    backup = args.backup_dir.resolve() / f'before_grant_{stamp}.sqlite'
    dest = sqlite3.connect(backup)
    with dest:
        db.backup(dest)
    dest.close()

    now = int(time.time())
    ids = []
    with db:
        db.execute('BEGIN IMMEDIATE')
        for p in presents:
            cur = db.execute('INSERT INTO user_presents(user_id,present_type,target_id,target_cnt,receipt_type,'
                             'description,present_date) VALUES(?,?,?,?,0,?,?)',
                             (user, int(p['type']), str(p.get('target', '')), int(p['count']),
                              f'{tag} | {p["why"]}', now))
            ids.append(cur.lastrowid)
    rows = [dict(r) for r in db.execute(
        'SELECT present_id,present_type,target_id,target_cnt,is_receipt,description FROM user_presents'
        ' WHERE user_id=? AND description LIKE ? ORDER BY present_id', (user, tag + '%'))]
    assert len(rows) == len(presents) and [r['present_id'] for r in rows] == ids, rows
    assert db.execute('PRAGMA quick_check').fetchone()[0] == 'ok'
    db.close()
    receipt = {'tag': tag, 'user_id': user, 'save': str(save), 'backup': str(backup),
               'granted_at': time.strftime('%Y-%m-%dT%H:%M:%S%z'), 'present_ids': ids, 'presents': rows,
               'unit_box': {'owned_before': owned_units, 'slots': slots, 'incoming_units': incoming},
               'manifest': manifest}
    out = args.backup_dir.resolve() / f'grant_receipt_{stamp}.json'
    out.write_text(json.dumps(receipt, indent=1), encoding='utf-8')
    print(f'Queued {len(ids)} present(s), ids {ids[0]}..{ids[-1]}; backup {backup}; receipt {out}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
