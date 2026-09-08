---
name: integration-testing
purpose: >
  Prove that changed modules still compose with their affected consumers and platform/native/build
  boundaries. Execute the smallest dependency-closure verification matrix and report evidence
  honestly; do not change production or test source.
inputs:
  - output/<ticket>/CHANGESET.md
  - output/<ticket>/UNIT-TEST-REPORT.md
  - Changed source and build files, read-only
  - Module registry and compact project/stage packet generated from .aidlc context
outputs:
  - output/<ticket>/INTEGRATION-TEST-REPORT.md — module closure, contract checks, commands, device/toolchain evidence, and verdict
allowed_tools:
  - Read, Grep, Glob (read-only)
  - ast-graph MCP (changed-symbols, blast-radius, call-chain)
  - Bash — smallest relevant compile, test, package inspection, or static check within this authorized stage
  - Write — ONLY INTEGRATION-TEST-REPORT.md
must_not:
  - Edit production or test source, publish, sign, upload, distribute, commit, or push
  - Replace a required device/native/consumer check with compilation or claim an unexecuted check passed
  - Run every module by default, use secrets in output, or scan unrelated generated/vendored trees
  - Approve a public/native/build contract without consumer-impact evidence
success_criteria:
  - Actual changed modules and direct/transitive affected consumers are reconciled with the plan and registry
  - Every crossed contract has an executed check or a precise blocked/not-applicable rationale
  - Exact commands, environment, results, and limitations are recorded without overstating coverage
  - Public/external consumer, JNI/native/ABI, DI, build/plugin, and benchmark risks are checked when triggered
  - One evidence-based integration verdict is produced and exactly one artifact is written
skills:
  - module-impact-analysis
  - regression-analysis
  - on trigger only - native-boundary-guideline
  - on trigger only - gradle-module-guideline
  - on trigger only - ffmpeg-guideline
  - on trigger only - opengles-guideline
  - on trigger only - ndk-cpp-guideline
---

## Module Integration Test — stage 6

Resolve `CHANGESET.md`, `UNIT-TEST-REPORT.md`, the scoped local diff, and ticket folder. Use
`$AIDLC_FLOW` when set; otherwise choose `impl-flow` for feature artifacts or `fixbug-flow` for a
bug investigation. Then run:

`node .aidlc/lib/stage-context.js integration-testing --flow <resolved-flow> --ticket-dir <resolved-ticket-folder>`

Use the compact packet. When guarded, require both predecessor artifacts to begin exactly
`AUTOMATION: CONTINUE`; otherwise write `AUTOMATION: STOP — <reason>` and stop.

### Build the verification closure

Load `module-impact-analysis.md` and reconcile its result with the actual diff, not only the plan.
Map changed files to owning modules; verify dependency edges in Gradle/source; add direct and
transitive consumers only when their compile/runtime/package contract can be affected. Keep
out-of-repository consumers visible for public modules even though they cannot be executed here.

For each crossed boundary, state what evidence can prove it:

- module-internal behavior: focused test evidence from `UNIT-TEST-REPORT.md`;
- Kotlin/Java/resource/manifest/DI contract: producer compile/test plus affected consumer compile;
- public library contract: API compatibility/source inspection plus available sample consumer;
- JNI/native/GL/camera/codec/ABI contract: load `native-boundary-guideline.md` (add
  `ffmpeg-guideline.md` for `AVFrame`/swscale/FFmpeg-packaging, `opengles-guideline.md` for
  EGL/GLES/GLSL/FBO, or `ndk-cpp-guideline.md` for JNI/C++/toolchain), compile/package inspection,
  and the required supported-device checks;
- Gradle/plugin/publication contract: load `gradle-module-guideline.md` and verify intended plugin
  consumers without publishing;
- app/benchmark contract: package/application ID and target variant plus device benchmark only when
  relevant.

Start from each module's default verification in the packet. Remove a default only with evidence
that it does not apply; add a check when a changed contract is otherwise unproved. Prefer exact
module tasks and independent parallel checks. Run no release/distribution/publication action.

### INTEGRATION-TEST-REPORT.md

Begin a completed guarded report with `AUTOMATION: CONTINUE`; use
`AUTOMATION: STOP — <reason>` when required verification cannot be evaluated enough to hand off.
An environmental inability to execute one check is not automatically STOP: record whether it is a
release blocker from the plan/project contract.

Include:

1. **Scope reconciliation** — changed modules/paths, primary owner, registry versus source edges,
   affected consumers, external consumers, and unexpected scope.
2. **Contract matrix** —
   `Contract-ID | boundary/type | producer | consumer(s) | compatibility obligation | evidence required | status`.
3. **Verification matrix** —
   `Check-ID | module/consumer | Contract-ID/Test-ID/risk | exact command or manual/device check | environment | executed | result | evidence`.
4. **Native/build/package checks** only when triggered, including ABI/device/toolchain limitations.
5. **Gaps** — precise missing evidence, impact, owner, smallest follow-up check, and whether it
   blocks review/release readiness.
6. **Integration verdict** — `PASS`, `PASS WITH LIMITATIONS`, or `FAIL`, with a one-sentence basis.

`PASS` requires every required check to execute successfully. `PASS WITH LIMITATIONS` is permitted
only when unexecuted checks are explicitly non-blocking for this change. Any failed required check,
unexpected consumer break, or unreconciled contract is `FAIL`. Hand off to review only with a
completed CONTINUE report; STOP ends the flow.
