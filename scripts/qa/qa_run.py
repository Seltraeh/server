"""Safety layer for isolated client runs: run names, containment, process identity and run records.

Used by prep_run.py, prep_chain.py and run_ctl.py beside it (usage: scripts/QA_RUN_HELPERS.md).  Replaces the
round-2 helpers under out/qa/current/fresh_round2_20261029/tools, which the round-2 QC found too broad:
they stopped every process matching an executable or script name, interpolated unchecked run names into
paths, and guarded safety with Python asserts (gone under -O).  Everything here raises QaError instead.

  * A run name is one identifier ([A-Za-z0-9][A-Za-z0-9_-]{0,62}); its folder must be <qa dir>/visual/saves/
    <name> with no link at visual, saves or the run itself, and the qa dir a folder below out/qa/current --
    checked after resolving, so a junction or "..", rooted or drive-qualified name cannot escape.
  * Every file a helper (or the server it starts) writes in a run is a run_file: absent, or a plain regular
    file with one link -- never written through a symlink, junction or shared hard link.  A run's config must
    name exactly its own gme.sqlite.
  * A process is identified by pid + creation time (FILETIME, from the process handle) + image path.  Every
    stop re-opens the pid, compares that identity on the SAME handle it then terminates through, and refuses
    anything that differs (a reused pid, another run's process).  Command lines are checked from a CIM
    snapshot tied to the same creation time.
"""
from __future__ import annotations

import ctypes
import ctypes.wintypes as wt
import datetime
import json
import os
import re
import stat
import subprocess
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
QA_PARENT = (REPO / 'out' / 'qa' / 'current').resolve()
LIVE_SAVE = (REPO / 'deploy' / 'gme.sqlite').resolve()
RUN_NAME = re.compile(r'[A-Za-z0-9][A-Za-z0-9_-]{0,62}')
RECORD = 'run.json'
FORMAT = 'bf-qa-run 1'


class QaError(Exception):
    """A refused or failed QA operation; the message says what was checked."""


# ---------------------------------------------------------------- names and containment

def validate_run_name(name) -> str:
    if not isinstance(name, str) or not RUN_NAME.fullmatch(name):
        raise QaError(f'run name {name!r} is not one identifier ([A-Za-z0-9][A-Za-z0-9_-]{{0,62}})')
    return name


def qa_dir(path) -> Path:
    """A round's QA folder: a folder strictly below out/qa/current (resolved, so links cannot escape)."""
    p = Path(path)
    if not p.is_absolute():
        p = REPO / p
    p = p.resolve()
    if p == QA_PARENT or QA_PARENT not in p.parents:
        raise QaError(f'{p}: a QA folder must lie below {QA_PARENT}')
    return p


def saves_root(qa: Path) -> Path:
    return qa / 'visual' / 'saves'


_REPARSE_POINT = 0x400      # FILE_ATTRIBUTE_REPARSE_POINT: symlinks, junctions and other redirections


def is_link(path: Path) -> bool:
    """True for a symlink, a junction or any other reparse point (lstat, so the link itself is examined)."""
    try:
        st = os.lstat(path)
    except FileNotFoundError:
        return False
    return stat.S_ISLNK(st.st_mode) or bool(getattr(st, 'st_file_attributes', 0) & _REPARSE_POINT)


def _same(a: Path, b: Path) -> bool:
    return os.path.normcase(str(a)) == os.path.normcase(str(b))


