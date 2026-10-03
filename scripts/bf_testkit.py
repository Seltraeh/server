"""Shared helpers for isolated-server regression tests.

Every fixture lives under out/ and is a SQLite *backup API* copy, so the live
save is only ever opened read-only.  The server is started with the fixture's
absolute config path as argv[1] (a Debug build otherwise opens deploy/config.json,
binds the live port and opens the live save), readiness is confirmed over HTTP
and in the log, and only the process started here is stopped, by PID tree.

    from bf_testkit import Fixture, IsolatedServer, Client, Checker

    fx = Fixture.create(qa_dir('items'), port=19960)
    with IsolatedServer(exe, fx) as server:
        client = Client(fx)
        reply = client.call('cTZ3W2JG', 'ScJx6ywWEb0A3njT', {})

Requires pycryptodome.
"""
from __future__ import annotations

import base64
import gzip
import json
import socket
import sqlite3
import subprocess
import time
import urllib.error
import urllib.request
from pathlib import Path

from Crypto.Cipher import AES

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'out'
LIVE_SAVE = ROOT / 'deploy' / 'gme.sqlite'
# The one place mutable fixtures go: out/qa/current/<suite>.  A suite reuses its
# own directory (recreated each run), so runs replace rather than accumulate.
QA_ROOT = OUT / 'qa' / 'current'


def qa_dir(suite: str) -> Path:
    """out/qa/current/<suite> -- a suite's fixture, config and short logs."""
    assert suite and '/' not in suite and '\\' not in suite and suite not in ('.', '..'), suite
    return QA_ROOT / suite

# Group id -> AES key for the requests the regression suites send.
KEYS = {
    'MissionStart': ('jE6Sp0q4', 'csiVLDKkxEwBfR70'),
    'MissionEnd': ('9TvyNR5H', 'oINq0rfUFPx5MgmT'),
    'MissionContinue': ('p8B2i9rJ', 'G3FwvQfy5hcxHMen'),
    'MissionRestart': ('IP96ys7T', '0Zy3G9eD'),
    'UserInfo': ('cTZ3W2JG', 'ScJx6ywWEb0A3njT'),
    'UpdateInfoLight': ('ynB7X5P9', '7kH9NXwC'),
    'ItemEdit': ('ruoB7bD8', 'DHEfRexCu0q5TAQm'),
    'ItemMix': ('4P5GELTF', 'AFqKIJ8Z4mHPB9xg'),
    'ItemSphereEqp': ('0IXGiC9t', 'CZE56XAY'),
    'UnitEvo': ('0gUSE84e', 'biHf01DxcrPou5Qt'),
    'UnitOmniEvo': ('4Dk4spf9', '4s3lsODp'),
    'UnitMix': ('Mw08CIg2', 'JnegC7RrN3FoW8dQ'),
    'SlotAction': ('vChFp73J', 'hm9X6BQj'),
    'GachaAction': ('F7JvPk5H', 'bL9fipzaSy7xN2w1'),
    'AchievementTrade': ('m9LiF6P2', '0IWC9LVq'),
    'GetAchievementInfo': ('YPBU7MD8', 'AKjzyZ81'),
    'GuildInfo': ('138ba8d4', '23gD81ia'),
    'GuildCreate': ('g298Da10', 'G23Bd01d'),
    'GuildTrade': ('38adiJeb', 'ja3biAqb'),
    'PresentList': ('nhjvB52R', '6F9sMzBxEv8jXpau'),
    'PresentReceipt': ('bV5xa0ZW', 'X2QFqAKfomPIg3rG'),
    'TownFacilityUpdate': ('8v43tz7g', 'rq7Yd1nG'),
    'DungeonEventUpdate': ('BjAt1D6b', 'k5EiNe9x'),
    'HomeInfo': ('NiYWKdzs', 'f6uOewOD'),
}

# PermitPlace's three partitions (see gimuserver/gme/common/PermitPlace.cpp).
PERMIT_CHANNELS = ('yXNM8kL3', 'Y73tHKS8', 'Y73mHKS8')


def permit_rows(reply: dict) -> dict:
    """{channel: [row, ...]} for every PermitPlace partition a reply carries."""
    return {k: list(reply.get(k) or []) for k in PERMIT_CHANNELS}


def permitted(reply: dict, key: str) -> set:
    """Place ids of one entity kind (j28VNcUW mission, MHx05sXt dungeon, VjCY7rX4
    area, 9C64Qwe0 land) permitted on any channel of a reply."""
    return {int(r[key]) for rows in permit_rows(reply).values() for r in rows if key in r}


