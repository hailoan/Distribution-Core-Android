---
name: qa-plan
purpose: >
  Turn approved acceptance criteria into an explicit, traceable, reviewable set of test cases —
  positive, negative, and boundary — marking which are automatable and at what level, before any
  automation is written.
inputs:
  - output/<ticket>/DEV-SPEC.md (acceptance criteria) and any referenced design/API evidence
  - Compact project/test packet generated from .aidlc context
outputs:
  - output/<ticket>/TEST-CASES.md — the test-case catalog with traceability and automatable flags
allowed_tools:
  - Read, Grep, Glob (read-only over spec + codebase for feasibility)
  - ast-graph MCP (search, symbol) — to confirm a case is reachable/observable
  - Write — ONLY TEST-CASES.md
must_not:
  - Write test code or the automation report (that is the automation-test stage)
  - Change production code, invent acceptance criteria absent from DEV-SPEC, or hide untestable ACs
  - Scan the ticket folder broadly or write another artifact
success_criteria:
  - Every acceptance criterion maps to at least one test case; unmappable ACs are called out
  - Each case has stable TC-ID, clear steps, test data, and a single expected result
  - Positive, negative, and boundary conditions are covered where the AC implies them
  - Each case is marked automatable (yes/no/partial) and given a level (ui/instrumented/manual)
  - Traceability FR-ID → SC-ID → AC-ID → TC-ID is intact; TEST-CASES.md is the only artifact
skills:
  - regression-analysis
---

## QA Test Plan — test-case authoring stage

Resolve DEV-SPEC and the ticket folder. Use `$AIDLC_FLOW` when set; otherwise resolve `qa-flow`.
Then run:

`node .aidlc/lib/stage-context.js qa-plan --flow <resolved-flow> --ticket-dir <resolved-ticket-folder>`

Use the compact packet; do not load either source context file wholesale. This stage reads the
approved acceptance criteria and designs test cases from them — it writes no test code and changes no
production code.

When the packet says `guarded: true`, require DEV-SPEC's first line to be exactly
`AUTOMATION: CONTINUE` and apply the packet's automation guard. Derive cases only from acceptance
criteria present in DEV-SPEC; report a missing or untestable AC as a gap rather than inventing
behavior.

### Method

Load `regression-analysis.md` to weight behavior at risk and prioritize cases. For each acceptance
criterion, enumerate the conditions it implies:

- **Positive** — the criterion is satisfied on the expected happy path.
- **Negative** — invalid input, denied permission, error/empty/failure states surfaced to the user.
- **Boundary** — limits, empty/single/max collections, rotation/config change, process death and
  state restoration, first-run vs returning.

Keep each case atomic: one behavior, deterministic steps, concrete test data, and exactly one
expected result. Mark **automatable**:

- `yes` — reliably drivable and observable through Espresso / Compose UI / instrumented hooks;
- `partial` — automatable with a fake/seam or only its assertable core;
- `no` — genuinely manual/exploratory (visual polish, external device state, human judgment).

Assign a **level**: `ui` (Compose/View interaction), `instrumented` (needs device/emulator but not a
full UI drive), or `manual`. Confirm feasibility lightly against the codebase (is the surface
reachable, is the outcome observable) but do not design the automation itself — that is the
automation-test stage, which will map each automatable TC-ID to a Test-ID.

### TEST-CASES.md

Include:

1. Summary: feature under test, in-scope surfaces, and out-of-scope notes.
2. Test-case catalog (one row per case):
   `FR-ID | SC-ID | AC-ID | TC-ID | title | type (positive/negative/boundary) | preconditions | steps | test data | expected result | priority (P0–P2) | automatable (yes/no/partial) | level (ui/instrumented/manual)`.
3. Coverage map: `AC-ID | covering TC-IDs | covered? (yes/partial/no)` — every AC accounted for.
4. Automation handoff: the subset of TC-IDs with automatable ≠ `no`, in priority order, as the owned
   contract the automation-test stage implements.
5. Gaps: untestable/ambiguous ACs and manual-only cases, each mapped to an AC-ID; no unowned backlog.

When guarded, begin the completed catalog with `AUTOMATION: CONTINUE`; use
`AUTOMATION: STOP — <reason>` only when acceptance criteria are missing, contradictory, or
untestable as written. When guarded, hand off to automation-test only on CONTINUE; STOP ends the
flow.
