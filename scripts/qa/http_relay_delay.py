"""Logging TCP relay that DELAYS chosen replies: http_relay_delay.py LISTEN TARGET LOG DELAY_S [GROUP ...] [GROUP=KEY ...]

The Session 19 relay (out/qa/current/session19/visual/http_relay.py), plus two things:

  * Delay: when a request on a connection carries one of the GROUP ids in its plaintext envelope
    ("Hhgi79M1":"<group>"), the server's reply to it is held DELAY_S seconds before the first byte goes back
    to the client.  The request itself is forwarded at once, so the server handles it at the normal time;
    only the client waits longer.  Requests on one keep-alive connection are sequential, so "the next reply
    on this connection" is that reply.  First used for the tuto8 quest-select race (round 1, 2026-10-28):
    YPBU7MD8 = GetAchievementInfo, ynB7X5P9 = UpdateInfoLight.
  * Dump (round 4): for each GROUP=KEY argument, the request and the reply of that group are decrypted with
    KEY (the handler's AES key, as bf_testkit.KEYS) and appended as JSON lines to LOG.dump.jsonl -- the
    exact fields a client sent and received, e.g. MissionStart's drop_info and MissionEnd's unit_rewards.
    Bytes are forwarded unchanged; the dump reassembles HTTP messages (Content-Length or chunked, gzip) on a
    copy -- a request is queued just before it is forwarded, so its reply cannot overtake it; a reply is
    forwarded first -- and a message it cannot decode is logged and skipped, never blocking the relay.

Each delay is logged with its group.  Loopback only.  Maintained copy (round 3); run_ctl.py starts it and
records its identity.  With DELAY_S 0 and no GROUP it only logs.
"""
import base64
import gzip
import json
import re
import socket
import sys
import threading
import time

LISTEN, TARGET, LOG, DELAY = int(sys.argv[1]), int(sys.argv[2]), sys.argv[3], float(sys.argv[4])
GROUPS = [g.encode() for g in sys.argv[5:] if '=' not in g]
DUMP = {g.encode(): k.encode()[:16].ljust(16, b'\0') for g, k in (a.split('=', 1) for a in sys.argv[5:] if '=' in a)}
REQ = re.compile(rb'(GET|POST|PUT|DELETE|HEAD|OPTIONS|PATCH) (\S{1,400}) HTTP/1\.[01]\r\n')
RESP = re.compile(rb'HTTP/1\.[01] (\d{3})[^\r\n]{0,60}\r\n')
GROUP = re.compile(rb'"Hhgi79M1"\s*:\s*"([A-Za-z0-9]{1,16})"')
lock = threading.Lock()
counter = [0]


def log(line: str) -> None:
    stamp = time.strftime('%H:%M:%S') + f'.{int(time.time() * 1000) % 1000:03d}'
    with lock, open(LOG, 'a', encoding='utf-8') as f:
        f.write(f'{stamp} {line}\n')


def dump(record: dict) -> None:
    record['time'] = time.strftime('%H:%M:%S') + f'.{int(time.time() * 1000) % 1000:03d}'
    with lock, open(LOG + '.dump.jsonl', 'a', encoding='utf-8') as f:
        f.write(json.dumps(record, ensure_ascii=False) + '\n')


class Framer:
    """Splits one direction's byte stream into complete HTTP messages: (head, body)."""

    def __init__(self):
        self.buf = b''

    def feed(self, data: bytes) -> list:
        self.buf += data
        out = []
        while True:
            h = self.buf.find(b'\r\n\r\n')
            if h < 0:
                return out
            head, rest = self.buf[:h], self.buf[h + 4:]
            length = re.search(rb'(?im)^content-length:\s*(\d+)', head)
            if length:
                n = int(length.group(1))
                if len(rest) < n:
                    return out
                out.append((head, rest[:n]))
                self.buf = rest[n:]
            elif re.search(rb'(?im)^transfer-encoding:\s*chunked', head):
                body, i = b'', 0
                while True:
                    e = rest.find(b'\r\n', i)
                    if e < 0:
                        return out
                    size = int(rest[i:e].split(b';')[0] or b'0', 16)
                    if size == 0:
                        if len(rest) < e + 4:
                            return out
                        break
                    if len(rest) < e + 2 + size + 2:
                        return out
                    body += rest[e + 2:e + 2 + size]
                    i = e + 2 + size + 2
                out.append((head, body))
                self.buf = rest[e + 4:]
            else:
                out.append((head, b''))
                self.buf = rest


