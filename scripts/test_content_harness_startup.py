"""Mocked startup/cleanup tests for the content-provider acceptance harness.

    python scripts/test_content_harness_startup.py

No server, client, package or port is touched: the harness's real readiness
and cleanup code (bf_testkit.IsolatedServer and content_provider_client_test's
serve / cmd_server) runs against a fake world -- a clock, a process table and
a listener table rendered as real `netstat -ano` text, so the real parsers run
too.  Every other process call (taskkill, Get-AppxPackage, CheckNetIsolation,
a real Popen or urlopen) trips an assertion instead of happening.

Covers: normal startup (READY only after exclusive ownership bracketed around
the endpoint answers), timeout with a live child that never listens,
premature exit, foreign / duplicate listeners before and after start, wrong
endpoint answers, interruption before and after readiness, loss of exclusive
ownership while serving, the live-port guards, the recycled-pid child filter
and the harness's pre-start refusals.

Session 16: server --manifest (refused without --policy, for another format or
client build, or without a layout EXE; installed byte for byte otherwise; a
stale copy never survives a start without it), the read-only coverage command
(complete / incomplete / --expect; every unreadable answer -- 404, truncated,
bfcp 1, no verdict, nothing listening -- is never complete) and the c16
identity and its candidate DLLs.
"""
from __future__ import annotations

import contextlib
import hashlib
import io
import json
import re
import shutil
import subprocess
import sys
import types
import urllib.request
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent))
import bf_testkit  # noqa: E402
import content_provider_client_test as harness  # noqa: E402
from bf_testkit import ROOT, Checker, Fixture, IsolatedServer, ServerStartError, qa_dir  # noqa: E402

PORT = harness.ISOLATED_PORT
LIVE = bf_testkit.LIVE_PORT
BUILD_ID = 'TEST1234-00ABCDEF-00123456'     # a layout EXE's build identity, as the harness reads it
PLAYER_SERVER = 4242          # the player's normal server: must never be stopped
OWNED = 21
STATUS_READY = 'bfcp 2\nenabled 1\nready 1\nepoch e1\ngeneration 3\nfiles 21\nowned 21\ncollisions 0\nunavailable 0\nend\n'
STATUS_INDEXING = STATUS_READY.replace('ready 1', 'ready 0')
STATUS_DORMANT = 'bfcp 2\nenabled 0\nready 0\nepoch e1\ngeneration 0\nfiles 0\nowned 0\ncollisions 0\nunavailable 0\nreason disabled\nend\n'
GOOD = {'/offline_mod/fps_cap': (200, '60'), '/offline_mod/content/v1/status': (200, STATUS_READY)}


class FakeProc:
    def __init__(self, pid: int):
        self.pid, self.returncode = pid, None

    def poll(self):
        return self.returncode

    def wait(self, timeout=None):
        return self.returncode


class World:
    """The fake OS: clock, processes, listeners and who answers on a port."""

    def __init__(self):
        self.t = 1_700_000_000.0
        self.listen = {}             # port -> {pid}
        self.answers = {}            # pid -> {path: (status, body)}
        self.procs = {}              # pid -> FakeProc
        self.children = []           # (parent pid, pid, created epoch seconds)
        self.timeline = []           # (at, fn)
        self.trace = []
        self.killed = []
        self.spawned = []
        self.interrupt_at = None
        self.next_pid = 7000
        self.behavior = None         # fn(world, proc) at spawn
        # Watchdog: a loop that never ends (a serving loop nobody stops, a
        # startup wait that never gives up) gets an operator Ctrl+C after two
        # fake hours, so it shows up as a failed case instead of a hang.
        self.horizon = self.t + 7200.0

    def new_pid(self) -> int:
        self.next_pid += 1
        return self.next_pid

    def at(self, delay: float, fn):
        self.timeline.append((self.t + delay, fn))

    def advance(self, dt: float):
        self.t += dt
        due = sorted((e for e in self.timeline if e[0] <= self.t), key=lambda e: e[0])
        self.timeline = [e for e in self.timeline if e[0] > self.t]
        for _, fn in due:
            fn()
        if self.interrupt_at is not None and self.t >= self.interrupt_at:
            self.interrupt_at = None
            self.trace.append(('ctrl-c',))
            raise KeyboardInterrupt
        if self.t > self.horizon:
            self.horizon = float('inf')
            self.trace.append(('watchdog',))
            raise KeyboardInterrupt

    # helpers for behaviours
    def bind(self, pid: int, port: int = PORT):
        self.listen.setdefault(port, set()).add(pid)

    def unbind(self, pid: int):
        for pids in self.listen.values():
            pids.discard(pid)

    def exit(self, pid: int, code: int):
        self.procs[pid].returncode = code
        self.unbind(pid)

    # the OS seams the real code reaches
    def netstat_text(self) -> str:
        lines = ['', 'Active Connections', '', '  Proto  Local Address          Foreign Address        State           PID',
                 '  TCP    0.0.0.0:135            0.0.0.0:0              LISTENING       1488',
                 '  UDP    0.0.0.0:5353           *:*                                    2000']
        for port, pids in self.listen.items():
            for i, pid in enumerate(sorted(pids)):
                addr = '127.0.0.1' if i % 2 == 0 else '[::]'
                lines.append(f'  TCP    {addr}:{port}        0.0.0.0:0              LISTENING       {pid}')
            lines.append(f'  TCP    127.0.0.1:{port}        127.0.0.1:50000        TIME_WAIT       0')
        return '\n'.join(lines) + '\n'

    def fake_run(self, cmd, *a, **k):
        if cmd[:2] == ['netstat', '-ano']:
            self.trace.append(('netstat',))
            return subprocess.CompletedProcess(cmd, 0, self.netstat_text(), '')
        if cmd[0] == 'powershell' and 'ParentProcessId=' in cmd[-1]:
            parent = int(re.search(r'ParentProcessId=(\d+)', cmd[-1]).group(1))
            out = ''.join(f'{pid} {int(created * 1000)}\n' for p, pid, created in self.children if p == parent)
            return subprocess.CompletedProcess(cmd, 0, out, '')
        raise AssertionError(f'unexpected real process call: {cmd}')


