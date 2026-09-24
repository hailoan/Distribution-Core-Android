---
name: Codebase Search
description: Locate minimal code evidence during discovery, feature-analysis, or bug-investigation. Return entry points, current behavior, and unknowns; write nothing and stop when the question is answered.
---

## Codebase Search

Use only task-relevant module, architecture, data-path, and high-risk topics from the stage packet.

Use a current `ast-graph` index when it is available and covers the target module:

- `search` to find entry symbols;
- `symbol` to inspect definitions and direct callers/callees;
- `call-chain` to trace the relevant execution path.

Validate graph conclusions against source. When the graph is absent, stale, or incomplete, use
focused `rg`/symbol search and source inspection; never treat a missing graph edge as proof that a
caller does not exist. Read bodies/comments only when structural evidence cannot answer. Do not
recursively scan. Skip unrelated and confirmed dead code; inspect deprecated code when it remains
reachable, and inspect a focused test only when it is the best behavioral evidence.
Honor the loading stage's evidence budget.

Before every search, ask whether the entry point and current behavior are already evidenced.
Stop when they are; report remaining uncertainty instead of searching for reassurance.

Return:

- `entry_points`: symbols anchored to `file:line`;
- `current_state`: a bounded behavior survey, not every call site;
- `touched_areas`: APIs, models, navigation, components, or storage;
- `high_risk_entries` for an optional dependency pass by the routing agent;
- `unknowns`.

For greenfield work, return `current_state: none; existing_code_impact: none`.