def decrypt(key: bytes, b64: str):
    from Crypto.Cipher import AES
    plain = AES.new(key, AES.MODE_ECB).decrypt(base64.b64decode(b64))
    return json.loads(plain[:-plain[-1]])


class Conn:
    def __init__(self, n: int):
        self.n = n
        self.hold = threading.Event()      # set: the next reply is to be delayed
        self.group = b''
        self.pending = []                  # per request in order: the dumped group, or None
        self.framers = {'REQ': Framer(), 'RESP': Framer()}


def dump_messages(conn: Conn, label: str, data: bytes) -> None:
    try:
        for head, body in conn.framers[label].feed(data):
            if label == 'REQ':
                g = GROUP.search(body)
                group = g.group(1) if g and g.group(1) in DUMP else None
                conn.pending.append(group)
                if group:
                    env = json.loads(body)
                    dump({'conn': conn.n, 'dir': 'request', 'group': group.decode(),
                          'body': decrypt(DUMP[group], env['a3vSYuq2']['Kn51uR4Y'])})
            else:
                group = conn.pending.pop(0) if conn.pending else None
                if not group:
                    continue
                if re.search(rb'(?im)^content-encoding:\s*gzip', head):
                    body = gzip.decompress(body)
                outer = json.loads(body)
                payload = (decrypt(DUMP[group], outer['a3vSYuq2']['Kn51uR4Y']) if 'a3vSYuq2' in outer
                           else {'unencrypted': outer})
                dump({'conn': conn.n, 'dir': 'reply', 'group': group.decode(), 'body': payload})
    except Exception as ex:          # the dump is evidence only: never let it disturb the relay
        log(f'#{conn.n}   dump skipped a {label} message: {type(ex).__name__}: {ex}')
        conn.framers[label] = Framer()


def pump(src: socket.socket, dst: socket.socket, conn: Conn, pattern, label: str) -> None:
    tail = b''
    try:
        while True:
            data = src.recv(65536)
            if not data:
                log(f'#{conn.n} {"client" if label == "REQ" else "server"} closed its side')
                break
            window = tail + data
            if label == 'REQ':
                # Before forwarding: the reply must not be able to overtake the hold.
                for g in GROUP.finditer(window):
                    if g.end() <= len(tail):
                        continue
                    log(f'#{conn.n}   group {g.group(1).decode()}')
                    if g.group(1) in GROUPS:
                        conn.group = g.group(1)
                        conn.hold.set()
            elif conn.hold.is_set() and RESP.match(data):
                log(f'#{conn.n}   holding the {conn.group.decode()} reply {DELAY:.1f}s')
                time.sleep(DELAY)
                conn.hold.clear()
                log(f'#{conn.n}   released the {conn.group.decode()} reply')
            if DUMP and label == 'REQ':
                dump_messages(conn, label, data)     # queued before forwarding, so its reply cannot overtake it
            dst.sendall(data)
            if DUMP and label == 'RESP':
                dump_messages(conn, label, data)
            for m in pattern.finditer(window):
                if m.end() <= len(tail):
                    continue          # already logged from the previous chunk
                if label == 'REQ':
                    log(f'#{conn.n} {m.group(1).decode()} {m.group(2).decode(errors="replace")[:300]}')
                else:
                    log(f'#{conn.n}   -> {m.group(1).decode()}')
            tail = window[-512:]
    except OSError:
        pass
    finally:
        for s in (src, dst):
            try:
                s.shutdown(socket.SHUT_RDWR)
            except OSError:
                pass


def handle(client: socket.socket) -> None:
    with lock:
        counter[0] += 1
        conn = Conn(counter[0])
    try:
        upstream = socket.create_connection(('127.0.0.1', TARGET), timeout=10)
        upstream.settimeout(None)
    except OSError as e:
        log(f'#{conn.n} upstream connect failed: {e}')
        client.close()
        return
    log(f'#{conn.n} opened')
    threading.Thread(target=pump, args=(client, upstream, conn, REQ, 'REQ'), daemon=True).start()
    threading.Thread(target=pump, args=(upstream, client, conn, RESP, 'RESP'), daemon=True).start()


def main() -> None:
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.bind(('127.0.0.1', LISTEN))
    srv.listen(64)
    log(f'relay listening 127.0.0.1:{LISTEN} -> 127.0.0.1:{TARGET}; delaying {DELAY}s the replies to '
        f'{[g.decode() for g in GROUPS]}; dumping {[g.decode() for g in DUMP]}')
    while True:
        client, _ = srv.accept()
        threading.Thread(target=handle, args=(client,), daemon=True).start()


if __name__ == '__main__':
    main()
