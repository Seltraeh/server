"""Phase C6 candidate policy: the plain-image UI families the Home entry downloads (Session 15).

    python scripts/gen_ui_family_policy.py --base BASE_POLICY --out POLICY_JSON [--report REPORT_JSON]
                                           [--family item --family dungeon ...]

The Session 11 first-visit trace (no-prompt branch) shows Home's entry
(ConfirmScene -> HomeScene2, a TransitionalDLCScene) queueing 761 downloads; with
C5 owned, 704 are still legacy, and all but a few are single images of these
families (counted on that trace by content_inventory.py):

  item              item_thum_<id>.png and friends  (803 files, Home entry: 218)
  dungeon           quest / area banners            (888, 236)
  gacha             summon banners, buttons, door   (1,501 images, 157; its .txt gate scripts excluded)
  home              Home screen art                 (77, 39)
  summon/skill_icon summon screen skill icons       (25, 18)
  area              area art                        (120, 9)
  banner            banners                         (32, 4)

Why these may be owned.  Every one is a lone .png/.jpg (PNG or JPEG magic
checked here) with no file that depends on it or that it depends on -- no
sprite-frame plist, layout or font in these folders (footer/, which has frame
plists, and the button/common folders, full of flat-name collisions, are left
out).  Images reach the hooked CCFileUtils::getFileData through
CommonUtils::getTexture 0x2353D0 / CCTextureCache::addImage, whose FileCrypt
decode decisions the provider already makes per reader (client-confirmed for
the unit families, C1/C2), and every cocos-level existence check of a LocalState
path is hooked; downloads of owned, present names are filtered at enqueue.

Excluded and reported: a basename with differing copies elsewhere in
game_content (a flat-name collision -- the client's LocalState holds only one),
names the base policy already owns, and anything that is not a PNG/JPEG.
Reported, not excluded: names the app package also ships with other bytes.  A
cold client draws the packaged copy only until the Home entry downloads the
server's; owning them serves the server's from the first draw -- what every
client that has visited Home already draws.  One bundle per file.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CONTENT = ROOT / 'deploy' / 'game_content' / 'content'
APPX = ROOT.parent / 'BraveFrontierAppxClient' / 'Assets'
FAMILIES = ('item', 'dungeon', 'gacha', 'home', 'summon/skill_icon', 'area', 'banner')
IMAGE = ('.png', '.jpg')


def sha(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()


def is_image(p: Path) -> bool:
    head = p.read_bytes()[:4]
    return head == b'\x89PNG' or head[:2] == b'\xff\xd8'


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--base', required=True, type=Path, help='policy to extend (the C5 policy)')
    ap.add_argument('--out', required=True, type=Path)
    ap.add_argument('--report', type=Path)
    ap.add_argument('--family', action='append', help=f'folder under content/ (default: {", ".join(FAMILIES)})')
    args = ap.parse_args()
    out = args.out.resolve()
    if (ROOT / 'deploy').resolve() in out.parents:
        raise SystemExit('refusing: write the candidate policy outside deploy/')
    families = args.family or list(FAMILIES)

    where = defaultdict(list)
    for p in CONTENT.rglob('*'):
        if p.is_file():
            where[p.name.lower()].append(p)
    packaged = defaultdict(list)
    if APPX.is_dir():
        for p in APPX.rglob('*'):
            if p.is_file():
                packaged[p.name.lower()].append(p)
    base = json.loads(args.base.read_text(encoding='utf-8'))
    owned = {m.lower() for ms in base['bundles'].values() for m in ms}
    report = {'base': str(args.base), 'base_sha256': sha(args.base), 'families': {}}
    policy = json.loads(json.dumps(base))
    added = 0
    for fam in families:
        folder = CONTENT / fam
        if not folder.is_dir():
            raise SystemExit(f'no folder {folder}')
        r = {'owned': [], 'collision': [], 'already_owned': [], 'not_image': [], 'packaged_differ': [],
             'other_files': []}
        for p in sorted(folder.iterdir()):
            if not p.is_file():
                continue
            name = p.name.lower()
            if p.suffix.lower() not in IMAGE:
                r['other_files'].append(p.name)
                continue
            if not is_image(p):
                r['not_image'].append(p.name)
                continue
            copies = where[name]
            if len(copies) > 1 and len({sha(c) for c in copies}) > 1:
                r['collision'].append(p.name)
                continue
            if name in owned:
                r['already_owned'].append(p.name)
                continue
            if any(sha(q) != sha(p) for q in packaged.get(name, [])):
                r['packaged_differ'].append(p.name)
            # Identical copies elsewhere are harmless: the scope names this one exact path.
            policy['scope'].append(p.relative_to(CONTENT).as_posix())
            bundle = 'ui_' + fam.replace('/', '_') + '_' + p.name   # stems repeat across .png / .jpg
            if bundle in policy['bundles']:
                raise SystemExit(f'bundle name clash: {bundle}')
            policy['bundles'][bundle] = [p.name]
            owned.add(name)
            r['owned'].append(p.name)
            added += 1
        report['families'][fam] = {k: (len(v) if k == 'owned' else v) for k, v in r.items()}
    for ext in IMAGE:
        if ext not in policy['extensions']:
            policy['extensions'].append(ext)
    policy['enabled'] = False
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(policy, indent=1), encoding='utf-8')
    report.update({'policy': str(out), 'policy_sha256': sha(out), 'added': added,
                   'owned_names': len({m.lower() for ms in policy['bundles'].values() for m in ms})})
    if args.report:
        args.report.write_text(json.dumps(report, indent=1), encoding='utf-8')
    for fam, r in report['families'].items():
        print(f'{fam:18s} owned {r["owned"]:5d}; collisions {len(r["collision"])}, already owned '
              f'{len(r["already_owned"])}, not an image {len(r["not_image"])}, other files {len(r["other_files"])}, '
              f'packaged with other bytes (owned) {len(r["packaged_differ"])}')
    print(f'{out}: +{added} names, {report["owned_names"]} owned in all; sha256 {report["policy_sha256"]}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