def make_server_class(world: World):
    class FakeServer(IsolatedServer):
        def _spawn(self):
            proc = FakeProc(world.new_pid())
            world.procs[proc.pid] = proc
            world.spawned.append(proc.pid)
            world.trace.append(('spawn', proc.pid))
            if world.behavior:
                world.behavior(world, proc)
            return proc

        def _kill(self, pid):
            world.killed.append(pid)
            world.trace.append(('kill', pid))
            if pid in world.procs and world.procs[pid].returncode is None:
                world.exit(pid, 1)

        def _probe(self, path):
            world.trace.append(('probe', path))
            pids = sorted(world.listen.get(self.fx.port, ()))
            if not pids:
                return None
            # With several listeners the worst case answers: the foreign one.
            responder = next((p for p in pids if p not in world.spawned), pids[0])
            return world.answers.get(responder, {}).get(path, (404, 'not found'))

        def _now(self):
            return world.t

        def _sleep(self, dt):
            world.advance(dt)
    return FakeServer


def fixture(name: str, port: int = PORT) -> Fixture:
    fx = Fixture(qa_dir('content_harness_startup') / name, port)
    fx.dir.mkdir(parents=True, exist_ok=True)
    fx.config.write_text(json.dumps({'plugins': [{'name': 'GimuServer', 'config': {'server': {'fps_cap': 60}}}]}),
                         encoding='utf-8')
    return fx


class Recorder(io.StringIO):
    """stdout that also records each printed line in the world's trace."""

    def __init__(self, world):
        super().__init__()
        self.world = world

    def write(self, s):
        if s.strip():
            self.world.trace.append(('print', s.strip()))
        return super().write(s)


@contextlib.contextmanager
def no_real_processes(world: World):
    def no_popen(*a, **k):
        raise AssertionError(f'unexpected real Popen: {a}')

    def no_urlopen(*a, **k):
        raise AssertionError(f'unexpected real HTTP request: {a}')
    with mock.patch.object(bf_testkit.subprocess, 'run', world.fake_run), \
            mock.patch.object(bf_testkit.subprocess, 'Popen', no_popen), \
            mock.patch.object(harness.subprocess, 'run', world.fake_run), \
            mock.patch.object(urllib.request, 'urlopen', no_urlopen):
        yield


def run_serve(world: World, *, broker: str = 'on', checks=None, name='case', allow_live=False, port=PORT,
              timeout=180.0):
    fx = fixture(name, port)
    cls = make_server_class(world)
    out = Recorder(world)
    with no_real_processes(world), contextlib.redirect_stdout(out):
        server = cls(Path(__file__), fx, checks=checks if checks is not None else
                     [bf_testkit.fps_cap_check(fx), harness.broker_check(broker, OWNED)],
                     allow_live_port=allow_live, timeout=timeout)
        code = harness.serve(server, broker=broker, poll=5.0)
    return code, out.getvalue(), server


def printed_ready(world) -> int | None:
    return next((i for i, e in enumerate(world.trace) if e[0] == 'print' and e[1].startswith('READY')), None)


def bracketed(world, ready_index: int, n_checks: int) -> bool:
    """Before READY: netstat (port), every check's probe, netstat (port), then at
    most the live-port netstat -- the answers sat between two exclusive views."""
    events = [e for e in world.trace[:ready_index] if e[0] in ('netstat', 'probe')]
    tail = events[-(n_checks + 3):]
    shapes = [e[0] for e in tail]
    return shapes in (['netstat'] + ['probe'] * n_checks + ['netstat', 'netstat'],
                      ['probe'] + ['netstat'] + ['probe'] * n_checks + ['netstat'])


def standard_child(bind_at=2.0, ready_at=3.0, console=True, port=PORT):
    def behavior(world, proc):
        world.answers[proc.pid] = {'/offline_mod/fps_cap': (200, '60'),
                                   '/offline_mod/content/v1/status': (200, STATUS_INDEXING)}
        if console:      # the server's own debug-console child, created after it
            world.at(0.2, lambda: world.children.append((proc.pid, proc.pid + 500, world.t)))
        world.at(bind_at, lambda: world.bind(proc.pid, port))
        world.at(ready_at, lambda: world.answers[proc.pid].update(GOOD))
    return behavior


def player_world() -> World:
    """A world where the player's normal server owns 9960 and an old process
    names a recycled pid as its parent (it must never be treated as ours)."""
    w = World()
    w.procs[PLAYER_SERVER] = FakeProc(PLAYER_SERVER)
    w.bind(PLAYER_SERVER, LIVE)
    w.answers[PLAYER_SERVER] = dict(GOOD)
    return w


