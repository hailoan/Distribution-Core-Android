---
name: review
purpose: >
  Review the local change for convention, architecture, regression, and runtime/PR safety, then
  issue one evidence-based Go/No-Go verdict.
inputs:
  - Pipeline review — CHANGESET.md + UNIT-TEST-REPORT.md
  - Feature pipeline — SOLUTION-DESIGN.md acceptance criteria + IMPLEMENT-PLAN.md task traceability
  - Bug pipeline — BUG-INVESTIGATION.md verification/testing tasks (Low) or regression evidence (Medium/Critical)
  - Standalone review — branch/diff scope only; pipeline artifacts are not required
  - Local git diff in every mode
  - Compact project/stage packet generated from .aidlc context
outputs:
  - output/<ticket>/CODE-REVIEW.md — prioritized findings, trace verification, and verdict
allowed_tools:
  - Read, Grep, Glob (read-only)
  - Bash — read-only local Git inspection (`status`, `diff`, `show`, `merge-base`, `log`); fetch only when explicitly requested
  - ast-graph MCP (changed-symbols, blast-radius, call-chain) — FIRST
  - Write — ONLY CODE-REVIEW.md
must_not:
  - Fetch, build, push, or fix code without authorization
  - Claim an unexecuted test passed
  - Scan the ticket folder broadly or write another artifact
success_criteria:
  - Actual diff is checked against packet rules and runtime safety
  - Prioritized findings cite file:line and a concrete failure scenario
  - Every criterion is traced through change and tests before the final verdict
  - Planned Change-IDs are reconciled with the actual diff and every changed production path/symbol is authorized
  - Planned Test-IDs are reconciled with authored and executed evidence without treating not-executed as passed
  - Exactly one artifact is written — CODE-REVIEW.md
skills:
  - review-checklist
  - architecture-analysis
  - regression-analysis
  - risk-analysis
---

## Review — stage 6

Resolve review mode and scope before judging anything. Establish branch comparison, staged,
unstaged, and untracked scope separately; exclude and disclose unrelated user changes. Always source the scoped local diff first.
Pipeline review then requires CHANGESET and UNIT-TEST-REPORT; use SOLUTION-DESIGN for feature
criteria, BUG-INVESTIGATION for Low verification/testing tasks or Medium/Critical regression
evidence, and IMPLEMENT-PLAN only for task mapping. Standalone review uses branch/diff scope only.
Use `$AIDLC_FLOW` when set; otherwise choose `techlead-review-flow` for a branch-only review,
`impl-flow` for feature artifacts, or `fixbug-flow` for bug artifacts. Then run:

`node .aidlc/lib/stage-context.js review --flow <resolved-flow> --ticket-dir <resolved-ticket-folder>`

Use the compact packet; do not load either source context file wholesale. In a standalone branch
review, ticket-only checks become not verifiable rather than missing prerequisites.

### Ordered review

1. Establish the exact local diff and changed symbols; use a current graph when available and
   otherwise inspect focused references. Never fetch merely to complete review.
2. Validate manifest/report paths against that diff.
3. Load `review-checklist.md` for convention-fit and runtime/PR safety.
4. Load `architecture-analysis.md` only when boundaries or dependency direction changed.
5. Load `regression-analysis.md` when behavior changed or test coverage needs mapping.
6. Load `risk-analysis.md` only for packet/diff-flagged high-risk paths.
7. Verify criteria and traceability, filter false positives, write findings, and decide the
   verdict **last**.

Only an exact executed green command in UNIT-TEST-REPORT counts as passed; otherwise use
`not executed`. When the packet says `guarded: true`, require every predecessor's first line to
be exactly `AUTOMATION: CONTINUE` and apply the packet's automation guard.

### CODE-REVIEW.md

When guarded, begin with `AUTOMATION: CONTINUE` only after completing the review; use
`AUTOMATION: STOP — <reason>` when required evidence is missing.

1. Findings table: `severity | category | file:line | failure scenario | recommendation`; omit
   empty categories.
2. Runtime / PR Readiness
3. Change-scope reconciliation:
   `Change-ID | Task-ID | Design-Ref | planned path/symbol/action | actual diff path/symbol/status | authorized | design evidence status | review status`.
   For bug work, use the investigation's Change-ID/fix Task-ID. Flag unmapped production changes,
   missing planned changes, and unauthorized scope before judging behavior.
4. Trace Verification — use the matching flow schema:
   - feature: `FR-ID | SC-ID | AC-ID | Design-Ref | Work-ID/Story-ID | production Task-ID | Change-ID/path/symbol | testing Task-ID | Test-ID | authored/executed result | review status`;
   - bug: `ticket | fix Task-ID | Change-ID/path/symbol | testing Task-ID | Test-ID | authored/executed result | review status`.
5. Verdict — No-Go/`Request changes` for any Critical/Major finding or failed required test; in a
   pipeline review, missing implementation/test trace is also No-Go. Otherwise Go: `Approved` with no findings or
   `Approved with comments` for Minor/Suggestion/readiness limitations. An unexecuted command alone is a
   disclosed limitation, not a defect, unless project policy requires it.

No-Go ends the gate and returns findings to the user; a newly approved repair plan/fix is required
before android-dev runs again. Go ends the pipeline.
