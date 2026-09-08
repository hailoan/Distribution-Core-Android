---
name: automation-test
purpose: >
  Add deterministic instrumented / UI tests for changed behavior at the Espresso, Compose UI, or
  Hilt-instrumented layer under the project's androidTest source set, and report authored coverage
  and on-device execution honestly.
inputs:
  - impl / standalone flow → output/<ticket>/CHANGESET.md and referenced source
  - qa-flow → output/<ticket>/TEST-CASES.md (the automatable cases) and referenced source
  - Compact project/test packet generated from .aidlc context
outputs:
  - output/<ticket>/AUTOMATION-TEST-REPORT.md — UI cases, coverage, on-device execution, and gaps
  - Approved instrumented test source under the project's existing androidTest roots
allowed_tools:
  - Read, Grep, Glob, Edit, Write (instrumented test source + AUTOMATION-TEST-REPORT.md)
  - ast-graph MCP (changed-symbols, symbol)
  - Bash — smallest relevant instrumented test command within an authorized testing stage
must_not:
  - Author JVM unit tests (that is the testing stage) or duplicate its UNIT-TEST-REPORT coverage
  - Use real network/realtime/production DB, or claim an unexecuted test passed
  - Run distribution/release tasks, scan the ticket folder broadly, or write another artifact
success_criteria:
  - User-facing changed behavior has deterministic instrumented/UI tests; non-UI gaps are explicit
  - Added tests and executed results are distinguished; on-device execution is reported honestly
  - Traceable coverage gaps are reported and mapped to a Test-ID, changed symbol, or risk
  - Report rows name the authored test path/symbol and distinguish authored, executed, passed, failed, and not executed
  - AUTOMATION-TEST-REPORT.md is the only ticket artifact; test-source writes stay within approved androidTest roots
skills:
  - regression-analysis
---

## Automation Test — instrumented / UI stage

Resolve the ticket folder and the **predecessor artifact**, which depends on the flow:

- **impl-flow / automation-test-flow** → `CHANGESET.md` (test the just-implemented code changes).
- **qa-flow** → `TEST-CASES.md` (automate the pre-written, automatable test cases).

Use `$AIDLC_FLOW` when set; otherwise resolve `qa-flow` when `TEST-CASES.md` exists, `impl-flow` when
feature design/plan artifacts exist, else `automation-test-flow` when only a `CHANGESET.md` exists.
Then run:

`node .aidlc/lib/stage-context.js automation-test --flow <resolved-flow> --ticket-dir <resolved-ticket-folder>`

Use the compact packet; do not load either source context file wholesale.

When the packet says `guarded: true`, require the predecessor artifact's first line to be
exactly `AUTOMATION: CONTINUE` and apply the packet's automation guard. Execute only test work named in
the handoff — the CHANGESET Testing Handoff (impl / standalone) or the TEST-CASES Automation Handoff
(qa-flow); report missing ownership/trace as a gap rather than inventing it.

### Scope boundary with the testing stage

This stage owns **instrumented / UI** coverage under `app/src/androidTest` only — Espresso view
interactions, Compose UI (`createAndroidComposeRule` / `createComposeRule`), and Hilt-instrumented
behavior that requires a device or emulator. Deterministic business logic belongs to the JVM
`testing` stage; do not re-author it here or restate its `UNIT-TEST-REPORT.md` rows. If the change is
rendering-only or has no user-facing surface, say so explicitly rather than forcing an instrumented
test.

### Conditional routing and method

Load `regression-analysis.md` to select the user-facing behavior at risk. Load `compose-guideline.md`
only when the changed surface is Compose UI. Mirror the project's existing androidTest patterns for
runner, Hilt test rule, Compose/Espresso rules, fakes, and naming — inspect neighboring instrumented
tests before authoring. Keep tests deterministic: drive UI through the framework's synchronization
(Compose test clock / idling resources), never `Thread.sleep`; assert observable UI state and
navigation outcomes, not implementation detail; isolate external state with fakes. Never access
production storage, network, or realtime services; use in-memory or fake backing stores as the
existing instrumented tests do.

Use changed-symbols when graph support is current; otherwise inspect the changed paths and focused
references. Cover the changed user-facing behavior: screens, dialogs, navigation, widget/host
surfaces, input validation shown in the UI, and state restoration — only where an instrumented test
is the closest useful layer.

Treat the flow's handoff as the owned test contract:

- **impl / standalone flow** — the CHANGESET Testing Handoff. Preserve its Test-ID, AC/risk, level,
  target component/contract, behavior scope, fixture boundary, dependencies, and execution
  expectation.
- **qa-flow** — the TEST-CASES Automation Handoff. Implement each automatable case (automatable ≠
  `no`) as an instrumented/UI test and map every `TC-ID → Test-ID`. Do not silently drop a case:
  a `partial` case is automated to its assertable core with the manual remainder reported as a gap,
  and a case you cannot automate is reported with the blocker. Do not automate cases the plan marked
  manual-only.

A focused UI regression discovered from the actual diff or spec may be added only when
`regression-analysis` supplies evidence; label it `added-by-automation-test` and map it to the
changed symbol/risk or AC. Do not redesign production behavior or expand into unrelated coverage.

An authorized testing stage may run the smallest relevant instrumented test command (e.g. a scoped
`connectedAndroidTest` / `connectedCheck` for the changed class). If a device, emulator, SDK, or
required credentials/configuration is unavailable, report `not executed` with the exact blocker and
suggested command; never broaden into a full build or release task.

### AUTOMATION-TEST-REPORT.md

Include:

1. Test implementation using the matching flow schema:
   - qa-flow: `FR-ID | SC-ID | AC-ID | TC-ID | Test-ID | test path/symbol | level (instrumented/ui) | automatable disposition (full/partial) | authored status`;
   - feature: `FR-ID | SC-ID | AC-ID | Work-ID/Story-ID | production Task-ID | testing Task-ID | changed production path/symbol | Test-ID | test path/symbol | level (instrumented/ui) | authored status`;
   - bug: `ticket | fix Task-ID | changed production path/symbol | testing Task-ID | Test-ID | test path/symbol | level (instrumented/ui) | authored status`.
2. Coverage matrix: `Test-ID | user-facing behavior/screen/interaction/risk | fixture boundary | assertion scope | coverage status | evidence`.
3. Execution results: `Test-ID/command scope | exact command | device/emulator + API level | executed status | result | failure evidence`.
4. Failed cases and root cause, distinguishing product failure, test defect, and environment failure.
5. Gaps and recommendations, each mapped to a Test-ID, changed symbol, or risk; no unowned backlog.

When guarded, begin the completed report with `AUTOMATION: CONTINUE`; use
`AUTOMATION: STOP — <reason>` only when required test authoring cannot
be completed. When guarded, hand off to review only on CONTINUE; STOP ends the flow.
