"""Model of the client's monster AI selector, for testing authored ai.json.

Built only from the disassembled client (arm64 libgame.so); the grammar and
its evidence are documented in packet-generator/assets/archive/ai.kdl.  What
is modelled:

  * BattleUnit::setAiTargetList -- rows in wire order, the first whose
    conditions all pass AND whose percent roll succeeds is executed; none
    passing means a plain attack.
  * checkAiActionTerm self terms: non, skill, act, actbetween, hp_pr_under,
    hp_pr_over, hp_under, hp_over, limited_act, turn_limited_act, turn_act,
    flg_on, flg_off.  Unknown terms raise, so an authored record cannot use a
    term this model has not been taught.
  * The per-row execution record (battle count, this-turn count, last turn),
    the flag array and its immediate/deferred ops, `turn_end`, and the action
    budget of MonsterUnit::initTurnChild.
  * Party conditions on the OPPOSING party (target_id 5, any one unit) for
    the last-action terms bb_use / sbb_use / ubb_use / skill_use.  The client
    reads each unit's last action (BattleUnit +0x1c0 type, +0x240 skill),
    which exitAction leaves in place until that unit acts again; here the
    caller states which of those terms some player unit satisfies, through
    `Monster.opposing`.

It is NOT the client.  Damage, buffs, target choice and the mission script
are outside it; `hp_pct` is supplied by the caller.  Treat a pass as "the
rows say what we meant", never as in-game confirmation.
"""
import random

MAX_ACTIONS_PER_TURN = 64   # a free row that never stops would hang the client


class AuthoringError(Exception):
    pass


class Monster:
    def __init__(self, ai_record, act_min=1, act_max=1, act_rate=100.0, max_hp=100, rng=None):
        self.rows = ai_record['actions']
        self.name = ai_record.get('name', str(ai_record.get('id')))
        self.act_min, self.act_max, self.act_rate = act_min, act_max, act_rate
        self.max_hp = max_hp
        self.hp = max_hp
        self.rng = rng or random.Random(0)
        self.flags = [0] * 50
        self.total = {}        # row priority -> fires this battle
        self.this_turn = {}    # row priority -> fires this turn
        self.last_turn = {}    # row priority -> party turn it last fired
        self.selected_skill = None
        self.actions_taken = 0
        # Last-action terms at least one player unit satisfies this enemy
        # turn, e.g. {'ubb_use'}.  Set by the caller before take_turn.
        self.opposing = set()
        priorities = [r['priority'] for r in self.rows]
        if len(set(priorities)) != len(priorities):
            raise AuthoringError(f'{self.name}: priorities must be unique (resume key)')

    @property
    def hp_pct(self):
        return self.hp * 100.0 / self.max_hp

    def set_hp_pct(self, pct):
        self.hp = self.max_hp * pct / 100.0

    # -- conditions -------------------------------------------------------
    def _term(self, term, n, row, turn):
        key = row['priority']
        if term == 'non':
            return True
        if term == 'skill':
            self.selected_skill = n   # 0 would be random; authored rows name one
            return True               # MonsterUnit::isSkillFree
        if term == 'act':
            return turn == n
        if term == 'actbetween':
            return n != 0 and turn % n == 0
        if term == 'hp_pr_under':
            return self.hp_pct < n
        if term == 'hp_pr_over':
            return self.hp_pct >= n
        if term == 'hp_under':
            return self.hp < n
        if term == 'hp_over':
            return self.hp >= n
        if term == 'limited_act':
            return self.total.get(key, 0) < n
        if term == 'turn_limited_act':
            return self.this_turn.get(key, 0) < n
        if term == 'flg_on':
            return self.flags[n - 1] != 0
        if term == 'flg_off':
            return self.flags[n - 1] == 0
        raise AuthoringError(f'{self.name}: term {term!r} is not modelled')

    def _turn_act(self, param, row, turn):
        # param "a,b": at least a, at most b turns since this row last fired;
        # 50% inside the window, certain at b.  Checked only while at most one
        # action has been taken this turn.
        if self.actions_taken > 1:
            return False
        a, b = (int(x) for x in str(param).split(','))
        elapsed = turn - self.last_turn.get(row['priority'], 0)
        if elapsed < a or elapsed > b:
            return False
        return elapsed == b or self.rng.random() < 0.5

    LAST_ACTION_TERMS = ('bb_use', 'sbb_use', 'ubb_use', 'skill_use')

    def _party(self, c):
        if int(c['target_id']) != 5 or c.get('target_parameter', 'non') not in ('non', ''):
            raise AuthoringError(f'{self.name}: only target 5 / any-unit party conditions are modelled')
        if c['type'] not in self.LAST_ACTION_TERMS:
            raise AuthoringError(f'{self.name}: party term {c["type"]!r} is not modelled')
        return c['type'] in self.opposing

    def _passes(self, row, turn):
        self.selected_skill = None
        ok = all(self._party(c) for c in row.get('party_conditions', []))
        # The client evaluates every self term (AND) -- a failed term does not
        # short-circuit the `skill` selection side effect, which is harmless.
        for c in row['self_conditions']:
            if c['type'] == 'turn_act':
                ok &= self._turn_act(c['parameter'], row, turn)
            else:
                ok &= self._term(c['type'], c['parameter'], row, turn)
        return ok

    # -- one turn ---------------------------------------------------------
    def _budget(self):
        planned = self.act_min
        for _ in range(self.act_min, self.act_max):
            if self.rng.random() * 100.0 < self.act_rate:
                planned += 1
        return planned

    def _apply_flags(self, pairs, stage):
        deferred = []
        for i in range(0, len(pairs) - 1, 2):
            flag, op = pairs[i], pairs[i + 1]
            if flag < 0:
                continue
            value = 1 if op == 1 else 0
            if stage == 'select':
                if flag == 0:
                    self.flags = [value] * 50
                else:
                    self.flags[flag - 1] = value
                if op in (2, 3):
                    deferred.append((flag, 1 if op == 3 else 0))
        return deferred

    def take_turn(self, turn, force_percent=None):
        """Run one enemy turn; returns [(kind, skill_index_or_None), ...].

        force_percent: None rolls the rows' percents with the seeded RNG;
        True makes every roll succeed, False every roll below 100 fail.
        """
        self.this_turn = {}
        self.actions_taken = 0
        planned = self._budget()
        log = []
        guard = 0
        while self.actions_taken < planned:
            guard += 1
            if guard > MAX_ACTIONS_PER_TURN:
                raise AuthoringError(f'{self.name}: turn {turn} never ends -- a free row has no limit')
            chosen = None
            for row in self.rows:
                if not self._passes(row, turn):
                    continue
                pct = row['percent']
                if force_percent is None:
                    hit = self.rng.random() * 100.0 < pct
                else:
                    hit = pct >= 100.0 or force_percent
                if hit:
                    chosen = row
                    break
            if chosen is None:
                log.append(('attack', None))
                self.actions_taken += 1
                continue
            action = chosen['action']
            skill = self.selected_skill if action['type'] == 'skill' else None
            deferred = self._apply_flags(action['flag_changes'], 'select')
            key = chosen['priority']
            self.total[key] = self.total.get(key, 0) + 1
            self.this_turn[key] = self.this_turn.get(key, 0) + 1
            self.last_turn[key] = turn
            log.append((action['type'], skill))
            for flag, value in deferred:
                self.flags[flag - 1] = value
            if action['unknown_bool']:          # counts_as_action
                self.actions_taken += 1
            if action['type'] == 'turn_end':
                self.actions_taken = planned
        return log