def backup_copy(source: Path, target: Path) -> None:
    """Consistent copy of a possibly-live SQLite file (read-only source)."""
    target.parent.mkdir(parents=True, exist_ok=True)
    if target.exists():
        target.unlink()
    src = sqlite3.connect(f'file:{source.as_posix()}?mode=ro', uri=True)
    dst = sqlite3.connect(target)
    with dst:
        src.backup(dst)
    src.close()
    dst.close()


def replay_warehouse_migration(db: sqlite3.Connection) -> None:
    """Return a copied save's warehouse to its pre-29092026 shape, on the copy only.

    Suites that seed legacy user_items rows (instance ids, favorites) and then
    check how 29092026_PersistentWarehouseRows carries them over need a save that
    has not been migrated yet -- and the live save they copy has been since
    2026-10-01, so their seeds silently did nothing.  Both migrations involved are
    replayable (CREATE ... IF NOT EXISTS, then INSERT OR IGNORE from user_items),
    so dropping the table and forgetting them makes the server rebuild the rows
    from user_items at startup, exactly as it did for the real save.  The seen
    mark restarts at 0, as it was before the first warehouse list.
    """
    db.execute('DROP TABLE IF EXISTS user_warehouse_rows')
    db.execute("DELETE FROM migration_status WHERE hash IN "
               "('29092026_PersistentWarehouseRows', '30092026_WarehouseLiveRowIndex')")
    db.execute('UPDATE user_info SET warehouse_seen_id=0')


class Fixture:
    """An isolated save + config under out/."""

    def __init__(self, directory: Path, port: int):
        self.dir = Path(directory).resolve()
        assert self.dir.is_relative_to(OUT), f'fixtures must live under out/: {self.dir}'
        self.port = port
        self.db_path = self.dir / 'gme.sqlite'
        self.config = self.dir / 'config.json'

    @classmethod
    def create(cls, directory, port: int = 19960, source: Path = LIVE_SAVE,
               archive_root: Path | None = None, mst_root: Path | None = None,
               fresh: bool = False) -> 'Fixture':
        fx = cls(Path(directory) if Path(directory).is_absolute() else ROOT / directory, port)
        fx.dir.mkdir(parents=True, exist_ok=True)
        if fresh:
            if fx.db_path.exists():
                fx.db_path.unlink()
        else:
            backup_copy(Path(source), fx.db_path)
        config = {
            'listeners': [{'address': '127.0.0.1', 'port': port, 'https': False}],
            'app': {'number_of_threads': 2, 'log': {'log_level': 'INFO'},
                    'enable_session': True, 'session_timeout': 1200,
                    'document_root': (ROOT / 'deploy' / 'game_content').as_posix()},
            'db_clients': [{'name': 'default', 'rdbms': 'sqlite3', 'is_fast': False,
                            'connection_number': 1, 'filename': fx.db_path.as_posix()}],
            'plugins': [{'name': 'GimuServer', 'dependencies': [], 'config': {
                'extra_log': {'enable': False},
                'server': {
                    'wallpaper_banner': '/wallpaper/title_logo20151028.jpg',
                    'game_version': 21900,
                    'notice_url': 'http://ios21900.bfww.gumi.sg/pages/versioninfo',
                    'mst_root': (mst_root or ROOT / 'deploy' / 'mst').as_posix(),
                    'system_root': (ROOT / 'deploy' / 'system').as_posix(),
                    'archive_root': (archive_root or ROOT / 'deploy' / 'archive').as_posix(),
                    'fps_cap': 60,
                    'vortex_weekend_opens_all': False}}}],
        }
        fx.config.write_text(json.dumps(config, indent=1), encoding='utf-8')
        return fx

    def db(self) -> sqlite3.Connection:
        conn = sqlite3.connect(self.db_path, timeout=30)
        conn.row_factory = sqlite3.Row
        return conn


