---
name: bug-investigation
purpose: >
  Classify a bug, determine whether evidence confirms a root cause, and specify a tier-scaled fix
  only when confirmed. Investigation only; never implement.
inputs:
  - BUG/crash report with observed/expected behavior, repro, logs, trace, and referenced attachments
  - Optional priority (auto/low/medium/critical)
  - Current codebase read-only, within the lookup cap
  - Compact project/stage packet generated from .aidlc context
outputs:
  - output/<ticket>/BUG-INVESTIGATION.md — diagnosis + conditional fix/testing tasks
allowed_tools:
  - Read, Grep, Glob (read-only)
  - ast-graph MCP (search, symbol, blast-radius, call-chain) — before file scanning
  - Write — ONLY BUG-INVESTIGATION.md and referenced input/*.md conversions
must_not:
  - Implement, write feature code, run builds, or guess a root cause
  - Load any conditional skill whose exact trigger is not present in the evidence in hand
  - Apply Critical depth to Low work or under-analyze a crash/payment issue
  - Exceed the lookup cap unless a Critical/high-risk surface forces a recorded escalation
  - Scan the ticket folder broadly or write another artifact
success_criteria:
  - The higher conflicting tier wins; crash traces are never Low
  - Confirmed causes cite file:line evidence; unconfirmed cases stop with candidates and a next check
  - Confirmed fix and testing tasks contain the concrete change surface, dependencies, done conditions, and test scope needed to execute without a separate plan
skills:
  - always - ticket-reading
  - always - module-impact-analysis
  - on trigger only - bug-root-cause (Medium/Critical or unclear tier), codebase-search, dependency-analysis, regression-analysis, risk-analysis
  - on trigger only - native-boundary-guideline, gradle-module-guideline
---

## Bug Investigation — understanding front door

Resolve the report, attachments, priority, and ticket folder; do not scan unrelated ticket output:

`node .aidlc/lib/stage-context.js bug-investigation --flow "${AIDLC_FLOW:-fixbug-flow}" --ticket-dir <resolved-ticket-folder>`

Use the compact packet, not either source context file.

Load `module-impact-analysis.md` after the failing path is bounded. A confirmed handoff must name
the owning module, affected consumer closure, crossed contract, and required integration checks.

### Triage first

Load `ticket-reading.md` (referenced attachments only), then tier the bug inline in one line —
crashes and payment/data issues are never Low; the higher conflicting tier wins. Then take one path:

- **Fast path (Low / single-surface):** direct evidence already connects the observed mismatch to
  the responsible branch/value and incorrect result. A named file or stack frame alone is not
  causal evidence. Write the artifact from that evidence with a small bounded inspection; load
  `bug-root-cause.md` whenever causation still needs to be established.
- **Full path (Medium / Critical, or tier unclear):** load `bug-root-cause.md`, run only that tier's
  workflow, then load further skills strictly by trigger below.

### Skill triggers

Every conditional skill defaults to **OFF**. Load one **only** when its exact trigger below is
literally satisfied by evidence in hand — never for completeness, symmetry, or reassurance; when in
doubt, do not load it. The tier sets breadth and verification depth, not a required number of
hypotheses, skills, or tokens. A trace locates failure but does not prove cause.

1. `bug-root-cause.md` — Medium/Critical, or when the tier is not obvious from the report/trace.
2. `codebase-search.md` — report/trace evidence has not already resolved the failing symbol.
3. `dependency-analysis.md` — a symbol is known **and** reachable/shared impact affects the
   diagnosis; depth ≤2.
4. `regression-analysis.md` — Medium/Critical only.
5. `risk-analysis.md` — Critical or a packet-declared high-risk surface only.
6. `native-boundary-guideline.md` — camera/JNI/C++/CMake/ABI evidence is in the failing path.
7. `gradle-module-guideline.md` — dependency/build/plugin/publication behavior is in the failing path.

Use a bounded evidence budget appropriate to the path; coroutine concurrency, token/auth, DI,
JNI/native lifecycle, camera/EGL/codec, build/plugin, or cross-module failures may require more
than a single-file bug. Prefer a current project graph
when available, verify graph claims against source, and fall back to focused text search and source
inspection when it is absent or stale. Stay in one stage context without fan-out and record an
unknown rather than searching for reassurance.

### Guard

When the packet says `guarded: true`, start the artifact with exactly `AUTOMATION: CONTINUE` or
`AUTOMATION: STOP — <reason>` and apply the packet's automation guard. Critical tier also stops.
Write the investigation on STOP, but do not hand off.

### BUG-INVESTIGATION.md

State `ticket_priority`, evidence-derived `technical_severity`, confidence, reproduction status,
affected environment/variant, trigger, and `status: confirmed | unconfirmed`. When unconfirmed, write ranked candidates
with evidence for/against each and the next discriminating check — no fix, no downstream tasks. In a
guarded flow this artifact starts with STOP and ends the flow.

When confirmed, write:

- Low: Cause (`file:line`), minimal Fix, Verification.
- Medium: Summary, confirmed Root Cause, Recommended Fix, evidence-supported alternatives when any,
  Regression Checklist.
- Critical: Scope/timeline/repro, evidence trail and Root Cause, evidence-supported viable options
  with a recommendation, Risks, Rollback, Regression, Monitoring.

For a confirmed handoff, add both tables:

- Change Surface: `Change-ID | existing/new | action | exact path | symbol/resource/config key | responsibility | evidence | collision key`.
- Fix Tasks: `Task-ID | owner stage (android-dev) | objective | Change-IDs/exact path-symbol scope | preconditions | invariants | done condition | verification | depends on | collision key`.
- Testing Tasks: `Task-ID | owner stage (testing) | target behavior/regression | Test-ID | level | target component/contract | fake/fixture boundary | relevant fix Task-ID | depends on | execution expectation`.
- Integration Tasks: `Task-ID | owner stage (integration-testing) | Check-ID | changed module | affected consumer/external contract | boundary | exact command or device/manual check | blocking policy | depends on`.

Because bug flows skip solution-design and implementation-plan, a confirmed investigation owns the
minimum fix design and execution plan. Do not hand off a task that asks android-dev to locate the
failure, choose among causes, decide a contract, or discover the affected path. If exact scope is
not supported by evidence, keep the investigation unconfirmed and STOP.

This preserves `fix Task-ID -> changed path -> testing Task-ID/Test-ID -> integration Check-ID -> review status`. Hand off
to android-dev only with `AUTOMATION: CONTINUE` (or after guided approval); otherwise end at the
artifact.
