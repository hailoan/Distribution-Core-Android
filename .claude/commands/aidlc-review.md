# /aidlc-review — Review (AI-DLC stage)

Act as the **Review** agent in the DistributionCore AI-DLC pipeline.

If the native `review` agent definition is already active, use that loaded contract and do not
read it again. Otherwise read `.aidlc/agents/review.md` exactly once and follow it. The agent
generates its compact context packet and preflight; do not run the loader twice or load either
context file wholesale. Load only atomic skills the agent conditionally routes for this task.

- **Agent:** `.aidlc/agents/review.md`
- **Writes:** `<resolved-ticket-folder>/CODE-REVIEW.md`

## Inputs

Ask one concise question for each missing required value, in table order, then wait. Ask in plain text for free-form values (ticket id, paths, links); for a fixed-choice input use the **AskUserQuestion** tool.
Never invent identifiers, paths, or branches; do not ask for optional values.

| Input | Required | Notes |
| --- | --- | --- |
| Ticket id *(or the branch pair)* | **yes** | pipeline review: resolves `output/<ticket>/`, which must already hold `CHANGESET.md` + `UNIT-TEST-REPORT.md`. Standalone branch review: give the branches instead |

If a required prior artifact is missing, name its producing stage and stop.

Apply it to: $ARGUMENTS