class IsolatedServer:
    """Start/stop one test server process for a fixture."""

    def __init__(self, exe: Path, fixture: Fixture, log_name: str = 'server.log'):
        self.exe = Path(exe).resolve()
        assert self.exe.is_file(), self.exe
        self.fx = fixture
        self.log_path = fixture.dir / log_name
        self.proc = None

    @staticmethod
    def _listeners(port: int) -> set[int]:
        """PIDs listening on 127.0.0.1:port, from the OS (not from the log)."""
        out = subprocess.run(['netstat', '-ano', '-p', 'TCP'], capture_output=True, text=True).stdout
        pids = set()
        for line in out.splitlines():
            parts = line.split()
            if len(parts) >= 5 and parts[1] in (f'127.0.0.1:{port}', f'0.0.0.0:{port}') \
                    and parts[3] == 'LISTENING':
                pids.add(int(parts[4]))
        return pids

    @staticmethod
    def _children(pid: int, since: float) -> list[int]:
        """Processes whose parent is `pid` and that were created after `since`.

        NOT `taskkill /T`: that trusts the parent-pid link alone, and Windows
        recycles pids.  A process whose launcher has exited still names the dead
        pid as its parent -- the live server started from a shell that has since
        closed is exactly that -- so a test server handed the recycled pid would
        take it down as "its" child.  The server's own children (the debug-console
        process) are always created after it; anything older is not ours.
        """
        script = (f"Get-CimInstance Win32_Process -Filter 'ParentProcessId={int(pid)}' | ForEach-Object "
                  "{ '{0} {1}' -f $_.ProcessId, ([DateTimeOffset]$_.CreationDate).ToUnixTimeMilliseconds() }")
        out = subprocess.run(['powershell', '-NoProfile', '-NonInteractive', '-Command', script],
                             capture_output=True, text=True).stdout
        children = []
        for line in out.splitlines():
            parts = line.split()
            if len(parts) == 2 and parts[0].isdigit() and parts[1].isdigit() and int(parts[1]) / 1000 >= since:
                children.append(int(parts[0]))
        return children

    def __enter__(self):
        assert not self._listeners(self.fx.port), f'port {self.fx.port} already in use'
        self._log = self.log_path.open('w', encoding='utf-8', errors='replace')
        # A second of slack for clock granularity; a recycled-pid victim is
        # older than this by far (it outlived its own launcher).
        self._started = time.time() - 1.0
        self.proc = subprocess.Popen(
            [str(self.exe), str(self.fx.config)], cwd=str(self.fx.dir),
            stdin=subprocess.DEVNULL, stdout=self._log, stderr=subprocess.STDOUT,
            creationflags=subprocess.CREATE_NO_WINDOW)
        try:
            deadline = time.time() + 180
            while time.time() < deadline:
                if self.proc.poll() is not None:
                    raise RuntimeError(f'server exited early; see {self.log_path}')
                try:
                    with urllib.request.urlopen(
                            f'http://127.0.0.1:{self.fx.port}/offline_mod/fps_cap', timeout=1) as r:
                        if r.status == 200:
                            break
                except OSError:
                    time.sleep(0.5)
            else:
                raise RuntimeError(f'server did not start; see {self.log_path}')
            owners = self._listeners(self.fx.port)
            assert owners == {self.proc.pid}, \
                f'port {self.fx.port} is owned by {owners}, not the test process {self.proc.pid}'
            assert self.proc.pid not in self._listeners(9960), 'test process bound the live port 9960'
        except BaseException:
            self.stop()
            raise
        return self

    def stop(self):
        if self.proc:
            # Children first (see _children), each by its own pid; then the server.
            for child in self._children(self.proc.pid, self._started):
                subprocess.run(['taskkill', '/F', '/PID', str(child)], capture_output=True)
            if self.proc.poll() is None:
                subprocess.run(['taskkill', '/F', '/PID', str(self.proc.pid)], capture_output=True)
                try:
                    self.proc.wait(timeout=20)
                except subprocess.TimeoutExpired:
                    pass
        if getattr(self, '_log', None):
            self._log.close()
            self._log = None

    def __exit__(self, *exc):
        self.stop()

    def log_text(self) -> str:
        return self.log_path.read_text(encoding='utf-8', errors='replace')


