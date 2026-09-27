"""Publish deploy/mst/unit_mst.json to the client's versioned download cache.

Run after changing UnitMst. Requires pycryptodome. Writes all parts before
atomically replacing the manifest; retains previous versions for in-flight clients.
"""
import base64
import json
from pathlib import Path
from Crypto.Cipher import AES
from Crypto.Util.Padding import pad, unpad

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / 'deploy/game_content/content/F_UNIT_MST'
KEY = b'7nL1WTUb'.ljust(16, b'\0')


def main():
    rows = json.loads((ROOT / 'deploy/mst/unit_mst.json').read_text(encoding='utf-8'))['2r9cNSdt']
    DEST.mkdir(parents=True, exist_ok=True)
    manifest_path = DEST / 'manifest.json'
    old = json.loads(manifest_path.read_text()) if manifest_path.exists() else {'version': 1084}
    # Each independently encrypted part must contain a complete JSON array.
    parts, current, size = [], [], 2
    for row in rows:
        encoded = json.dumps(row, ensure_ascii=False, separators=(',', ':')).encode()
        if current and size + len(encoded) + 1 > 3 * 1024 * 1024:
            parts.append(current)
            current, size = [], 2
        current.append(row)
        size += len(encoded) + 1
    if current:
        parts.append(current)
    version = int(old['version']) + 1
    recovered = []
    for index, part in enumerate(parts, 1):
        suffix = f'_{index}' if len(parts) > 1 else ''
        path = DEST / f'Ver{version}_2r9cNSdt{suffix}.dat'
        plain = json.dumps(part, ensure_ascii=False, separators=(',', ':')).encode()
        blob = base64.b64encode(AES.new(KEY, AES.MODE_ECB).encrypt(pad(plain, 16)))
        path.write_bytes(blob)
        recovered.extend(json.loads(unpad(AES.new(KEY, AES.MODE_ECB).decrypt(base64.b64decode(path.read_bytes())), 16)))
    assert recovered == rows
    manifest = {'table': 'F_UNIT_MST', 'file_key': '2r9cNSdt', 'version': version,
                'file_count': len(parts), 'record_num': len(rows)}
    temp = DEST / 'manifest.json.tmp'
    temp.write_text(json.dumps(manifest, indent=1), encoding='utf-8')
    temp.replace(manifest_path)
    print(f'Published and verified UnitMst version {version}: {len(rows)} rows, {len(parts)} parts')


if __name__ == '__main__':
    main()
