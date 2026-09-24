---
name: testing
purpose: >
  Add deterministic tests for changed behavior at the closest useful JVM, Android, Room, Paging,
  or UI layer, and report authored coverage and execution honestly.
inputs:
  - output/<ticket>/CHANGESET.md and referenced source
  - Compact project/test packet generated from .aidlc context
outputs:
  - output/<ticket>/UNIT-TEST-REPORT.md — cases, coverage, execution, and gaps
  - Approved test source under the project's existing test roots
allowed_tools:
  - Read, Grep, Glob, Edit, Write (test source + UNIT-TEST-REPORT.md)
  - ast-graph MCP (changed-symbols, symbol)
  - Bash — smallest relevant test command within an authorized testing stage
must_not:
  - Force unit tests onto UI-only surfaces or use real network/realtime/production DB
  - Run distribution/release tasks or claim an unexecuted test passed
  - Scan the ticket folder broadly or write another artifact
success_criteria:
  - Changed logic layers have deterministic tests; UI-only gaps are explicit
  - Added tests and executed results are distinguished
  - Traceable coverage gaps are reported
  - Every Testing Handoff Test-ID is implemented, explicitly inapplicable, or blocked with evidence; no unplanned scope is silently added
  - Report rows name the authored test path/symbol and distinguish authored, executed, passed, failed, and not executed
  - UNIT-TEST-REPORT.md is the only ticket artifact; test-source writes stay within approved roots
skills:
  - module-impact-analysis
  - regression-analysis
---

## Testing — stage 5

Resolve CHANGESET, its changed paths, and ticket folder. Use `$AIDLC_FLOW` when set; otherwise
resolve `impl-flow` when feature design/plan artifacts exist or `fixbug-flow` when a bug
investigation exists. Then run:

`node .aidlc/lib/stage-context.js testing --flow <resolved-flow> --ticket-dir <resolved-ticket-folder>`

Use the compact packet; do not load either source context file wholesale.

When the packet says `guarded: true`, require CHANGESET's first line to be
exactly `AUTOMATION: CONTINUE` and apply the packet's automation guard. Execute only test work named in
the CHANGESET Testing Handoff; report missing ownership/trace as a gap rather than inventing it.

### Conditional routing and method

Load `module-impact-analysis.md` to keep tests in the owning module and retain consumer-facing
contract coverage. Load `regression-analysis.md` to select behavior at risk. Keep tests deterministic: control time
and dispatchers, avoid sleeps/order dependence, assert outcomes rather than implementation detail,
and isolate external state. Review/PR/release guidance does not belong in this stage.

Use changed-symbols when graph support is current; otherwise inspect the changed paths and focused
references. Use neighboring tests to mirror frameworks, fakes, dispatchers, source sets, and
naming. Cover the changed behavior, including applicable ViewModels, use cases, repositories,
mappers, reducers/stores, factories, policies, paging sources/mediators, socket transforms,
notification builders, routing logic, workers, services, and serialization. Do not force JVM unit
tests for rendering-only UI, but use existing Compose UI, Android view, or instrumentation patterns
when the behavior and source set justify them.
Use fake DAOs for ordinary unit tests; use a Room in-memory database only when the project already
has that test pattern and the relevant JVM/instrumented test is in scope. Never access production
storage, network, or realtime services.

Treat the CHANGESET Testing Handoff as the owned test contract. Preserve its Test-ID, AC/risk,
level, target component/contract, behavior/transition/error scope, fixture boundary, dependencies,
and execution expectation. A focused regression discovered from the actual diff may be added only
when `regression-analysis` supplies evidence; label it `added-by-testing` and map it to the changed
symbol/risk. Do not redesign production behavior or expand into unrelated coverage.

An authorized testing stage may run the smallest relevant test command. If required credentials,
devices, SDKs, or environment configuration are unavailable, report `not executed` with the exact
blocker and suggested command; never broaden into a full build or release task.

### UNIT-TEST-REPORT.md

Include:

1. Test implementation using the matching flow schema:
   - feature: `FR-ID | SC-ID | AC-ID | Work-ID/Story-ID | production Task-ID | testing Task-ID | changed production path/symbol | Test-ID | test path/symbol | level | authored status`;
   - bug: `ticket | fix Task-ID | changed production path/symbol | testing Task-ID | Test-ID | test path/symbol | level | authored status`.
2. Coverage matrix: `Test-ID | behavior/transition/error/risk | fixture boundary | assertion scope | coverage status | evidence`.
3. Execution results: `Test-ID/command scope | exact command | environment | executed status | result | failure evidence`.
4. Failed cases and root cause, distinguishing product failure, test defect, and environment failure.
5. Gaps and recommendations, each mapped to a Test-ID, changed symbol, or risk; no unowned backlog.

When guarded, begin the completed report with `AUTOMATION: CONTINUE`; use
`AUTOMATION: STOP — <reason>` only when required test authoring cannot
be completed. When guarded, hand off to integration-testing only on CONTINUE; STOP ends the flow.
