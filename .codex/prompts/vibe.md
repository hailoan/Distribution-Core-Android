# /vibe — autonomous AI-DLC (vibe flow)

Run the guarded, module-aware vibe flow end-to-end: feature-analysis → solution-design →
implementation-plan → android-dev → testing → integration-testing → review. The request in this
chat is the authoritative feature input; a separate BA document and ticket id are optional. If no
ticket id is supplied, derive a readable lowercase slug and use `output/vibe-<slug>/` (append a
numeric suffix if it already exists). `$AIDLC_OUTPUT_DIR` still wins. Resolve the folder once and
use it for every stage. Do not ask the user to choose a module: use `.aidlc/modules.json` and source
evidence to identify the owner and dependency closure. Ask only when a missing decision can
materially change behavior, data/security, a public contract, or authorized scope. For each stage,
use a fresh native subagent/context when the runtime supports it and return only its marker,
artifact path, and short summary before continuing. A native subagent already has its agent
definition; do not reload it. Without native isolation, read `.aidlc/agents/<stage>.md` once.
Add `--flow impl-flow` to that agent's context-loader command. Use only the resulting packet,
named prior artifacts, and applicable atomic skills. Require each stage artifact's first
line to be `AUTOMATION: CONTINUE` or
`AUTOMATION: STOP — <reason>`, and stop immediately on STOP. In android-dev, implement parallel
work only when module and file ownership are disjoint; serialize public contracts, JNI/native
boundaries, DI, shared state, and build configuration. Testing owns focused tests.
Integration-testing verifies each changed module plus its affected consumer closure and records
device/toolchain checks as executed or explicitly blocked. Instrumented/UI automation is selected
only when the plan requires it; use `/qa` for requirement-driven UI automation.

If review returns No-Go only for defects introduced by this change and correction stays inside the
approved design and plan, run at most two bounded repair cycles: android-dev → testing →
integration-testing → review. Never widen scope during repair. Stop for a changed contract/design
decision, unavailable authority, external blocker, or the third No-Go. Return the final result from
<ticket folder>/CODE-REVIEW.md.

## Inputs

Required: a concrete feature/change request in the invocation or current chat. Optional: ticket id,
acceptance details, constraints, links, and local attachments. Do not require a duplicate BA spec.
If the request itself is missing, ask one concise question and wait. Ask in the chat — one message per missing input — and wait.

Apply it to: $ARGUMENTS
