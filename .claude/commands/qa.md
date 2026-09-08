# /qa — autonomous AI-DLC (qa flow)

Run the guarded QA / automation-tester flow end-to-end, no human gates: feature-analysis → qa-plan →
automation-test → review. This flow tests behavior from a requirement; it writes no production code.
Resolve the **ticket output folder** once ($AIDLC_OUTPUT_DIR if set, else output/<ticket>/) and use
it for every stage. For each stage, use a fresh native subagent/context when the runtime supports it
and return only its marker, artifact path, and short summary before continuing. A native subagent
already has its agent definition; do not reload it. Without native isolation, read
`.aidlc/agents/<stage>.md` once. Add `--flow qa-flow` to that agent's context-loader command. Use
only the resulting packet, named prior artifacts, and applicable atomic skills. feature-analysis
produces `DEV-SPEC.md` (acceptance criteria), qa-plan turns each AC into reviewable test cases in
`TEST-CASES.md`, automation-test automates the automatable cases (TC-ID → Test-ID) under the
androidTest source set and runs them, and review signs off coverage against the acceptance criteria.
Require each stage artifact's first line to be `AUTOMATION: CONTINUE` or
`AUTOMATION: STOP — <reason>`, and stop immediately on STOP. In automation-test, fan out one
subagent per disjoint test file only; never touch production code. Stop and report at
<ticket folder>/CODE-REVIEW.md.

## Inputs

Ask one concise question for each missing required value, in table order, then wait. Ask in plain text for free-form values (ticket id, paths, links); for a fixed-choice input use the **AskUserQuestion** tool.
Never invent identifiers, paths, or branches; do not ask for optional values.

| Input | Required | Notes |
| --- | --- | --- |
| Ticket id | **yes** | e.g. `DIST-123` — resolves the ticket folder `output/<ticket>/` |
| BA spec / API doc | **yes** | link or file path; a non-`.md` file is converted into `output/<ticket>/input/` first |

If a required prior artifact is missing, name its producing stage and stop.

Apply it to: $ARGUMENTS