def run_dir(qa: Path, name: str, *, must_exist: bool) -> Path:
    """<qa>/visual/saves/<name>, with no link anywhere between the validated qa dir and the run.

    The resolved saves root must BE <qa>/visual/saves: checking only that the run is a direct child of the
    resolved root (round 3) let a junction at visual or saves carry every run outside the qa dir while the
    child check still passed (round-3 QC, P2)."""
    validate_run_name(name)
    qa = qa_dir(qa)
    expected = saves_root(qa)
    for part in (qa / 'visual', expected):
        if is_link(part):
            raise QaError(f'{part} is a link: refusing a saves root that could lead outside {qa}')
    root = expected.resolve()
    if not _same(root, expected):
        raise QaError(f'saves root {expected} resolves to {root}, outside {qa}')
    d = root / name
    if is_link(d) or not _same(d.resolve(), d):
        raise QaError(f'run {name!r} resolves to {d.resolve()}, not {d}: a link would redirect the run')
    if must_exist and not d.is_dir():
        raise QaError(f'no run folder {d}')
    return d


def run_file(run: Path, name: str, *, must_exist: bool = False) -> Path:
    """A file directly inside a validated run folder that a helper -- or the server it starts -- writes.

    Absent is fine (it will be created there) unless must_exist.  An existing one must be a plain regular
    file with one link: a symlink, junction or other reparse point would redirect the write, and a hard
    link shares its bytes with another path (another run's save, say), so writing it changes that file too.
    """
    if not isinstance(name, str) or name in ('', '.', '..') or any(c in name for c in '/\\:'):
        raise QaError(f'{name!r} is not one file name')
    p = run / name
    if not os.path.lexists(p):
        if must_exist:
            raise QaError(f'{p} does not exist')
        return p
    if is_link(p):
        raise QaError(f'{p} is a link: refusing to write through it')
    st = os.lstat(p)
    if not stat.S_ISREG(st.st_mode):
        raise QaError(f'{p} is not a regular file')
    if not _same(p.resolve(), p):
        raise QaError(f'{p} resolves to {p.resolve()}: refusing a redirected write')
    if st.st_nlink > 1:
        raise QaError(f'{p} has {st.st_nlink} hard links: writing it would also change another file')
    return p


def check_run_config(run: Path, config: Path) -> Path:
    """The run's config must name exactly one database, and it must be this run's own gme.sqlite (a
    validated run_file).  The server writes wherever the config points, so this is checked before anything
    starts: a config edited to name another save -- the live one included -- is refused."""
    run_file(run, Path(config).name, must_exist=True)
    try:
        cfg = json.loads(Path(config).read_text(encoding='utf-8'))
    except (OSError, ValueError) as ex:
        raise QaError(f'{config}: not a readable JSON config ({ex})')
    dbs = cfg.get('db_clients') if isinstance(cfg, dict) else None
    if not isinstance(dbs, list) or len(dbs) != 1 or not isinstance(dbs[0], dict):
        raise QaError(f'{config}: must name exactly one database')
    named = Path(str(dbs[0].get('filename', '')))
    own = run_file(run, 'gme.sqlite', must_exist=True)
    if not named.is_absolute() or not _same(named.resolve(), own):
        raise QaError(f'{config} names database {named}, not this run\'s {own}')
    return own


def require_inside(path: Path, folder: Path, what: str) -> Path:
    p = Path(path).resolve()
    if folder.resolve() not in p.parents:
        raise QaError(f'{what} {p} is not inside {folder}')
    return p


def posix_abs(path: Path) -> str:
    """Forward-slash absolute path for drogon configs (the October 27 harness's backslashes were stripped)."""
    s = Path(path).resolve().as_posix()
    if '\\' in s or not Path(s).is_absolute():
        raise QaError(f'not a forward-slash absolute path: {s}')
    return s


# ---------------------------------------------------------------- Win32 process identity

