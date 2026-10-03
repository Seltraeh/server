"""Port the client's F_FE_SKILL_MST into deploy/mst/fe_skill_mst.json.

python scripts/gen_fe_skill_mst.py          # write the server copy
python scripts/gen_fe_skill_mst.py --check  # exit 1 if it is missing or stale

WHY.  The SP enhancement skills -- what each costs (need_bp), its series and
level -- live only in F_FE_SKILL_MST, a table no response carries: the client
downloads it from this server as /mst/Ver<version>_2h9r3yEY.dat and reads it
through FESkillMstList.  FeSkillGet has to price a skill exactly the way the
client's Enhancements screen did before it let the player tap it
(UnitDetailVirtuallyInfoScene::skillTermCheck @0x1BE21E8), so the server keeps
a decrypted copy of THAT file, not an authored table.

SOURCE.  The version is the one deploy/mst/version_info_mst.json advertises for
F_FE_SKILL_MST (618 on 2026-09-30), the file is the one the server serves from
deploy/game_content/mst/, and the encode key is w3Z9EeUR
(tools/mst_download_map.json).  Rows are copied verbatim -- every value a
string, the client's own column hashes -- under the wrapper "2h9r3yEY" the
FeSkillMst KDL declares.

CHECK.  --check also compares deploy/mst/unit_fe_skill_mst.json (the per-unit
tree FeSkillGet validates nodes and prerequisites against) with the served
F_UNIT_FE_SKILL_MST file.  If the two ever diverge, the server would refuse or
allow a node the client drew differently, so the check fails loudly.
"""
import argparse
import base64
import json
import sys
from pathlib import Path

from Crypto.Cipher import AES
from Crypto.Util.Padding import unpad

ROOT = Path(__file__).resolve().parents[1]
MST = ROOT / 'deploy' / 'mst'
SERVED = ROOT / 'deploy' / 'game_content' / 'mst'
OUT = MST / 'fe_skill_mst.json'
TABLES = {  # table -> (file key, encode key, server json, wrapper)
    'F_FE_SKILL_MST': ('2h9r3yEY', 'w3Z9EeUR', OUT, '2h9r3yEY'),
    'F_UNIT_FE_SKILL_MST': ('8gu2U4Mh', 'dc0jxn8u', MST / 'unit_fe_skill_mst.json', 'kXes8fSi'),
}


def advertised(table):
    rows = json.loads((MST / 'version_info_mst.json').read_text(encoding='utf-8'))['KeC10fuL']
    row = next((r for r in rows if r['moWQ30GH'] == table), None)
    if row is None:
        raise SystemExit(f'{table} is not in version_info_mst.json')
    return int(row['d2RFtP8T']), int(row['5kbnkTp0'])


def served_rows(table):
    file_key, encode_key, _, _ = TABLES[table]
    version, count = advertised(table)
    path = SERVED / f'Ver{version}_{file_key}.dat'
    if not path.is_file():
        raise SystemExit(f'{path} is missing (the client could not download {table} either)')
    plain = unpad(AES.new(encode_key.encode().ljust(16, b'\0'), AES.MODE_ECB)
                  .decrypt(base64.b64decode(path.read_bytes())), 16)
    rows = json.loads(plain)
    if len(rows) != count:
        raise SystemExit(f'{path.name}: {len(rows)} rows, version_info_mst says {count}')
    return path, rows


def dump(wrapper, rows):
    return (json.dumps({wrapper: rows}, indent=1, ensure_ascii=False) + '\n').encode('utf-8')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--check', action='store_true')
    args = ap.parse_args()

    path, rows = served_rows('F_FE_SKILL_MST')
    ids = [r['ri6D9yBi'] for r in rows]
    if len(set(ids)) != len(ids):
        raise SystemExit('duplicate FE skill ids in the served table')
    new = dump('2h9r3yEY', rows)

    tree_path, tree = served_rows('F_UNIT_FE_SKILL_MST')
    server_tree = json.loads((MST / 'unit_fe_skill_mst.json').read_text(encoding='utf-8'))['kXes8fSi']
    tree_ok = server_tree == tree
    orphans = sorted({r['ri6D9yBi'] for r in tree} - set(ids))

    print(f'{path.name}: {len(rows)} FE skills; {tree_path.name}: {len(tree)} tree nodes, '
          f'{"identical to" if tree_ok else "DIFFERENT from"} deploy/mst/unit_fe_skill_mst.json; '
          f'{len(orphans)} tree node(s) without a skill row')
    if not tree_ok or orphans:
        return 1

    if args.check:
        current = OUT.read_bytes() if OUT.is_file() else b''
        print('up to date' if current == new else f'stale or missing: {OUT.name}')
        return 0 if current == new else 1
    if not OUT.is_file() or OUT.read_bytes() != new:
        OUT.write_bytes(new)
        print(f'wrote {OUT.relative_to(ROOT)}')
    else:
        print('nothing to write (already current)')
    return 0


if __name__ == '__main__':
    sys.exit(main())