def coverage_corpus(path: Path) -> list[tuple[str, str, str, str]]:
    """(name, expect, build, body) of every case in the shared coverage-report corpus."""
    cases, name, expect, build, body = [], None, None, None, None
    for line in path.read_text(encoding='utf-8').split('\n'):
        line = line.rstrip('\r')
        if body is not None:
            if line == '>>>':
                cases.append((name, expect, build, '\n'.join(body) + '\n'))
                body = None
            else:
                body.append(line)
        elif line.startswith('== '):
            name, expect, build = line[3:], None, None
        elif line.startswith('expect '):
            expect = line[7:]
        elif line.startswith('build '):
            build = line[6:]
        elif line == '<<<':
            body = []
    return cases


def main() -> int:
    check = Checker()

    # 1. normal startup, then Ctrl+C
    w = player_world()
    w.behavior = standard_child()
    w.at(60.0, lambda: setattr(w, 'interrupt_at', w.t + 15))
    code, out, server = run_serve(w, name='normal')
    child = w.spawned[0]
    r = printed_ready(w)
    check('normal: READY printed once', out.count('READY') == 1, out)
    check('normal: READY only after exclusive ownership bracketed the endpoint answers',
          r is not None and bracketed(w, r, 2), w.trace[:r][-8:] if r else w.trace[-8:])
    check('normal: READY names the pid, port and log', r is not None and f'pid {child}' in out
          and f'127.0.0.1:{PORT}' in out and str(server.log_path) in out, out)
    check('normal: not ready before the broker finished indexing (ready 0 kept it waiting)',
          sum(1 for e in w.trace[:r] if e == ('probe', '/offline_mod/content/v1/status')) >= 2, w.trace[:r])
    check('normal: Ctrl+C after READY exits 0', code == 0, (code, out))
    check('normal: stopped exactly the server and its own console child', sorted(w.killed) == sorted([child, child + 500]),
          w.killed)
    check("normal: the player's server was never touched", PLAYER_SERVER not in w.killed
          and w.procs[PLAYER_SERVER].returncode is None)

    # 2. timeout: a live child that never listens
    w = player_world()
    w.behavior = lambda world, proc: None
    code, out, server = run_serve(w, name='timeout')
    child = w.spawned[0]
    check('timeout: exits nonzero', code == 1, (code, out))
    check('timeout: says it never became ready, with the reason and the log path',
          'did not become ready within 180 s' in out and 'not listening' in out and str(server.log_path) in out, out)
    check('timeout: never printed READY', 'READY' not in out.replace('NOT READY', ''), out)
    check('timeout: stopped only its own child', w.killed == [child] and PLAYER_SERVER not in w.killed, w.killed)
    check('timeout: waited the full startup window (fake clock)', w.t - 1_700_000_000.0 >= 180.0, w.t)

    # 3. premature exit (its console child is still cleaned up)
    w = player_world()

    def dies(world, proc):
        world.at(0.2, lambda: world.children.append((proc.pid, proc.pid + 500, world.t)))
        world.at(1.0, lambda: world.exit(proc.pid, 3))
    w.behavior = dies
    code, out, server = run_serve(w, name='early_exit')
    child = w.spawned[0]
    check('early exit: exits nonzero with the exit code and log path',
          code == 1 and 'exited before it was ready (exit code 3)' in out and str(server.log_path) in out, out)
    check('early exit: never printed READY', 'READY' not in out.replace('NOT READY', ''), out)
    check('early exit: only its leftover console child was stopped (the dead server needs no kill)',
          w.killed == [child + 500], w.killed)

    # 4. a foreign server answers on the port while the child never binds
    w = player_world()
    foreign = 9100

    def foreign_takes_port(world, proc):
        world.answers[proc.pid] = dict(GOOD)
        world.procs[foreign] = FakeProc(foreign)
        world.answers[foreign] = dict(GOOD)        # even a perfect answer is not ours
        world.at(1.0, lambda: world.bind(foreign))
    w.behavior = foreign_takes_port
    code, out, server = run_serve(w, name='foreign')
    child = w.spawned[0]
    check('foreign listener: exits nonzero naming the foreign pid', code == 1 and 'foreign listener' in out
          and str(foreign) in out, out)
    check('foreign listener: never printed READY although the endpoint answered perfectly',
          'READY' not in out.replace('NOT READY', ''), out)
    check('foreign listener: stopped only its own child, never the foreign process',
          w.killed == [child] and foreign not in w.killed, w.killed)

    # 5. duplicate: the child and a foreign process both listen
    w = player_world()

    def duplicate(world, proc):
        world.answers[proc.pid] = dict(GOOD)
        world.procs[foreign] = FakeProc(foreign)
        world.answers[foreign] = dict(GOOD)
        world.at(1.0, lambda: (world.bind(proc.pid), world.bind(foreign)))
    w.behavior = duplicate
    code, out, server = run_serve(w, name='duplicate')
    child = w.spawned[0]
    check('duplicate listener: exits nonzero, called a duplicate', code == 1 and 'duplicate listener' in out, out)
    check('duplicate listener: never printed READY', 'READY' not in out.replace('NOT READY', ''), out)
    check('duplicate listener: stopped only its own child', w.killed == [child], w.killed)

    # 5b. a duplicate that appears between the answers and the second ownership view
    w = player_world()

    def late_duplicate(world, proc):
        world.answers[proc.pid] = dict(GOOD)
        world.procs[foreign] = FakeProc(foreign)
        world.at(1.0, lambda: world.bind(proc.pid))
    w.behavior = late_duplicate
    fx = fixture('late_duplicate')
    cls = make_server_class(w)

    class SneakyServer(cls):
        def _probe(self, path):
            got = super()._probe(path)
            if path == '/offline_mod/content/v1/status' and got and got[0] == 200:
                w.bind(foreign)                   # binds right after the answer
            return got
    out_io = Recorder(w)
    with no_real_processes(w), contextlib.redirect_stdout(out_io):
        s = SneakyServer(Path(__file__), fx, checks=[bf_testkit.fps_cap_check(fx), harness.broker_check('on', OWNED)])
        code = harness.serve(s, broker='on')
    out = out_io.getvalue()
    check('duplicate after the answers: the second ownership view catches it, no READY',
          code == 1 and 'duplicate listener' in out and 'READY' not in out.replace('NOT READY', ''), out)

    # 6. duplicate appears while serving
    w = player_world()

    def dup_later(world, proc):
        standard_child()(world, proc)
        world.procs[foreign] = FakeProc(foreign)
        world.at(40.0, lambda: world.bind(foreign))
    w.behavior = dup_later
    code, out, server = run_serve(w, name='dup_while_serving')
    child = w.spawned[0]
    check('lost exclusivity while serving: exits nonzero after READY', code == 1 and 'READY' in out
          and 'FAILED' in out and f'{foreign}' in out and 'also listen' in out, out)
    check('lost exclusivity while serving: tells the operator to close the test package',
          'close the test package now' in out, out)
    check('lost exclusivity while serving: stopped its server and console child only',
          sorted(w.killed) == sorted([child, child + 500]) and foreign not in w.killed, w.killed)

    # 7. endpoint answers wrongly until the timeout
    w = player_world()

    def dormant(world, proc):
        world.answers[proc.pid] = {'/offline_mod/fps_cap': (200, '60'),
                                   '/offline_mod/content/v1/status': (200, STATUS_DORMANT)}
        world.at(1.0, lambda: world.bind(proc.pid))
    w.behavior = dormant
    code, out, server = run_serve(w, name='broker_dormant', broker='on', timeout=30)
    check('wrong endpoint answer: broker requested on but dormant -> nonzero with the last status',
          code == 1 and 'broker not ready' in out and 'enabled 0' in out and 'reason disabled' in out, out)
    check('wrong endpoint answer: stopped only its child', w.killed == [w.spawned[0]], w.killed)

    w = player_world()

    def other_config(world, proc):
        world.answers[proc.pid] = {'/offline_mod/fps_cap': (200, '30'),
                                   '/offline_mod/content/v1/status': (200, STATUS_READY)}
        world.at(1.0, lambda: world.bind(proc.pid))
    w.behavior = other_config
    code, out, server = run_serve(w, name='other_config', timeout=20)
    check("wrong endpoint answer: an fps_cap from another config is not this fixture's server",
          code == 1 and "want 200 '60'" in out, out)

    w = player_world()

    def old_protocol(world, proc):
        world.answers[proc.pid] = {'/offline_mod/fps_cap': (200, '60'),
                                   '/offline_mod/content/v1/status': (200, 'bfcp 1\nenabled 1\nend\n')}
        world.at(1.0, lambda: world.bind(proc.pid))
    w.behavior = old_protocol
    code, out, server = run_serve(w, name='old_protocol', timeout=20)
    check('wrong endpoint answer: a bfcp 1 broker is refused', code == 1 and 'want 200 "bfcp 2"' in out, out)

    w = player_world()

    def missing_route(world, proc):
        world.answers[proc.pid] = {'/offline_mod/fps_cap': (200, '60')}      # status -> 404
        world.at(1.0, lambda: world.bind(proc.pid))
    w.behavior = missing_route
    code, out, server = run_serve(w, name='no_broker', timeout=20)
    check('wrong endpoint answer: a server without the broker (404) is refused', code == 1 and 'HTTP 404' in out, out)

    w = player_world()
    w.behavior = standard_child()
    w.at(60.0, lambda: setattr(w, 'interrupt_at', w.t + 1))

    def dormant_ok(world, proc):
        world.answers[proc.pid] = {'/offline_mod/fps_cap': (200, '60'),
                                   '/offline_mod/content/v1/status': (200, STATUS_DORMANT)}
        world.at(1.0, lambda: world.bind(proc.pid))
    w.behavior = dormant_ok
    code, out, server = run_serve(w, name='broker_off', broker='off')
    check('--broker off: a dormant broker is the expected state (READY)', code == 0 and 'READY' in out
          and 'broker off' in out, out)

    # 8. Ctrl+C before readiness
    w = player_world()
    w.behavior = lambda world, proc: world.at(0.2, lambda: world.children.append((proc.pid, proc.pid + 500, world.t)))
    w.interrupt_at = w.t + 5
    code, out, server = run_serve(w, name='interrupt_startup')
    child = w.spawned[0]
    check('interrupted during startup: exit 130, no READY', code == 130 and 'READY' not in out, out)
    check('interrupted during startup: stopped its server and console child only',
          sorted(w.killed) == sorted([child, child + 500]) and PLAYER_SERVER not in w.killed, w.killed)

    # 9. the server exits while serving; 10. it stops listening while alive
    w = player_world()

    def crashes_later(world, proc):
        standard_child(console=False)(world, proc)
        world.at(30.0, lambda: world.exit(proc.pid, 1))
    w.behavior = crashes_later
    code, out, server = run_serve(w, name='exit_while_serving')
    check('server exits while serving: nonzero, says so, nothing else stopped',
          code == 1 and 'the server exited (exit code 1)' in out and w.killed == [], (out, w.killed))

    w = player_world()

    def stops_listening(world, proc):
        standard_child(console=False)(world, proc)
        world.at(30.0, lambda: world.unbind(proc.pid))
    w.behavior = stops_listening
    code, out, server = run_serve(w, name='unbind_while_serving')
    check('server stops listening while serving: nonzero, says so, stops its own server',
          code == 1 and 'no longer listens' in out and w.killed == [w.spawned[0]], (out, w.killed))

    # 11. live-port guards
    w = player_world()
    w.behavior = standard_child()
    code, out, server = run_serve(w, name='suite_live', port=LIVE)
    check('suite mode never takes 9960: refused before spawning', code == 1 and 'refusing the live port' in out
          and w.spawned == [] and w.killed == [], out)

    w = World()
    w.behavior = standard_child(port=LIVE)
    w.at(60.0, lambda: setattr(w, 'interrupt_at', w.t + 1))
    code, out, server = run_serve(w, name='harness_live', port=LIVE, allow_live=True)
    check('switch-over mode: 9960 allowed when explicitly requested and free', code == 0 and 'READY' in out, out)

    w = player_world()

    def binds_live_too(world, proc):
        standard_child(console=False)(world, proc)
        world.at(2.5, lambda: world.bind(proc.pid, LIVE))
    w.behavior = binds_live_too
    code, out, server = run_serve(w, name='binds_live_too')
    check('isolated mode: a server that also binds 9960 is refused and stopped',
          code == 1 and 'also bound the live port 9960' in out and w.killed == [w.spawned[0]], out)

    w = player_world()
    w.bind(9100, PORT)
    w.procs[9100] = FakeProc(9100)
    w.behavior = standard_child()
    code, out, server = run_serve(w, name='port_busy')
    check('port busy before launch: refused, nothing started or stopped', code == 1 and 'already in use' in out
          and w.spawned == [] and w.killed == [], out)

    # 12. default checks (suites): fps_cap of the fixture's own config
    w = World()
    w.behavior = standard_child(console=False, port=19960)
    fx = fixture('suite_default', 19960)
    cls = make_server_class(w)
    with no_real_processes(w), contextlib.redirect_stdout(Recorder(w)):
        with cls(Path(__file__), fx) as s:
            ok_default = s.proc.pid == w.spawned[0] and [c[0] for c in s.checks] == ['/offline_mod/fps_cap']
    check('suite default: ready on the fixture fps_cap, stopped on exit', ok_default and w.killed == [w.spawned[0]],
          (w.killed, w.trace[-6:]))
    w = World()

    def no_bind(world, proc):
        world.answers[proc.pid] = dict(GOOD)
    w.behavior = no_bind
    cls = make_server_class(w)
    raised = None
    with no_real_processes(w), contextlib.redirect_stdout(Recorder(w)):
        try:
            with cls(Path(__file__), fixture('suite_timeout', 19960), timeout=10):
                pass
        except ServerStartError as e:
            raised = e
    check('suite default: a server that never listens raises ServerStartError (a RuntimeError) with the log',
          isinstance(raised, RuntimeError) and raised.log_path.name == 'server.log' and 'see ' in str(raised), raised)

    # 13. the real child filter and netstat parser
    w = World()
    w.children = [(7001, 8001, w.t - 3600.0), (7001, 8002, w.t + 0.5), (7002, 8003, w.t + 0.5)]
    with no_real_processes(w):
        kids = IsolatedServer._children(7001, w.t - 1.0)
    check('recycled-pid guard: only children created after the start are ours', kids == [8002], kids)
    w = World()
    w.bind(111, PORT)
    w.bind(222, PORT)
    w.bind(333, PORT + 1)
    with no_real_processes(w):
        got = IsolatedServer._listeners(PORT)
    check('netstat parser: IPv4 and IPv6 listeners of the port, not TIME_WAIT/UDP/other ports',
          got == {111, 222}, got)

    # 14. harness refusals before anything is written or started
    def run_cmd(world, *, layout, procs, port_arg=None, broker='on', extra=None, build=BUILD_ID, copytree=None):
        out_io = Recorder(world)
        args = types.SimpleNamespace(exe=str(Path(__file__)), broker=broker, port=port_arg, timeout=180.0,
                                     **(extra or {}))
        cls = make_server_class(world)
        created = []

        def fake_create(directory, port, **k):
            created.append(port)
            return fixture('cmd_server', port)
        # This fake broker advertises OWNED files. Do not scan the real asset
        # tree to derive its count; policy resolution has separate live tests.
        with no_real_processes(world), contextlib.redirect_stdout(out_io), \
                mock.patch.object(harness, 'layout_port', lambda: layout), \
                mock.patch.object(harness, 'layout_build', lambda: build), \
                mock.patch.object(harness, 'processes', lambda: procs), \
                mock.patch.object(harness, 'owned_targets', lambda: {str(i): '' for i in range(OWNED)}), \
                mock.patch.object(harness, 'WORK', qa_dir('content_harness_startup') / 'work'), \
                mock.patch.object(harness.Fixture, 'create', fake_create), \
                mock.patch.object(harness.shutil, 'copytree', copytree or harness.shutil.copytree), \
                mock.patch.object(harness, 'IsolatedServer', cls):
            code = harness.cmd_server(args)
        return code, out_io.getvalue(), created

    gimu = ('4242', 'gimuserverw.exe', r'C:\Users\Evan\BF\BF-WorkingDirRust\out\build\debug-win64\gimuserverw.exe')
    client = ('5151', 'BraveFrontier.Windows.exe',
              r'C:\Program Files\WindowsApps\gumi.BraveFrontier_2.19.6.0_x86__99p3jr0gh0z6w\BraveFrontier.Windows.exe')

    w = player_world()
    code, out, created = run_cmd(w, layout=(None, 'unknown DLL 0123'), procs=[])
    check('cmd_server: an unknown layout DLL is refused before anything starts',
          code == 1 and 'cannot tell which port' in out and not created and not w.spawned, out)
    w = player_world()
    code, out, created = run_cmd(w, layout=(PORT, 'isolated'), procs=[], port_arg=LIVE)
    check('cmd_server: --port that disagrees with the layout DLL is refused',
          code == 1 and 'talks to 29960' in out and not created, out)
    w = player_world()
    code, out, created = run_cmd(w, layout=(LIVE, 'reviewed'), procs=[gimu])
    check('cmd_server switch-over: 9960 in use by the normal server -> refused, nothing stopped',
          code == 1 and 'in use by pid(s) [4242]' in out and not created and w.killed == [], out)
    w = World()
    code, out, created = run_cmd(w, layout=(LIVE, 'reviewed'), procs=[client])
    check('cmd_server switch-over: the installed client running -> refused', code == 1 and 'Close these' in out
          and 'WindowsApps' in out and not created and not w.spawned, out)
    w = player_world()
    w.behavior = standard_child()
    w.at(60.0, lambda: setattr(w, 'interrupt_at', w.t + 1))
    code, out, created = run_cmd(w, layout=(PORT, 'isolated build'), procs=[gimu, client])
    check('cmd_server isolated: runs beside the normal server and the installed client, READY on 29960',
          code == 0 and created == [PORT] and 'READY' in out and 'may keep running' in out, out)
    check("cmd_server isolated: Ctrl+C stopped only the harness server's processes",
          sorted(w.killed) == sorted([w.spawned[0], w.spawned[0] + 500]) and PLAYER_SERVER not in w.killed, w.killed)

    # 15. Session 16: server --manifest (the broker's content_startup.json)
    scratch = qa_dir('content_harness_startup') / 's16'
    scratch.mkdir(parents=True, exist_ok=True)
    policy_file = scratch / 'policy.json'
    policy_file.write_text(json.dumps({'enabled': False, 'scope': ['a/x.png'], 'aliases': {}, 'extensions': ['.png'],
                                       'exclude': [], 'bundles': {'b': ['x.png']}}), encoding='utf-8')
    good_manifest = scratch / 'content_startup.json'
    good_manifest.write_text(json.dumps({'format': 'bfcp-startup 1', 'build': BUILD_ID, 'generated': 'test',
                                         'inputs': {}, 'sets': {'login': ['x.png']}, 'unknown': [],
                                         'excluded': {}, 'absent': {}}), encoding='utf-8')
    wrong_format = scratch / 'wrong_format.json'
    wrong_format.write_text(json.dumps({'format': 'bfcp-startup 0', 'build': BUILD_ID}), encoding='utf-8')
    other_build = scratch / 'other_build.json'
    other_build.write_text(json.dumps({'format': 'bfcp-startup 1', 'build': 'FFFFFFFF-00000000-00000000'}),
                           encoding='utf-8')
    system_dir = qa_dir('content_harness_startup') / 'work' / 'system'

    w = player_world()
    code, out, created = run_cmd(w, layout=(PORT, 'isolated'), procs=[], extra={'manifest': str(good_manifest)})
    check('--manifest without --policy is refused before anything starts',
          code == 1 and '--manifest needs --policy' in out and not created and not w.spawned, out)
    for label, bad, needle in (('another format', wrong_format, 'is not a startup manifest'),
                               ('another client build', other_build, 'was derived for client build'),
                               ('an unreadable file', scratch / 'missing.json', 'not a readable JSON file')):
        w = player_world()
        code, out, created = run_cmd(w, layout=(PORT, 'isolated'), procs=[],
                                     extra={'policy': str(policy_file), 'manifest': str(bad)})
        check(f'--manifest with {label} is refused before anything starts',
              code == 1 and needle in out and not created and not w.spawned, out)
    w = player_world()
    code, out, created = run_cmd(w, layout=(PORT, 'isolated'), procs=[], build=None,
                                 extra={'policy': str(policy_file), 'manifest': str(good_manifest)})
    check('--manifest without a layout EXE to compare builds with is refused',
          code == 1 and 'no client EXE in the layout' in out and not created, out)
    w = player_world()
    w.behavior = standard_child()
    w.at(60.0, lambda: setattr(w, 'interrupt_at', w.t + 1))
    code, out, created = run_cmd(w, layout=(PORT, 'isolated'), procs=[],
                                 extra={'policy': str(policy_file), 'manifest': str(good_manifest)})
    installed = system_dir / 'content_startup.json'
    check('--manifest with --policy: the manifest is installed byte for byte, then READY',
          code == 0 and 'READY' in out and installed.is_file()
          and installed.read_bytes() == good_manifest.read_bytes() and 'installed as' in out, out)

    real_copytree = shutil.copytree              # harness.shutil IS this module: keep the real one

    def planting_copytree(src, dst, *a, **k):
        result = real_copytree(src, dst, *a, **k)
        (Path(dst) / 'content_startup.json').write_text('{"format": "stale"}', encoding='utf-8')
        return result
    w = player_world()
    w.behavior = standard_child()
    w.at(60.0, lambda: setattr(w, 'interrupt_at', w.t + 1))
    code, out, created = run_cmd(w, layout=(PORT, 'isolated'), procs=[], extra={'policy': str(policy_file)},
                                 copytree=planting_copytree)
    check('--policy without --manifest: a startup manifest copied with deploy/system is removed (the broker'
          ' reports "no startup manifest")', code == 0 and not installed.exists() and 'no startup manifest' in out, out)

    # 16. Session 16: the read-only coverage command
    # Consistent reports, as the corrected broker writes them: complete = both sets fully owned, no
    # row, no reason; incomplete = one background name unowned, listed, with the reason.
    input_row = 'I\tlocalstate\tVer1_Fixture1.dat\tsha256:' + 'c' * 64 + '\n'
    complete = ('bfcp 2\ncoverage 1\nenabled 1\nready 1\nepoch e1\ngeneration 3\npolicy sha256:aa\nmanifest sha256:bb\n'
                f'manifest_build {BUILD_ID}\nclient_build {BUILD_ID}\nbuild_match 1\nunknown 0\nexcluded 1\n'
                'inputs 1\n' + input_row +
                'set background required 5 ok 5 absent 0 unowned 0 missing 0 ambiguous 0 unavailable 0\n'
                'set login required 3 ok 3 absent 0 unowned 0 missing 0 ambiguous 0 unavailable 0\ncomplete 1\nend\n')
    incomplete = (complete.replace('complete 1\n', 'complete 0\nreason set background: 1 unowned\n'
                                   'P\tbackground\tunowned\tbroken.sam\n')
                  .replace('set background required 5 ok 5 absent 0 unowned 0',
                           'set background required 5 ok 4 absent 0 unowned 1'))

    def run_coverage(answer, expect=None, layout=(PORT, 'isolated'), build=BUILD_ID):
        asked = []

        def fake_fetch(port, b):
            asked.append((port, b))
            return answer
        out_io = io.StringIO()
        with contextlib.redirect_stdout(out_io), \
                mock.patch.object(harness, 'layout_port', lambda: layout), \
                mock.patch.object(harness, 'layout_build', lambda: build), \
                mock.patch.object(harness, 'fetch_coverage', fake_fetch), \
                mock.patch.object(urllib.request, 'urlopen', lambda *a, **k: (_ for _ in ()).throw(
                    AssertionError('unexpected real HTTP request'))):
            code = harness.cmd_coverage(types.SimpleNamespace(expect=expect))
        return code, out_io.getvalue(), asked

    code, out, asked = run_coverage((200, complete), 'complete')
    check('coverage: complete + --expect complete exits 0, asked for the layout build on its port',
          code == 0 and 'startup coverage for build ' + BUILD_ID in out and ': complete' in out
          and asked == [(PORT, BUILD_ID)] and 'leaves the LocalState census out' in out, out)
    code, out, _ = run_coverage((200, incomplete), 'complete')
    check('coverage: incomplete + --expect complete exits 1 with the reason and every problem name',
          code == 1 and 'set background: 1 unowned' in out and 'problem: background unowned broken.sam' in out
          and 'EXPECTED complete, got incomplete' in out and 'builds the census' in out, out)
    code, out, _ = run_coverage((200, incomplete))
    check('coverage: without --expect the report is read-only information (exit 0)', code == 0 and 'incomplete' in out, out)
    code, out, _ = run_coverage((200, incomplete), 'incomplete')
    check('coverage: incomplete + --expect incomplete exits 0', code == 0, out)
    for label, answer in (('HTTP 404 (an older broker)', (404, '')), ('no end line (truncated)', (200, complete[:-4])),
                          ('the bfcp 1 protocol', (200, complete.replace('bfcp 2', 'bfcp 1'))),
                          ('no complete line', (200, complete.replace('complete 1\n', ''))),
                          ('nothing listening', (0, 'connection refused')),
                          ('QC: a bare "complete 1" without sets or header',
                           (200, 'bfcp 2\ncoverage 1\ncomplete 1\ngeneration 1\nend\n')),
                          ('the first Session 16 broker (accepted-absent A rows)',
                           (200, complete.replace('complete 1\n', 'complete 1\nA\tlogin\tgone.png\n'))),
                          ('a Session 17 broker (no inputs field)',
                           (200, complete.replace('inputs 1\n' + input_row, ''))),
                          ('an input row leaving its root',
                           (200, complete.replace('Ver1_Fixture1.dat', '../Ver1_Fixture1.dat')))):
        code, out, _ = run_coverage(answer, 'complete')
        check(f'coverage: {label} is not readable -> exit 1, never complete',
              code == 1 and 'NOT READABLE' in out and ': complete' not in out, out)
    for label, answer in (('only the login set', complete.replace(
                               'set background required 5 ok 5 absent 0 unowned 0 missing 0 ambiguous 0 unavailable 0\n', '')),
                          ('accepted absent names', complete.replace('set login required 3 ok 3 absent 0',
                                                                     'set login required 3 ok 2 absent 1')),
                          ('another client build', complete.replace(f'client_build {BUILD_ID}', 'client_build OTHER')
                           .replace(f'manifest_build {BUILD_ID}', 'manifest_build OTHER')),
                          ('no client file named', complete.replace('inputs 1\n' + input_row, 'inputs 0\n'))):
        code, out, _ = run_coverage((200, answer), 'complete')
        check(f"coverage: a report that CLAIMS complete with {label} is incomplete (the DLL's rule) -> exit 1",
              code == 1 and ': incomplete -- ' in out and 'EXPECTED complete, got incomplete' in out
              and 'the report itself says complete 1' in out, out)

    # The DLL and the harness judge the same corpus alike (offline-proxy/tests/fixtures/coverage_reports.txt,
    # also read by content_provider_tests TestCoverageCorpus).
    corpus = ROOT.parent / 'offline-proxy' / 'tests' / 'fixtures' / 'coverage_reports.txt'
    cases = coverage_corpus(corpus)
    wrong = []
    for name, expect, build, body in cases:
        r = harness.parse_coverage(body, build)
        got = 'unreadable' if r is None else ('complete' if r['proved'] else 'incomplete')
        if got != expect:
            wrong.append((name, expect, got, r and r['why']))
    check(f'corpus parity: the harness parser judges all {len(cases)} shared reports as the DLL must',
          len(cases) >= 25 and not wrong, wrong)
    code, out, asked = run_coverage((200, complete), 'complete', layout=(None, 'unknown DLL 0123'))
    check('coverage: an unknown layout DLL is refused without a request', code == 1 and 'cannot tell which port' in out
          and not asked, out)
    code, out, asked = run_coverage((200, complete), 'complete', build=None)
    check('coverage: no layout EXE -> refused without a request', code == 1 and 'no client EXE' in out and not asked, out)

    # 17. Session 16: the c16 identity and its candidate DLL
    try:
        harness.select_identity('c16')
        c16 = (harness.WORK, harness.TEST_PFN, harness.LAYOUT_RECORD, harness.LABEL)
    finally:
        harness.select_identity('test')
    check('c16: work folder under out/qa/current/session16, its own package family and label',
          c16 == (qa_dir('session16') / 'content_client_c16', 'gumi.BraveFrontier.ContentTestC16_99p3jr0gh0z6w',
                  qa_dir('session16') / 'content_client_c16' / 'layout.json', '[ContentTest C16]'), c16)
    check('c1 keeps its accepted work folder', harness.IDENTITIES['c1'][2] == 'content_client_c1')
    s17 = harness.KNOWN_DLLS.get(harness.S17_ISOLATED_DLL_SHA256, {})
    check('the Session 17 isolated candidate is known for port 29960 with the audio hook and the startup path',
          s17.get('port') == PORT and s17.get('audio') is True and s17.get('startup') is True
          and 'Session 17' in s17.get('label', '') and 'SUPERSEDED' not in s17.get('label', ''), s17)
    check('the Session 17 live-port twin is known for 9960 (switch-over only)',
          harness.KNOWN_DLLS.get(harness.S17_LIVE_DLL_SHA256, {}).get('port') == LIVE)
    s18 = harness.KNOWN_DLLS.get(harness.S18_ISOLATED_DLL_SHA256, {})
    check('the Session 18 isolated candidate is known for port 29960 with the audio hook and the startup path',
          s18.get('port') == PORT and s18.get('audio') is True and s18.get('startup') is True
          and 'Session 18' in s18.get('label', '') and 'SUPERSEDED' not in s18.get('label', '')
          and harness.S18_ISOLATED_DLL_SHA256 != harness.S17_ISOLATED_DLL_SHA256, s18)
    check('the Session 18 live-port twin is known for 9960 (switch-over only)',
          harness.KNOWN_DLLS.get(harness.S18_LIVE_DLL_SHA256, {}).get('port') == LIVE)
    for digest, folder in ((harness.S18_ISOLATED_DLL_SHA256, 'proxy_29960_s18'), (harness.S18_LIVE_DLL_SHA256, 'proxy_9960_s18')):
        built = qa_dir('session18') / folder / 'libcurl.dll'
        check(f'the registered Session 18 hash is the staged {folder} DLL',
              built.is_file() and hashlib.sha256(built.read_bytes()).hexdigest() == digest, built)
    s16 = harness.KNOWN_DLLS.get(harness.S16_ISOLATED_DLL_SHA256, {})
    check('the Session 16 corrected build is still known (the C16 layout can be swapped off it) but marked superseded:'
          ' its startup path declined on the client after the provider\'s own detour',
          s16.get('port') == PORT and 'SUPERSEDED' in s16.get('label', '')
          and 'SUPERSEDED' in harness.KNOWN_DLLS.get(harness.S16_LIVE_DLL_SHA256, {}).get('label', '')
          and harness.S16_ISOLATED_DLL_SHA256 != harness.S17_ISOLATED_DLL_SHA256, s16)
    old16 = harness.KNOWN_DLLS.get(harness.S16_PRECORRECTION_ISOLATED_DLL_SHA256, {})
    check('the pre-correction Session 16 build is still known (a layout can be swapped off it) but marked superseded',
          old16.get('port') == PORT and 'SUPERSEDED' in old16.get('label', '')
          and harness.S16_PRECORRECTION_ISOLATED_DLL_SHA256 != harness.S16_ISOLATED_DLL_SHA256, old16)

    return check.summary('content harness startup (mocked)')


if __name__ == '__main__':
    sys.exit(main())