_k32 = ctypes.WinDLL('kernel32', use_last_error=True)
_k32.OpenProcess.argtypes = (wt.DWORD, wt.BOOL, wt.DWORD)
_k32.OpenProcess.restype = wt.HANDLE
_k32.CloseHandle.argtypes = (wt.HANDLE,)
_k32.GetProcessTimes.argtypes = (wt.HANDLE,) + (ctypes.POINTER(wt.FILETIME),) * 4
_k32.QueryFullProcessImageNameW.argtypes = (wt.HANDLE, wt.DWORD, wt.LPWSTR, ctypes.POINTER(wt.DWORD))
_k32.GetExitCodeProcess.argtypes = (wt.HANDLE, ctypes.POINTER(wt.DWORD))
_k32.TerminateProcess.argtypes = (wt.HANDLE, wt.UINT)
_k32.WaitForSingleObject.argtypes = (wt.HANDLE, wt.DWORD)
_k32.WaitForSingleObject.restype = wt.DWORD
_QUERY, _TERMINATE, _SYNC = 0x1000, 0x0001, 0x00100000
_STILL_ACTIVE = 259


class Proc:
    """An open handle to one process, with its identity read through that handle."""

    def __init__(self, pid: int, terminate: bool = False):
        self.pid = int(pid)
        access = _QUERY | _SYNC | (_TERMINATE if terminate else 0)
        self.h = _k32.OpenProcess(access, False, self.pid)
        if not self.h and terminate:      # an AppContainer process may refuse TERMINATE: query only
            self.h = _k32.OpenProcess(_QUERY | _SYNC, False, self.pid)

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        if self.h:
            _k32.CloseHandle(self.h)
            self.h = None

    @property
    def alive(self) -> bool:
        code = wt.DWORD()
        return bool(self.h) and _k32.GetExitCodeProcess(self.h, ctypes.byref(code)) and code.value == _STILL_ACTIVE

    def created(self) -> int | None:
        c, e, k, u = wt.FILETIME(), wt.FILETIME(), wt.FILETIME(), wt.FILETIME()
        if not self.h or not _k32.GetProcessTimes(self.h, ctypes.byref(c), ctypes.byref(e), ctypes.byref(k),
                                                  ctypes.byref(u)):
            return None
        return (c.dwHighDateTime << 32) | c.dwLowDateTime

    def image(self) -> str | None:
        buf, size = ctypes.create_unicode_buffer(1024), wt.DWORD(1024)
        if not self.h or not _k32.QueryFullProcessImageNameW(self.h, 0, buf, ctypes.byref(size)):
            return None
        return buf.value

    def terminate(self) -> bool:
        return bool(self.h) and bool(_k32.TerminateProcess(self.h, 1))

    def wait(self, seconds: float) -> bool:
        """True when the process has exited within the time."""
        return not self.h or _k32.WaitForSingleObject(self.h, int(seconds * 1000)) == 0


def identity(pid: int) -> dict | None:
    """{'pid', 'created', 'image'} of a running process, or None."""
    with Proc(pid) as p:
        if not p.alive:
            return None
        return {'pid': int(pid), 'created': p.created(), 'image': p.image()}


def same_image(a: str | None, b: str | None) -> bool:
    return bool(a and b) and os.path.normcase(os.path.abspath(a)) == os.path.normcase(os.path.abspath(b))


def snapshot(names: list[str], ports: list[int]) -> dict:
    """CIM rows {pid, ppid, image, cmd, created} for the named images, and the TCP listeners on the ports."""
    filt = ' OR '.join(f"Name = '{n}'" for n in names) or "Name = ''"
    plist = ','.join(str(int(p)) for p in ports) or '0'
    script = (
        f'$p = @(Get-CimInstance Win32_Process -Filter "{filt}" | ForEach-Object {{ [pscustomobject]@{{ '
        'pid = [int]$_.ProcessId; ppid = [int]$_.ParentProcessId; image = $_.ExecutablePath; '
        'cmd = $_.CommandLine; created = $(if ($_.CreationDate) { $_.CreationDate.ToFileTimeUtc() } else { 0 }) } }); '
        f'$l = @(Get-NetTCPConnection -State Listen -LocalPort {plist} -ErrorAction SilentlyContinue | '
        'ForEach-Object { [pscustomobject]@{ addr = $_.LocalAddress; port = [int]$_.LocalPort; '
        'pid = [int]$_.OwningProcess } }); '
        '@{ processes = $p; listeners = $l } | ConvertTo-Json -Depth 3 -Compress')
    out = subprocess.run(['powershell', '-NoProfile', '-NonInteractive', '-Command', script],
                         capture_output=True, text=True, timeout=120)
    if out.returncode != 0:
        raise QaError(f'process snapshot failed: {out.stderr.strip()[:300]}')
    data = json.loads(out.stdout or '{}')
    as_list = lambda v: [] if v is None else (v if isinstance(v, list) else [v])
    return {'processes': as_list(data.get('processes')), 'listeners': as_list(data.get('listeners'))}


