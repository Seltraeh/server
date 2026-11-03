"""Reject comparisons which cannot establish harvest credits."""
import contextlib
import io
import sqlite3
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import verify_town_harvest as verify


class HarvestEvidence(unittest.TestCase):
    def check_inconclusive(self, users, taps, drops):
        with tempfile.TemporaryDirectory() as folder:
            paths = [Path(folder) / f'{i}.sqlite' for i in range(2)]
            for i, path in enumerate(paths):
                with contextlib.closing(sqlite3.connect(path)) as db:
                    db.execute('CREATE TABLE user_info(id TEXT)')
                    db.executemany('INSERT INTO user_info VALUES (?)', [(u,) for u in users[i]])
                    db.execute('CREATE TABLE user_town_locations(location_id, tap_cnt, drop_info, lv)')
                    db.execute('INSERT INTO user_town_locations VALUES (3,?,?,1)', (taps[i], drops[i]))
                    db.commit()
            opened = []
            original_ro = verify.ro

            def tracked_ro(path):
                db = original_ro(path)
                opened.append(db)
                return db

            try:
                with patch.object(verify.sys, 'argv', ['verify', *map(str, paths)]), \
                     patch.object(verify, 'ro', side_effect=tracked_ro), \
                     patch.object(verify, 'item_names', return_value={}), \
                     contextlib.redirect_stdout(io.StringIO()) as output:
                    self.assertEqual(verify.main(), 2)
                self.assertIn('INCONCLUSIVE', output.getvalue())
                self.assertNotIn('OK  ', output.getvalue())
            finally:
                for db in opened:
                    db.close()

    def test_multiple_users(self):
        self.check_inconclusive([['a', 'b'], ['a', 'b']], [1, 0], ['0:0:0'] * 2)

    def test_rerolled_list(self):
        self.check_inconclusive([['a'], ['a']], [1, 0], ['0:0:0', '0:1:0'])

    def test_increased_taps(self):
        self.check_inconclusive([['a'], ['a']], [0, 1], ['0:0:0'] * 2)


if __name__ == '__main__':
    unittest.main()
