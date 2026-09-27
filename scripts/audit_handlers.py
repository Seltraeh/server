"""Inventory GME registrations and literal empty replies; this is not a C++ compiler.

python scripts/audit_handlers.py [--json]
Fails for duplicate IDs/definitions or a registration without a HANDLEF body.
Empty acknowledgements are candidates for review, not automatically defects.
"""
import collections
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
TOKENS = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\\r\n])\'|//[^\n]*|/\*.*?\*/', re.S)


def scrub(source, strings=False):
    def replace(match):
        text = match.group()
        if strings or text.startswith('//') or text.startswith('/*'):
            return ''.join('\n' if c == '\n' else ' ' for c in text)
        return text
    return TOKENS.sub(replace, source)


def audit():
    registry = ROOT / 'gimuserver/gme/handlers/GmeControllerHandlers.cpp'
    registrations = re.findall(r'REGISTER\(\s*"([^"]+)"\s*,\s*(\w+)\s*,\s*"[^"]+"\s*\)', scrub(registry.read_text(encoding='utf-8')))
    bodies = collections.defaultdict(list)
    errors = []
    for path in sorted((ROOT / 'gimuserver/gme/handlers').glob('*.cpp')):
        original = path.read_text(encoding='utf-8')
        code = scrub(original)
        masked = scrub(original, strings=True)
        for match in re.finditer(r'HANDLEF\((\w+)\)\s*\{', masked):
            start, depth, end = match.end(), 1, match.end()
            while depth and end < len(masked):
                depth += (masked[end] == '{') - (masked[end] == '}')
                end += 1
            if depth:
                errors.append(f'Unbalanced handler {match[1]} in {path.name}')
                continue
            body = code[start:end-1]
            bodies[match[1]].append({
                'file': path.relative_to(ROOT).as_posix(),
                'line': original.count('\n', 0, match.start()) + 1,
                'literal_empty_replies': len(re.findall(r'HandleResult::success\(\s*"\{\}"\s*\)', body)),
                'awaits': len(re.findall(r'\bco_await\b', body)),
            })
    for group, count in collections.Counter(group for group, _ in registrations).items():
        if count != 1: errors.append(f'Duplicate GroupId {group}: {count} registrations')
    for name, found in bodies.items():
        if len(found) != 1: errors.append(f'Duplicate HANDLEF body: {name}')
    entries = []
    for group, name in registrations:
        if name not in bodies:
            errors.append(f'{group} registers {name} without a HANDLEF body')
        else:
            entries.append({'group': group, 'handler': name, **bodies[name][0]})
    registered_names = {name for _, name in registrations}
    return {'registered': len(registrations), 'errors': errors,
            'unregistered_definitions': sorted(set(bodies)-registered_names),
            'empty_reply_candidates': [row for row in entries if row['literal_empty_replies']],
            'handlers': entries}


if __name__ == '__main__':
    result = audit()
    if '--json' in sys.argv:
        print(json.dumps(result, indent=2))
    else:
        print(f"{result['registered']} registrations; {len(result['errors'])} structural errors")
        seen = set()
        for row in result['empty_reply_candidates']:
            if row['handler'] in seen: continue
            seen.add(row['handler'])
            label = 'NO-AWAIT CANDIDATE' if row['awaits'] == 0 else 'empty branch/ack'
            print(f"{label:20} {row['handler']:28} {row['file']}:{row['line']}")
        for name in result['unregistered_definitions']:
            print('UNREGISTERED DEFINITION:', name)
        for error in result['errors']:
            print('ERROR:', error)
    sys.exit(bool(result['errors']))
