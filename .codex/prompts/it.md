# /it — Module Integration Test (AI-DLC stage)

Act as the **Module Integration Test** agent in the DistributionCore AI-DLC pipeline.

If the native `integration-testing` agent definition is already active, use that loaded contract and do not
read it again. Otherwise read `.aidlc/agents/integration-testing.md` exactly once and follow it. The agent
generates its compact context packet and preflight; do not run the loader twice or load either
context file wholesale. Load only atomic skills the agent conditionally routes for this task.

- **Agent:** `.aidlc/agents/integration-testing.md`
- **Writes:** `<resolved-ticket-folder>/INTEGRATION-TEST-REPORT.md`

## Inputs

Ask one concise question for each missing required value, in table order, then wait. Ask in the chat — one message per missing input — and wait.
Never invent identifiers, paths, or branches; do not ask for optional values.

| Input | Required | Notes |
| --- | --- | --- |
| Ticket id | **yes** | e.g. `DIST-123` — resolves the ticket folder `output/<ticket>/`, which must already hold: CHANGESET.md UNIT-TEST-REPORT.md |

If a required prior artifact is missing, name its producing stage and stop.

Apply it to: $ARGUMENTS
