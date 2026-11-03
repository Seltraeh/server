# Claude Session 18 — startup coverage, content gaps, and offline first launch

October 21, 2026. Work in `C:/Users/Evan/BF/BF-WorkingDirRust` and its existing sibling `C:/Users/Evan/BF/offline-proxy`. This directive is current; read-first documents provide evidence and constraints, not permission to discard current dirty work.

## Read first

1. `docs/BF_OFFLINE_SERVER_HANDBOOK.md` (especially Current direction and build hygiene).
2. `docs/CLAUDE_SESSION_17_HANDOFF.md`, `docs/QC_SESSION_17_REVIEW.md`, and `docs/SESSION_16_CLIENT_RESULTS.md`.
3. `docs/OFFLINE_STARTUP_TRANSITION_PLAN.md` §8, including §8.4's exact unknown lists and content gaps.
4. `docs/CLAUDE_SESSION_16.md` for the original objective and safety/acceptance constraints.
5. Inspect `git status` and existing outputs before editing. Preserve every dirty file and existing C1/C16 package, save, registration, consent and LocalState state.

## Verified baseline and client evidence

Session 17 DLL `45a2c1b1…` is installed in C16. The real client log confirms the startup hook installed after checking all nine sites before and after provider attach. The manifest remains incomplete; the census was built and the background-download prompt was left to the player. This is fallback behavior, not a cache-free startup result.

The bounded C16 run is complete; do not repeat its controls. See
`SESSION_16_CLIENT_RESULTS.md`: Home Selena; mission 10 won and returned to Mistral;
no Store/Facebook prompts; client and server restarts preserved squad, currency and
quest counter. Before controls: zero owned / 109 other cached assets. Missing-file
control: exactly one owned file (`action_attack1.png`) downloaded, then restored.
Provider-off control: Home and Mistral worked; 2,233-file login queue observed; final
snapshot 3,147 owned / 109 other. C16 is now warm. Game and harness are stopped.
Logs show installed hooks, census built, stored DLC No retained, zero LEFT OUT
decisions. Audio and precise brief-screen timings were not assessed. Results had
no visible Next cue in screenshots, but bottom-center tap advanced: compare against
accepted rendering before classifying a regression.

## Objective

Deliver measurable progress toward a genuinely offline first launch while preserving the game's required initialization and fail-closed behavior. Work in this order:

1. **Use the completed C16 evidence.** Read the final results and logs. Preserve
   C16 as evidence; do not reset it or launch it for redundant controls. Compare the
   result-screen continuation cue via consumer/assets evidence before making a fix.

2. **Derive exact startup names for the 16 unknown lists.** Trace the client request builders from the supported binary and the actual MST ids/rows; do not infer names by listing existing folders. Choose a bounded, tractable list/category first, document its source and derivation, and add fixtures/tests for empty, populated, and missing-required-name cases. Keep every unresolved list explicitly unknown so coverage remains incomplete. Continue only as evidence and time allow.
3. **Close high-value content gaps.** Prioritize login/Home and the directly observed route. Address the known 67 absent required names and 26 unresolved families through verified source assets, corrected definitions/resource maps, or client evidence proving a name is not required. Begin with tractable families that improve actual login/Home behavior. Record each basename collision and consumer evidence. Never add placeholders, wildcards, or ownership claims that conceal a missing dependency.
4. **Bind coverage inputs to the client's master tables.** Determine a practical integrity check between installed/client MST inputs and the manifest's source data. Keep unsupported builds, unknown lists, missing dependencies, absent names and generation changes incomplete. Do not enable census/prompt bypass while any such blocker remains.
5. **Remove the obsolete online-service first-launch screen where justified.** Find the narrow offline-only path and determine which initialization or consent state it protects. Preserve required initialization. Do not fake an age/parent consent checkbox, automate acceptance, or rewrite the user's consent. The user explicitly requested removal for the offline patch and personally accepted C16 consent. Implement an offline-only scene-flow change when the reversed code supports it; do not simulate acceptance or set age/parental-attestation flags. If those concerns cannot be separated, document the exact blocker and leave that part for review. Keep unrelated legal/privacy UI unchanged.

Do not spend the session only revising plans or increasing ownership counts without improving the measured login/Home route. Do not force a scene, FILE_CHECK, or request queue to finish. Do not suppress legitimate master-table checks, text dictionaries, save/login initialization, corruption errors, or required downloads.

## Change and build rules

- Preserve all existing user changes and the accepted C1 fallback. Keep work in the current server/proxy checkout; inspect dirty diffs before touching any file that is already modified or untracked.
- Reuse existing Visual Studio/CMake build trees. Use one reusable `out/qa/current/session18/` output folder; inspect it first and preserve any existing contents. Do not create timestamped build trees.
- For any startup-hook change, preserve Session 17's two-stage signature validation and overlap guard: validate original sites before provider attach, then accept only the exact recorded provider detour and verified trampoline afterward. Add negative cases for altered targets, displaced bytes, records and detach order. Never relax a signature to accept arbitrary jumps.
- Run focused tests for changed server/proxy/policy code and relevant regressions. Record exact commands, executable hashes, results and limitations; verify the binaries being tested are the ones reported. Preserve the gate-flash, audio-reader and post-quest proofs.
- No install/swap into any client package, no deployment, commit, push, live-save mutation, or consent action. Prepare reviewable candidate artifacts only.

## Acceptance

- Exact derived names have a reproducible binary/MST source trail and tests; unresolved names remain marked unknown.
- Gap work identifies the concrete login/Home route improvement and demonstrates dependency closure, collision handling and missing-file failure behavior.
- Coverage remains incomplete unless every list, required name, family, client build and generation is actually proven. No bypass or screen-removal claim is allowed on incomplete coverage.
- Any consent-screen change is limited to an evidence-supported offline-only path and does not simulate or alter the user's age/parental consent.
- Client evidence distinguishes installed hook, stock fallback, asset writes, visual observations and audio assessment. A visible frame sample alone cannot prove there were no brief download screens.

## Deliverables

Write `docs/CLAUDE_SESSION_18_HANDOFF.md` with changed-file scope, derivation/evidence, commands and results, hashes, remaining blockers, and exact status labels (implemented, automated-tested, client-observed, deployed). Update `docs/CLIENT_SESSION_18_STEPS.md` with a concise runnable checklist for any remaining client controls or later candidate acceptance, using exact identities/commands and explicit rollback/collection steps. Update the handbook's current direction only after the evidence is settled. Do not overwrite or rewrite the Session 16 results history; append a dated Session 18 section.
