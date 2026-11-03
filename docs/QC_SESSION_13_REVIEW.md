# Session 13 QC — October 8, 2026

Verdict: ready for the scoped client visual run; no blocking defect found.

Scope: C3 policy generation, the shared-bundle broker correction, fixture
preparation, expanded live integration and player instructions. Earlier dirty
work remains preserved. No deployment, commit or live-save edits.

## Independent evidence

- Candidate server hash matches `90e3eff97739b0d5d5713cae983db9fc4cadaa7e105b8bdf009a81b1444b5ebc`.
- Proxy test executable matches `ad5cbc67ba473365ecf2c6890c692ecae8609ef63745170521a7828f8f9870d5`.
- Broker regression: 82 passed, zero failures, including cascading withdrawal
  and recovery of shared-member bundles.
- Synthetic generator regression: 21 passed, zero failures.
- Regenerating the C3 policy is byte-identical to the supplied `5b9c5e52...`.
- Real C3 broker/proxy integration: 5,973 proxy checks and 9 driver checks
  passed. All 36,321 added reads matched the CRT bytes, including 1,156 text
  reads and 30,932 page paths. Two unrelated opt-in live modes were skipped.
- The prepared fixture hash matches `e2244ed2...`; its manifest records one
  valid unit, 50 gems and no dangling references.
- SuperAnim page swap/restore/park/restore: 340 proxy checks and 10 driver
  checks passed, with two unrelated live modes skipped. All disposable
  changes were restored; the 15 involved real files and live save were
  unchanged, and port 19963 was released. Publication took 47.2, 41.3, 6.1
  and 41.3 seconds respectively.

Logs under out/qa/current/session13: qc_broker.log, qc_generator.log,
qc_family.log, qc_policy_regen.log and qc_page_swap.log. The full 52-entry
gameplay regression claimed in Claude's handoff was not independently repeated;
this QC reruns the systems affected by the pass. No C++ changes were needed
during this review, so the candidate was not rebuilt.

## Review and corrections

The broker fix converges: every non-final pass removes at least one owned
member; the final pass records unavailability for all incomplete bundles.
It corrects the previous order-dependent partial availability. The generator
keeps shared SuperAnim pages together and excludes incomplete/colliding families.
No blocking defect found for the supplied policy and content tree.

The client checklist now explicitly closes other test harnesses, stops on a
crash, makes Level Up conditional on EXP, and avoids promising animation on a
static description screen. Its scope is sampled visual acceptance, not every
possible effect. The startup plan now explicitly retains removal of FILE_CHECK
as unfinished work; leaving dictionary initialization untouched this session
does not satisfy that user goal.

The historical Session 12 proxy-test baseline could not be reconstructed with
its recorded hash, as Claude disclosed. This QC reviewed current behavior and
ran the current candidate; it does not certify that reconstructed diff as a
complete historical change list. Documented disassembly was not independently
reversed in full. Live hook tests use fake game functions and do not substitute
for the actual client rendering pass.

## Acceptance boundaries

C3 provides 53,766 names; it does not remove all download screens or login
verification. Audio and other legacy content remain outside this phase.
Family-scale replacement latency remains material. Follow CLIENT_SESSION_13_STEPS.md
using [ContentTest C1], not C1 Yes. Session 12's separate acceptance checks
remain pending unless the player supplies results.