def cim_row(snap: dict, pid: int, created: int) -> dict | None:
    """The snapshot row of this pid, only if its creation time is this one (CIM keeps microseconds)."""
    for r in snap['processes']:
        if r['pid'] == pid and created and abs(int(r['created']) - created) < 100:
            return r
    return None


def listener_owners(snap: dict, port: int) -> set[int]:
    return {r['pid'] for r in snap['listeners'] if r['port'] == port}


# ---------------------------------------------------------------- run records

def now_iso() -> str:
    return datetime.datetime.now().astimezone().isoformat(timespec='seconds')


def load_record(run: Path) -> dict:
    path = run / RECORD
    if not path.is_file():
        raise QaError(f'no {RECORD} in {run}: nothing recorded as started by this run')
    rec = json.loads(path.read_text(encoding='utf-8'))
    if rec.get('format') != FORMAT:
        raise QaError(f'{path}: not a {FORMAT} record')
    return rec


def save_record(run: Path, rec: dict) -> None:
    tmp = run_file(run, RECORD + '.tmp')
    run_file(run, RECORD)
    tmp.write_text(json.dumps(rec, indent=1), encoding='utf-8')
    os.replace(tmp, run / RECORD)


def verify_recorded(entry: dict, snap: dict | None = None) -> tuple[str, str]:
    """('match' | 'gone' | 'mismatch', detail) for one recorded process, read through a fresh handle."""
    with Proc(entry['pid']) as p:
        if not p.alive:
            return 'gone', 'not running'
        created, image = p.created(), p.image()
    if created != entry['created']:
        return 'mismatch', f'pid {entry["pid"]} was created at {created}, the record says {entry["created"]} (pid reuse?)'
    if not same_image(image, entry['image']):
        return 'mismatch', f'pid {entry["pid"]} runs {image}, the record says {entry["image"]}'
    needs = entry.get('cmdline_has') or []
    if needs:
        row = cim_row(snap, entry['pid'], created) if snap else None
        if row is None:
            return 'mismatch', f'pid {entry["pid"]}: no command line to check'
        cmd = os.path.normcase(row.get('cmd') or '')
        missing = [n for n in needs if os.path.normcase(n) not in cmd]
        if missing:
            return 'mismatch', f'pid {entry["pid"]} command line lacks {missing}'
    return 'match', 'recorded identity holds'


_k32.CreateFileW.argtypes = (wt.LPCWSTR, wt.DWORD, wt.DWORD, wt.LPVOID, wt.DWORD, wt.DWORD, wt.HANDLE)
_k32.CreateFileW.restype = wt.HANDLE
_INVALID = ctypes.c_void_p(-1).value


def db_probe(path: Path) -> str:
    """'free' when an open that shares nothing succeeds, 'held' when it is refused (or 'missing').  SQLite
    shares its file for read and write, so 'held' means SOME process has the file open; the probe cannot
    say which.  Run it before a start (expect 'free') and after (expect 'held') and say exactly that."""
    if not Path(path).is_file():
        return 'missing'
    h = _k32.CreateFileW(str(path), 0x80000000, 0, None, 3, 0x80, None)   # GENERIC_READ, share none, OPEN_EXISTING
    if not h or h == _INVALID:
        return 'held'
    _k32.CloseHandle(h)
    return 'free'
