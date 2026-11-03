"""Shared helpers for isolated-server regression tests.

Every fixture lives under out/ and is a SQLite *backup API* copy, so the live
save is only ever opened read-only.  The server is started with the fixture's
absolute config path as argv[1] (a Debug build otherwise opens deploy/config.json,
binds the live port and opens the live save), readiness is confirmed over HTTP
and by the OS listener table (the started process must be the port's only
listener), and only the processes started here are stopped, each by its PID.

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
# What the November 2, 2026 content release replaced in deploy (readiness §3): archive/ before any generator ran,
# and jp_units/ (the files jp_units.py replaced).  A test that diffs one release step against deploy starts from
# these once deploy carries that step itself.
PRE_RELEASE = OUT / 'backups' / '2026-11-02_r8_content_apply'
# What scripts/jp_exclusive_units.py replaced when it was installed into deploy (November 2, later).
PRE_JP_EXCLUSIVE = OUT / 'backups' / '2026-11-02_r8_jp_exclusive'


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
               fresh: bool = False, document_root: Path | None = None,
               system_root: Path | None = None, keep: bool = False) -> 'Fixture':
        """keep=True: a save already in the fixture is used as it is (seeded from
        `source` only when absent), so progress persists across server restarts."""
        fx = cls(Path(directory) if Path(directory).is_absolute() else ROOT / directory, port)
        fx.dir.mkdir(parents=True, exist_ok=True)
        if keep and fx.db_path.exists():
            pass
        elif fresh:
            if fx.db_path.exists():
                fx.db_path.unlink()
        else:
            backup_copy(Path(source), fx.db_path)
        config = {
            'listeners': [{'address': '127.0.0.1', 'port': port, 'https': False}],
            'app': {'number_of_threads': 2, 'log': {'log_level': 'INFO'},
                    'enable_session': True, 'session_timeout': 1200,
                    'document_root': (document_root or ROOT / 'deploy' / 'game_content').as_posix()},
            'db_clients': [{'name': 'default', 'rdbms': 'sqlite3', 'is_fast': False,
                            'connection_number': 1, 'filename': fx.db_path.as_posix()}],
            'plugins': [{'name': 'GimuServer', 'dependencies': [], 'config': {
                'extra_log': {'enable': False},
                'server': {
                    'wallpaper_banner': '/wallpaper/title_logo20151028.jpg',
                    'game_version': 21900,
                    'notice_url': 'http://ios21900.bfww.gumi.sg/pages/versioninfo',
                    'mst_root': (mst_root or ROOT / 'deploy' / 'mst').as_posix(),
                    'system_root': (system_root or ROOT / 'deploy' / 'system').as_posix(),
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


LIVE_PORT = 9960


class ServerStartError(RuntimeError):
    """A server that never became ready.  The message names its log."""

    def __init__(self, message: str, log_path: Path):
        super().__init__(f'{message}; see {log_path}')
        self.log_path = log_path


def fps_cap_check(fixture: Fixture):
    """(path, judge) for the fixture's own /offline_mod/fps_cap: HTTP 200 and the
    cap its config sets -- an answer from a server reading THIS config."""
    plugin = json.loads(fixture.config.read_text(encoding='utf-8'))['plugins'][0]['config']
    want = str(plugin['server']['fps_cap'])

    def judge(status: int, body: str):
        return (status == 200 and body.strip() == want), f'fps_cap answered HTTP {status} {body.strip()[:40]!r}, want 200 {want!r}'
    return '/offline_mod/fps_cap', judge


class IsolatedServer:
    """Start/stop one test server process for a fixture.

    Ready means all of these, observed in this order: the child is running;
    the OS lists it as the ONLY listener on the fixture's port; every
    readiness check (default: the fixture's fps_cap) answers as required;
    the OS still lists it as the only listener.  The answers are bracketed by
    two exclusive-ownership observations because Windows lets a second
    process bind the same port silently -- an answer alone could come from
    someone else's server.  A foreign or duplicate listener, the child
    exiting, a wrong answer that persists, or the timeout raise
    ServerStartError naming the log, after stopping only what this object
    started (see stop).

    The OS and the clock are reached only through the underscore methods so
    scripts/test_content_harness_startup.py can drive every branch with fakes.
    """

    def __init__(self, exe: Path, fixture: Fixture, log_name: str = 'server.log', *,
                 checks=None, allow_live_port: bool = False, timeout: float = 180.0):
        self.exe = Path(exe).resolve()
        if not self.exe.is_file():
            raise FileNotFoundError(self.exe)
        self.fx = fixture
        self.log_path = fixture.dir / log_name
        self.proc = None
        self._log = None
        # [(path, judge(status, body) -> (ok, why))]; all must hold.
        self.checks = list(checks) if checks is not None else [fps_cap_check(fixture)]
        # Only the client-acceptance harness takes the live port, on purpose
        # and after its own refusals; suites never do.
        self.allow_live_port = allow_live_port
        self.timeout = timeout

    # -- the OS and the clock (replaced by fakes in tests) ---------------------

    @staticmethod
    def _listeners(port: int) -> set[int]:
        """PIDs listening on port on loopback or any address, IPv4 or IPv6 (a
        dual-stack [::] socket also takes 127.0.0.1), from the OS, not the log."""
        out = subprocess.run(['netstat', '-ano'], capture_output=True, text=True).stdout
        local = {f'127.0.0.1:{port}', f'0.0.0.0:{port}', f'[::]:{port}', f'[::1]:{port}'}
        pids = set()
        for line in out.splitlines():
            parts = line.split()
            if len(parts) >= 5 and parts[0] == 'TCP' and parts[1] in local and parts[3] == 'LISTENING':
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

    def _spawn(self):
        self._log = self.log_path.open('w', encoding='utf-8', errors='replace')
        return subprocess.Popen(
            [str(self.exe), str(self.fx.config)], cwd=str(self.fx.dir),
            stdin=subprocess.DEVNULL, stdout=self._log, stderr=subprocess.STDOUT,
            creationflags=subprocess.CREATE_NO_WINDOW)

    def _kill(self, pid: int) -> None:
        subprocess.run(['taskkill', '/F', '/PID', str(pid)], capture_output=True)

    def _probe(self, path: str):
        """(status, body) of GET path on the fixture's port, or None without an answer."""
        try:
            with urllib.request.urlopen(f'http://127.0.0.1:{self.fx.port}{path}', timeout=2) as r:
                return r.status, r.read().decode('utf-8', 'replace')
        except urllib.error.HTTPError as e:      # an answer, just not a 2xx one
            return e.code, e.read().decode('utf-8', 'replace')
        except OSError:
            return None

    _now = staticmethod(time.time)
    _sleep = staticmethod(time.sleep)

    # -- lifecycle ---------------------------------------------------------------

    def _fail(self, message: str):
        raise ServerStartError(message, self.log_path)

    def _answers(self):
        """(all ok, why not) over every readiness check, one request each."""
        for path, judge in self.checks:
            answer = self._probe(path)
            if answer is None:
                return False, f'no answer from {path}'
            ok, why = judge(*answer)
            if not ok:
                return False, why
        return True, ''

    def _owners_or_fail(self) -> set[int]:
        owners = self._listeners(self.fx.port)
        if owners - {self.proc.pid}:
            what = 'duplicate' if self.proc.pid in owners else 'foreign'
            self._fail(f'{what} listener on 127.0.0.1:{self.fx.port}: pid(s) {sorted(owners)} listen there,'
                       f' the started server is pid {self.proc.pid}')
        return owners

    def start(self) -> 'IsolatedServer':
        port = self.fx.port
        if port == LIVE_PORT and not self.allow_live_port:
            self._fail(f'refusing the live port {LIVE_PORT} (suites never take it)')
        busy = self._listeners(port)
        if busy:
            self._fail(f'127.0.0.1:{port} is already in use by pid(s) {sorted(busy)}; nothing was started')
        # A second of slack for clock granularity; a recycled-pid victim is
        # older than this by far (it outlived its own launcher).
        self._started = self._now() - 1.0
        self.proc = self._spawn()
        try:
            deadline = self._now() + self.timeout
            last = 'no answer yet'
            while True:
                code = self.proc.poll()
                if code is not None:
                    self._fail(f'server exited before it was ready (exit code {code})')
                if self._owners_or_fail() == {self.proc.pid}:
                    ok, why = self._answers()
                    if ok and self._owners_or_fail() == {self.proc.pid}:
                        break
                    last = why or 'the started server stopped listening'
                else:
                    last = f'pid {self.proc.pid} is not listening on 127.0.0.1:{port} yet'
                if self._now() >= deadline:
                    self._fail(f'server did not become ready within {self.timeout:.0f} s (last: {last})')
                self._sleep(0.5)
            if port != LIVE_PORT and self.proc.pid in self._listeners(LIVE_PORT):
                self._fail(f'the started server also bound the live port {LIVE_PORT}')
        except BaseException:
            self.stop()
            raise
        return self

    def __enter__(self):
        return self.start()

    def ownership_problem(self) -> str | None:
        """None while the started server is running and is the only listener on
        its port; otherwise what changed.  For long-running watchers."""
        code = self.proc.poll()
        if code is not None:
            return f'the server exited (exit code {code})'
        owners = self._listeners(self.fx.port)
        if owners - {self.proc.pid}:
            return (f'pid(s) {sorted(owners - {self.proc.pid})} also listen on 127.0.0.1:{self.fx.port}'
                    if self.proc.pid in owners else
                    f'pid(s) {sorted(owners)} listen on 127.0.0.1:{self.fx.port}, not the server pid {self.proc.pid}')
        if not owners:
            return f'the server (pid {self.proc.pid}) no longer listens on 127.0.0.1:{self.fx.port}'
        return None

    def stop(self):
        """Stop only what start() launched: the children the server created after
        it was started (see _children), each by its own pid, then the server."""
        if self.proc:
            for child in self._children(self.proc.pid, self._started):
                self._kill(child)
            if self.proc.poll() is None:
                self._kill(self.proc.pid)
                try:
                    self.proc.wait(timeout=20)
                except subprocess.TimeoutExpired:
                    pass
        if self._log:
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


class DebugConsole:
    """The server's debug console over its named pipe, gimudebug_<pid>.

    Each command's output ends with the SOH sentinel (DebugCli.cpp kEot); the
    first read is the banner.  Use it as a context manager around commands sent
    to a running IsolatedServer:

        with DebugConsole(server) as console:
            said = console.run('clearunits')
    """

    def __init__(self, server: 'IsolatedServer', timeout: float = 30.0):
        # The server creates the pipe in its beginning advice, after the
        # migrations, which can finish after the HTTP readiness probe has
        # already answered: wait for it rather than failing the first open.
        name = '\\\\.\\pipe\\gimudebug_' + str(server.proc.pid)
        deadline = time.time() + timeout
        while True:
            try:
                self.pipe = open(name, 'r+b', buffering=0)
                break
            except OSError:
                if time.time() > deadline:
                    raise
                time.sleep(0.25)
        self.banner = self._read()

    def _read(self) -> str:
        data = bytearray()
        while True:
            b = self.pipe.read(1)
            if not b:
                raise RuntimeError('debug console pipe disconnected')
            if b == b'\x01':
                return data.decode('utf-8', errors='replace')
            data.extend(b)

    def run(self, command: str) -> str:
        self.pipe.write((command + '\n').encode())
        return self._read()

    def close(self) -> None:
        self.pipe.close()

    def __enter__(self) -> 'DebugConsole':
        return self

    def __exit__(self, *exc) -> None:
        self.close()


def roster_problems(user_info: dict) -> list:
    """What would break the client in a UserInfo reply's roster and squads.

    The client looks every squad slot up in its unit list and dereferences the
    result unchecked (PartyEditTopScene2::setUnitAnime: Manage Squad and the
    Home squad panes; x86 +0xA6ECFC, Session 11 Run C).  Checks: every slot
    (dX7S2Lc1) names a unit in 4ceMWH6k; each squad has exactly one leader
    (member_type 0), no unit twice and no slot twice; the active squad
    (fEi17cnx[0].Z0Y4RoD7, deck_type 1) exists and is led by an owned unit.
    Returns human-readable problems; empty means the payload is drawable."""
    problems = []
    units = {str(u.get('edy7fq3L')) for u in user_info.get('4ceMWH6k', [])}
    if not units:
        problems.append('no units in 4ceMWH6k')
    squads = {}
    for s in user_info.get('dX7S2Lc1', []):
        squads.setdefault((str(s.get('U9ABSYEp')), str(s.get('zsiAn9P1'))), []).append(s)
    for key, slots in sorted(squads.items()):
        names = [str(s.get('edy7fq3L')) for s in slots]
        missing = [n for n in names if n not in units]
        if missing:
            problems.append(f'squad {key} names units the roster lacks: {missing}')
        leaders = [s for s in slots if str(s.get('gr48vsdJ')) == '0']
        if len(leaders) != 1:
            problems.append(f'squad {key} has {len(leaders)} leaders')
        if len(set(names)) != len(names):
            problems.append(f'squad {key} names a unit twice: {names}')
        positions = [str(s.get('XuJL4pc5')) for s in slots]
        if len(set(positions)) != len(positions):
            problems.append(f'squad {key} uses a slot twice: {positions}')
    team = (user_info.get('fEi17cnx') or [{}])[0]
    active = str(team.get('Z0Y4RoD7', '0'))
    leader = [str(s.get('edy7fq3L')) for s in squads.get(('1', active), []) if str(s.get('gr48vsdJ')) == '0']
    if not leader:
        problems.append(f'active squad (1, {active}) is missing or has no leader')
    elif leader[0] not in units:
        problems.append(f'active squad (1, {active}) is led by {leader[0]}, which the roster lacks')
    return problems


def dangling_unit_references(db: sqlite3.Connection, user_id: str) -> dict:
    """{table: rows} of a user's rows that name a user_unit_id the user does not own."""
    owned = 'SELECT user_unit_id FROM user_units WHERE user_id = ?'
    queries = {
        'user_decks': f'SELECT deck_type, deck_num, disp_order, user_unit_id FROM user_decks'
                      f' WHERE user_id = ? AND user_unit_id NOT IN ({owned})',
        'user_campaign_decks': f'SELECT deck_num, disporder, user_unit_id FROM user_campaign_decks'
                               f' WHERE user_id = ? AND user_unit_id NOT IN ({owned})',
        'user_unit_dbb': f'SELECT user_unit_id, bonded_user_unit_id FROM user_unit_dbb WHERE user_id = ?'
                         f' AND (user_unit_id NOT IN ({owned}) OR bonded_user_unit_id NOT IN ({owned}))',
        'user_warehouse_rows': f'SELECT instance_id, item_id, equip_unit_id FROM user_warehouse_rows'
                               f' WHERE user_id = ? AND equip_unit_id != 0 AND equip_unit_id NOT IN ({owned})',
    }
    out = {}
    for table, sql in queries.items():
        args = (user_id, user_id) if table != 'user_unit_dbb' else (user_id, user_id, user_id)
        rows = db.execute(sql, args).fetchall()
        if rows:
            out[table] = [tuple(r) for r in rows]
    return out