class Client:
    """Encrypted GME client for one account in a fixture."""

    def __init__(self, fixture: Fixture, user_id: str | None = None):
        self.fx = fixture
        with fixture.db() as db:
            row = (db.execute('SELECT id, gumi_user_id FROM user_info WHERE id=?', (user_id,)).fetchone()
                   if user_id else db.execute('SELECT id, gumi_user_id FROM user_info LIMIT 1').fetchone())
        assert row, 'fixture has no account'
        self.user, self.login = row['id'], row['gumi_user_id']
        self.url = f'http://127.0.0.1:{fixture.port}/bf/gme/action.php'

    def call(self, name_or_group: str, key: str | None = None, body: dict | None = None,
             tag: str = 'bugfix'):
        if key is None:
            group, key = KEYS[name_or_group]
        else:
            group = name_or_group
        body = dict(body or {})
        # Preserve request-specific scenario markers while pinning identity to
        # the fixture account. Deck/event requests upload these in this group.
        login_info = dict((body.get('IKqx1Cn9') or [{}])[0])
        login_info.update(iN7buP2h=self.login, h7eY3sAK=self.user)
        body['IKqx1Cn9'] = [login_info]
        k = key.encode().ljust(16, b'\0')[:16]
        raw = json.dumps(body).encode()
        pad = 16 - len(raw) % 16
        cipher = AES.new(k, AES.MODE_ECB)
        envelope = {'F4q6i9xe': {'Hhgi79M1': group, 'aV6cLn3v': tag},
                    'a3vSYuq2': {'Kn51uR4Y': base64.b64encode(
                        cipher.encrypt(raw + bytes([pad]) * pad)).decode()}}
        req = urllib.request.Request(self.url, json.dumps(envelope).encode(),
                                     {'Content-Type': 'application/json'})
        with urllib.request.urlopen(req, timeout=120) as r:
            data = r.read()
            if r.headers.get('Content-Encoding') == 'gzip':
                data = gzip.decompress(data)
        outer = json.loads(data)
        if 'b5PH6mZa' in outer:
            return {'error': outer['b5PH6mZa']}
        plain = cipher.decrypt(base64.b64decode(outer['a3vSYuq2']['Kn51uR4Y']))
        return json.loads(plain[:-plain[-1]])


class Checker:
    """Collects PASS/FAIL lines; exit status is the failure count."""

    def __init__(self):
        self.failures = 0
        self.passes = 0

    def __call__(self, label: str, cond, detail='') -> bool:
        ok = bool(cond)
        print(('PASS  ' if ok else 'FAIL  ') + label + ('' if ok else f'  -- {detail}'), flush=True)
        if ok:
            self.passes += 1
        else:
            self.failures += 1
        return ok

    def summary(self, name: str) -> int:
        print(f'{name}: {self.passes} passed, {self.failures} failed', flush=True)
        return 1 if self.failures else 0


def mission_start_body(mission_id: int, deck: int = 0) -> dict:
    """MissionStart as the client sends it (every number quoted)."""
    return {
        '6FrKacq7': [{'Kn51uR4Y': '5EdKHavF'}],
        '9Q1Lq5FS': [{'h7eY3sAK': '0', 'J3stQ7jd': '0', 'j28VNcUW': str(mission_id),
                      'jkldTrhL': '0', 'Z0Y4RoD7': str(deck), 'nA95Bdj6': '0',
                      '5Z1LNoyH': '0', 'u1iPEVUq': '-1'}],
        'JzS3uxsZ': [{'0b4efi1W': '1'}],
    }


def mission_end_body(mission_id: int, *, status: int = 2, zel: int = 0, karma: int = 0,
                     items: str = '', units: str = '', used: str = '',
                     deck_units=None, serial: int | None = None) -> dict:
    """MissionEnd as the client sends it: rXvA1E5y battle log, all strings.

    The client echoes the serial MissionStart replied with (Kz7qfSs5.k9cxD7Ba,
    see started_serial); without `serial` the mission id is sent, which is the
    serial an older server issued.
    """
    battle = {'47cJBUxz': used, 'Najhr8m6': str(zel), 'HTVh8a65': str(karma),
              '4T0Q2Bh5': items, '3MAT6quo': units, 'hoG2ieT5': '0', '6PLsn8xo': '0',
              '5NRJQ1LU': '0', 'XP06YWdT': '0', 'U8uZLA34': '0', 'rZQJF5G9': '0',
              'TW1Mrtp5': '0', 'e6BKoYy9': '0'}
    body = {'6FrKacq7': [{'Kn51uR4Y': '5EdKHavF'}],
            'Kz7qfSs5': [{'k9cxD7Ba': str(mission_id if serial is None else serial), 'j3g5P4cq': str(status)}],
            'rXvA1E5y': [battle]}
    if deck_units:
        body['0xgIw1Ns'] = [{'edy7fq3L': str(u), 'gr48vsdJ': '0' if i == 0 else '1',
                             'XuJL4pc5': str(i)} for i, u in enumerate(deck_units)]
    return body


def started_serial(reply: dict) -> int:
    """The battle serial a MissionStart reply issued (Kz7qfSs5[0].k9cxD7Ba)."""
    return int(reply['Kz7qfSs5'][0]['k9cxD7Ba'])


def free_port() -> int:
    with socket.socket() as s:
        s.bind(('127.0.0.1', 0))
        return s.getsockname()[1]
