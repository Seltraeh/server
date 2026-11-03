"""Every success reply after a transaction starts must follow an explicit commit.

    python scripts/audit_transactions.py

Drogon commits a transaction from its destructor, after the handler has
replied, and on failure neither retries nor rolls back (gme/common/
Transactions.hpp).  So the server's rule is: transactions start through
gme::beginTransaction, and a reply that reports success is preceded by
gme::commitTransaction (or tryCommitTransaction) -- or, for a "refused,
nothing changed" success, by the rollback that undid the work.

A lint over the source text, not a proof:
  1. No newTransactionCoro() / ->newTransaction( outside gme/common/Transactions.hpp.
  2. In each function that calls beginTransaction(, walking back from every
     `co_return HandleResult::success` after it -- skipping whole catch blocks,
     which only rethrow or refuse -- the LAST statement that touches the
     transaction must be a commit call, or a rollback()/refuse( with no
     `throw` between it and the reply.  A later reuse of the transaction (a
     write after the commit) or a branch with no commit of its own fails.

Prints "<n> transaction site(s); 0 unchecked success replies" when clean (the
runner's completion marker) and every finding otherwise; exit status 1 then.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HELPERS = ROOT / 'gimuserver' / 'gme' / 'common' / 'Transactions.hpp'
BEGIN = re.compile(r'\b(\w+)\s*=\s*(?:co_await\s+|drogon::sync_wait\()?gme::beginTransaction\(')
SUCCESS = re.compile(r'co_return\s+HandleResult::success\b')
COMMIT = re.compile(r'\b(?:try)?[Cc]ommitTransaction\(')
UNDO = re.compile(r'->rollback\(\)|\brefuse\(')
THROW = re.compile(r'\bthrow\b')


def code(line: str) -> str:
    return line.split('//', 1)[0]


def function_end(lines, start):
    """Line of the closing brace of the function that contains line `start`.

    Function bodies here close with a `}` in column 0; blocks, lambdas and
    catch handlers inside them are indented, so the first such line after the
    transaction is its function's end, not the end of an inner block."""
    for j in range(start, len(lines)):
        if lines[j].rstrip() == '}':
            return j
    return len(lines)


def matching_open(lines, close):
    """Line of the `{` that the lone `}` on line `close` closes."""
    depth = 0
    for k in range(close, -1, -1):
        for ch in reversed(code(lines[k])):
            if ch == '}':
                depth += 1
            elif ch == '{':
                depth -= 1
                if depth == 0:
                    return k
    return 0


def last_touch(lines, begin, ret, var):
    """Walk back from the reply to the last line that touches the transaction."""
    uses = re.compile(r'\b' + re.escape(var) + r'\b(?!::)')
    k = ret - 1
    while k > begin:
        text = code(lines[k]).strip()
        if text.startswith('catch'):
            k -= 1
            continue
        if text == '}':
            opener = matching_open(lines, k)
            header = code(lines[opener]) if code(lines[opener]).strip() != '{' else code(lines[opener - 1])
            if header.strip().startswith('catch') or '} catch' in header:
                k = opener - (1 if code(lines[opener]).strip() == '{' else 0) - 1
                continue
        if COMMIT.search(text) or UNDO.search(text) or uses.search(text):
            return k
        k -= 1
    return None


def main() -> int:
    problems, sites = [], 0
    for folder in ('gimuserver', 'standalone_frontend'):
        for path in sorted((ROOT / folder).rglob('*')):
            if path.suffix not in ('.cpp', '.hpp') or 'packets' in path.parts or path == HELPERS:
                continue
            lines = path.read_text(encoding='utf-8', errors='replace').splitlines()
            rel = path.relative_to(ROOT).as_posix()
            for i, line in enumerate(lines, 1):
                if 'newTransactionCoro(' in code(line) or re.search(r'->newTransaction\(', code(line)):
                    problems.append(f'{rel}:{i}: starts a transaction without gme::beginTransaction')
            for i, line in enumerate(lines):
                begin = BEGIN.search(code(line))
                if not begin:
                    continue
                sites += 1
                var = begin.group(1)
                for j in range(i + 1, function_end(lines, i)):
                    if not SUCCESS.search(code(lines[j])):
                        continue
                    k = last_touch(lines, i, j, var)
                    text = code(lines[k]) if k is not None else ''
                    ok = bool(COMMIT.search(text)) or (bool(UNDO.search(text)) and not any(
                        THROW.search(code(lines[m])) for m in range(k, j)))
                    if not ok:
                        where = f'line {k + 1}: {text.strip()[:60]}' if k is not None else 'nothing'
                        problems.append(f'{rel}:{j + 1}: success reply after transaction {var} (line {i + 1})'
                                        f' without a commit; last touch {where}')
    for p in problems:
        print(p)
    print(f'{sites} transaction site(s); {len(problems)} unchecked success replies')
    return 1 if problems else 0


if __name__ == '__main__':
    sys.exit(main())
