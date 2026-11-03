"""Publication failure and selective restore regressions, and log collection; no server or save writes."""
import tempfile
import unittest
from unittest.mock import patch
from pathlib import Path
from types import SimpleNamespace

import content_provider_client_test as h


class DisposableCommands(unittest.TestCase):
    def test_unreachable_broker_is_not_success(self):
        with patch.object(h, 'layout_port', return_value=(29960, 'test')), \
             patch.object(h, 'broker_index', return_value=None), \
             patch.object(h.time, 'time', side_effect=[0, 0, 241]), \
             patch.object(h.time, 'sleep'):
            self.assertEqual(h.await_broker(lambda *_: True, 'test'), 1)

    def test_transient_failure_waits_for_publication(self):
        with patch.object(h, 'layout_port', return_value=(29960, 'test')), \
             patch.object(h, 'broker_index', side_effect=[None, ({'a': 'digest'}, {})]), \
             patch.object(h.time, 'sleep'):
            self.assertEqual(h.await_broker(lambda owned, _: owned.get('a') == 'digest', 'test'), 0)

    def test_restore_one_ignores_unrelated_parked_file(self):
        state = {'changes': {'a.png': {'kind': 'park'}, 'b.png': {'kind': 'park'}}}

        def published(predicate, _message):
            self.assertTrue(predicate({'a.png': 'sha256:abc'}, {'b.png': 'missing'}))
            self.assertFalse(predicate({'a.png': 'sha256:wrong'}, {'b.png': 'missing'}))
            return 0

        with patch.object(h, 'disposable_state', return_value=state), \
             patch.object(h, 'disposable_target', return_value=('a.png', None, None)), \
             patch.object(h, 'disposable_paths', return_value=(Path('root'), Path('state'), Path('parked'))), \
             patch.object(h, 'relink'), patch.object(h, 'sha256', return_value='abc'), \
             patch.object(h, 'save_disposable_state'), patch.object(Path, 'exists', return_value=False), \
             patch.object(h, 'await_broker', side_effect=published):
            self.assertEqual(h.cmd_restore(SimpleNamespace(name='a.png')), 0)
        self.assertEqual(set(state['changes']), {'b.png'})


class CollectCommand(unittest.TestCase):
    def test_a_later_collect_never_loses_an_earlier_one(self):
        with tempfile.TemporaryDirectory() as tmp:
            packages, work = Path(tmp) / 'Packages', Path(tmp) / 'work'
            temp = packages / h.TEST_PFN / 'AC' / 'Temp'
            temp.mkdir(parents=True)
            work.mkdir()
            (temp / 'offlinemod_content.log').write_text('run 1', encoding='utf-8')
            with patch.object(h, 'PACKAGES', packages), patch.object(h, 'WORK', work), \
                 patch.object(h.time, 'strftime', return_value='20261006-100000'):
                self.assertEqual(h.cmd_collect(SimpleNamespace()), 0)
                (temp / 'offlinemod_content.log').write_text('run 2', encoding='utf-8')
                (temp / 'offlinemod_content.log.1').write_text('rotated', encoding='utf-8')
                self.assertEqual(h.cmd_collect(SimpleNamespace()), 0)
            self.assertEqual((work / 'offlinemod_content.log').read_text(encoding='utf-8'), 'run 2')
            first, second = work / 'collected' / '20261006-100000', work / 'collected' / '20261006-100000-1'
            self.assertEqual((first / 'offlinemod_content.log').read_text(encoding='utf-8'), 'run 1')
            self.assertFalse((first / 'offlinemod_content.log.1').exists())
            self.assertEqual((second / 'offlinemod_content.log').read_text(encoding='utf-8'), 'run 2')
            self.assertEqual((second / 'offlinemod_content.log.1').read_text(encoding='utf-8'), 'rotated')

    def test_nothing_to_collect_is_a_failure_and_keeps_nothing(self):
        with tempfile.TemporaryDirectory() as tmp:
            work = Path(tmp) / 'work'
            work.mkdir()
            with patch.object(h, 'PACKAGES', Path(tmp) / 'Packages'), patch.object(h, 'WORK', work):
                self.assertEqual(h.cmd_collect(SimpleNamespace()), 1)
            self.assertFalse((work / 'collected').exists())


if __name__ == '__main__':
    unittest.main()
