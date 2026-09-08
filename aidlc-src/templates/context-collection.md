# AI-DLC Runtime Contract — {{PROJECT_NAME}}

This file contains framework-independent workflow rules. Project facts live in `context.md`.
Stages should consume the compact packet from `.aidlc/lib/stage-context.js`, not load either file
wholesale.

## 0. Ground rules (read before touching anything)

1. Use a current project knowledge graph first for structural questions when it is available and
   covers the target module. Validate important results against source. Otherwise use focused text/
   symbol search and source inspection; never treat a missing graph edge as proof of no impact.
2. An authorized implementation or testing stage may run the smallest relevant compile, unit-test,
   or static-check command. Commit, push, distribution, signing, upload, and publishing always need
   separate explicit authorization.
3. Match the surrounding architecture, naming, and patterns. Project-specific invariants live in
   `.aidlc/context.md` under project ground rules.

## 10. Planning conventions

- Prefer independently testable vertical slices. A slice may cross modules when the architecture
  requires it; identify one primary user-facing surface.
- Create shared foundations first only when multiple slices truly depend on them.
- Give every work item a stable ID, dependencies, acceptance contract, owned scope, and test scope.
  Add formal user stories, estimates, priority, or sprint placement only when requested and based
  on a supplied team scale/capacity.
- Serialize navigation, DI, migrations/schema, shared state, and build configuration.
- Add an integration item when multiple slices must be wired together.

## 11. Feature → review workflow

The generated `.aidlc/pipelines.json` is the executable flow source. Preserve traceability across
artifacts:

Feature: `FR-ID → SC-ID → AC-ID → Work-ID/Story-ID → Task-ID → changed path → Test-ID → review status`.
Bug: `ticket → fix Task-ID → changed path → testing Task-ID → Test-ID → review status`.

### Ticket folder and automation

Resolve one ticket folder: `$AIDLC_OUTPUT_DIR`, otherwise `output/<ticket>/`, or `output/` only when
there is no ticket ID. Read only named ticket artifacts; inspect project/external evidence only as
the stage agent permits. Write only its primary artifact and explicitly allowed support/source
outputs.

Guarded flows (`impl-flow`, `auto-*`, `fixbug-flow`, and `qa-flow`) require the first artifact
line to be `AUTOMATION: CONTINUE` or `AUTOMATION: STOP — reason`. Stop on missing/contradictory
input, untestable/absent acceptance criteria, unconfirmed bug causation, or protected work involving
authentication, payments, security/privacy, destructive data/migration, realtime/offline sync,
shared navigation/DI/state, or build logic.

### Artifact contract

| Stage (skill) | Required ticket inputs / permitted evidence | Writes |
| --- | --- | --- |
| feature-analysis | requirements + optional design/API/docs/code evidence | `DEV-SPEC.md`; converted attachments under `input/`; one ticket-local remote-design cache under `figma/` when needed |
| bug-investigation | bug/crash report + optional logs/trace/code evidence | `BUG-INVESTIGATION.md`; converted attachments under `input/` |
| solution-design | `DEV-SPEC.md` | `SOLUTION-DESIGN.md`; optional `figma/`, `design/`, `screenshot/` |
| implementation-plan | `SOLUTION-DESIGN.md` | `IMPLEMENT-PLAN.md` |
| android-dev | feature: `IMPLEMENT-PLAN.md` + `SOLUTION-DESIGN.md`; bug: `BUG-INVESTIGATION.md`; referenced design support and plan/fix-owned code | `CHANGESET.md` + approved source/resource/config changes |
| testing | `CHANGESET.md` + changed code | `UNIT-TEST-REPORT.md` + approved test source |
| qa-plan | `DEV-SPEC.md` (acceptance criteria) | `TEST-CASES.md` |
| automation-test | impl/standalone: `CHANGESET.md` + changed code; qa-flow: `TEST-CASES.md` | `AUTOMATION-TEST-REPORT.md` + approved instrumented test source |
| review | `CHANGESET.md`, `UNIT-TEST-REPORT.md`, and feature design/plan or bug investigation (qa-flow: `AUTOMATION-TEST-REPORT.md` + `TEST-CASES.md` + `DEV-SPEC.md`) | `CODE-REVIEW.md` |
| discovery | request scope + codebase | `FLOW-DISCOVERY.md` |

### Per-stage load contract

The stage-context utility always includes project ground rules, project identity, the guarded-flow
policy when applicable, and the compact workflow record from `pipelines.json`. The table selects
additional project topics and optional machinery sections.

| Stage (skill) | context.md | + context-collection machinery |
| --- | --- | --- |
| feature-analysis | app/domain, modules, architecture, UI, data, high-risk | — |
| bug-investigation | architecture, data, test tooling, high-risk | — |
| solution-design | modules, architecture, UI, data, DI, storage, naming, high-risk | — |
| implementation-plan | modules, architecture, DI, high-risk, naming | — |
| android-dev | modules, architecture, UI, data, DI, storage, naming, high-risk | — |
| testing | architecture, data, UI state, test tooling, high-risk | — |
| qa-plan | app/domain, architecture, UI, UI state, test tooling, high-risk | — |
| automation-test | architecture, UI, UI state, test tooling, high-risk | — |
| review | modules, architecture, UI, data, DI, storage, naming, test tooling, high-risk | — |
| discovery | app/domain, modules, architecture, UI/data patterns, DI | — |

### Stage protocol

1. Resolve the ticket folder and required human inputs.
2. Run `node .aidlc/lib/stage-context.js <stage> --ticket-dir <folder>`; pass `--flow <id>` when
   known. Stop if prerequisites are missing.
3. Read only prior ticket artifacts named by the packet. Inspect project/external evidence only
   within the agent's allowed scope; load conditional guidance only when the change needs it.
4. Perform the stage and write its declared outputs. Do not reconstruct another stage's missing
   artifact.
5. Pass only the compact packet and file artifacts into the next stage. Use a fresh stage context
   when the runtime supports isolation; otherwise discard the prior stage's working set and do not
   carry unreferenced evidence forward.

Implementation/testing fan-out is allowed only for disjoint owned paths. Shared integration files
remain serialized. Analysis, design, planning, and review remain human-visible gates unless the
selected manifest flow says otherwise.

## 12. Testing process

Test changed behavior at the closest useful JVM, Android, Room, Paging, or UI layer. Use
deterministic fakes and in-memory test stores; never production storage or network. Cover relevant
success, failure, cancellation/concurrency, mapping, and regression paths. An authorized testing
stage may execute the smallest relevant command; report exact commands/results and environmental
blockers honestly.
