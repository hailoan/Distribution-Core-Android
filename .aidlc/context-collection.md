# AI-DLC Runtime Contract — Distribution Core Android

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
   `.aidlc/context.md`; executable module ownership, dependencies, risk tags, and verification
   defaults live in `.aidlc/modules.json`.
4. Before planning or changing code, name the owning module, changed modules, dependency closure,
   public/native/build contracts, and verification closure. Validate registry edges against
   Gradle/source and keep external consumers visible for public libraries.

## 10. Planning conventions

- Prefer independently testable behavior slices inside the narrowest owning module. A slice may
  cross modules only when a contract or consumer integration requires it; name a primary owner.
- Create shared foundations first only when multiple slices truly depend on them.
- Give every work item a stable ID, dependencies, acceptance contract, owned scope, and test scope.
  Add formal user stories, estimates, priority, or sprint placement only when requested and based
  on a supplied team scale/capacity.
- Order producer-contract work before consumer/wiring work. Serialize public API, JNI/native,
  publication/build logic, DI, migrations/schema, navigation, and shared-state boundaries.
- Add an integration task whenever the change is cross-module, modifies a public/native/build
  contract, changes consumer wiring, or the module registry requires an integration gate.
- The verification matrix must cover the changed module set and the direct/transitive consumers
  whose compile/runtime contract can be affected; device/toolchain-only evidence is explicit.

## 11. Feature → review workflow

The generated `.aidlc/pipelines.json` is the executable flow source. Preserve traceability across
artifacts:

Feature: `FR-ID → SC-ID → AC-ID → Work-ID/Story-ID → Task-ID/module/change → Test-ID → Check-ID/consumer → review status`.
Bug: `ticket → fix Task-ID/module/change → testing Task-ID/Test-ID → Check-ID/consumer → review status`.

### Ticket folder and automation

Resolve one ticket folder: `$AIDLC_OUTPUT_DIR`, otherwise `output/<ticket>/`, or `output/` only when
there is no ticket ID. Read only named ticket artifacts; inspect project/external evidence only as
the stage agent permits. Write only its primary artifact and explicitly allowed support/source
outputs.

Guarded flows (`impl-flow`, `auto-*`, `fixbug-flow`, and `qa-flow`) require the first artifact
line to be `AUTOMATION: CONTINUE` or `AUTOMATION: STOP — reason`. Stop on a missing material
decision, contradictory or untestable requirements, unconfirmed bug causation, an unresolved
external contract, or an action that needs authority not granted by the user. High-risk work
(public APIs, native/JNI, authentication/security/privacy, destructive data, shared DI/state, or
build/publication logic) does not stop merely because it is risky; it requires explicit contract,
consumer-impact, rollback, and verification evidence. Publishing, signing, upload, distribution,
commit, and push remain separately authorized actions.

### Artifact contract

| Stage (skill) | Required ticket inputs / permitted evidence | Writes |
| --- | --- | --- |
| feature-analysis | requirements + optional design/API/docs/code evidence | `DEV-SPEC.md`; converted attachments under `input/`; one ticket-local remote-design cache under `figma/` when needed |
| bug-investigation | bug/crash report + optional logs/trace/code evidence | `BUG-INVESTIGATION.md`; converted attachments under `input/` |
| solution-design | `DEV-SPEC.md` | `SOLUTION-DESIGN.md`; optional `figma/`, `design/`, `screenshot/` |
| implementation-plan | `SOLUTION-DESIGN.md` | `IMPLEMENT-PLAN.md` |
| android-dev | feature: `IMPLEMENT-PLAN.md` + `SOLUTION-DESIGN.md`; bug: `BUG-INVESTIGATION.md`; referenced design support and plan/fix-owned code | `CHANGESET.md` + approved source/resource/config changes |
| testing | `CHANGESET.md` + changed code | `UNIT-TEST-REPORT.md` + approved test source |
| integration-testing | `CHANGESET.md` + `UNIT-TEST-REPORT.md` + changed code and module registry | `INTEGRATION-TEST-REPORT.md`; no production changes |
| qa-plan | `DEV-SPEC.md` (acceptance criteria) | `TEST-CASES.md` |
| automation-test | standalone: `CHANGESET.md` + changed code; qa-flow: `TEST-CASES.md` | `AUTOMATION-TEST-REPORT.md` + approved instrumented test source |
| review | `CHANGESET.md`, `UNIT-TEST-REPORT.md`, `INTEGRATION-TEST-REPORT.md`, and feature design/plan or bug investigation (qa-flow: `AUTOMATION-TEST-REPORT.md` + `TEST-CASES.md` + `DEV-SPEC.md`) | `CODE-REVIEW.md` |
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
| integration-testing | modules, architecture, DI, storage, test tooling, high-risk | §10, §12 |
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

Implementation/testing fan-out is allowed only for disjoint module and path ownership. Shared
contract/integration files remain serialized. The integration-testing stage may run independent
module checks concurrently but produces one dependency-closure verdict. Analysis, design,
planning, and review remain human-visible gates unless the selected manifest flow says otherwise.

## 12. Testing process

Test changed behavior at the closest useful JVM, Android, native, or UI layer. Use deterministic
fakes; never production storage, network, credentials, or publication endpoints. Cover relevant
success, failure, cancellation/concurrency, mapping, lifecycle, and regression paths.

Verification has two gates:

1. `testing` authors and executes focused behavior tests in the owning modules.
2. `integration-testing` computes the affected module/consumer closure, then runs the smallest
   compile/test/package checks that prove crossed contracts still compose. Public library changes
   require compatibility reasoning even with no in-repository caller. Native changes additionally
   track Kotlin/JNI/C++, CMake/linkage, ABI packaging, lifecycle/thread ownership, and required
   device coverage. Build-logic changes verify intended plugin consumers without publishing.

Only an exact recorded green command is passed. A device, credential, SDK/NDK, or toolchain blocker
is reported as `not executed` with the smallest follow-up command; it is never converted into a
pass. The final review decides whether an unexecuted required check blocks release readiness.
