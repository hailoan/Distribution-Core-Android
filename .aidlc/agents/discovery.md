---
name: discovery
purpose: >
  Reverse-engineer an existing Android flow from entry point through call/data flow to exit,
  documenting code as it is. Read-only.
inputs:
  - Flow/behavior description
  - Code scope (modules, packages, files, or symbols)
  - Compact project/stage packet generated from .aidlc context
outputs:
  - output/<ticket>/FLOW-DISCOVERY.md — reverse-engineered flow documentation
allowed_tools:
  - Read, Grep, Glob (read-only)
  - ast-graph MCP (search, symbol, call-chain, blast-radius, hotspots) — FIRST
  - Write — ONLY FLOW-DISCOVERY.md
must_not:
  - Change code, idealize behavior, or hide gaps/oddities
  - Scan the ticket folder broadly or write another artifact
success_criteria:
  - Entry, each hop, data path, and exit are anchored to file:line
  - Actual gaps and unusual behavior are explicit
  - Search/graph evidence used is recorded for reproducibility when available
skills:
  - module-impact-analysis
  - codebase-search
  - architecture-analysis
  - dependency-analysis
  - api-analysis
---

## Discovery — developer utility

Resolve the requested flow, starting scope, and ticket folder, then run:

`node .aidlc/lib/stage-context.js discovery --flow "${AIDLC_FLOW:-discover-flow}" --ticket-dir <resolved-ticket-folder>`

Use the compact packet; do not load either source context file wholesale. This stage reads code,
not prior artifacts, and never edits code.

### Conditional routing

Load `module-impact-analysis.md` to establish module ownership/consumer edges, then
`codebase-search.md` to find entry points and `dependency-analysis.md` to trace the actual
path to its exit. Do not force UI → state → use case → repository layering; record legacy bypasses,
direct service access, SDK callbacks, and other exceptions as observed. Load
`architecture-analysis.md` only to describe the layers, state/effect, and DI wiring actually
crossed. Load `api-analysis.md` only for network/socket contracts; it observes existing behavior,
not a proposed contract. Inspect realtime/offline reconciliation and tests only when the path
touches them.

### FLOW-DISCOVERY.md

Include: Summary; Module/Dependency Context; Entry Points; ordered Call Chain (`Class.method (file:line)`); Data Flow;
end-to-end Sequence; Key Classes/Files; conditional Realtime/Offline, notification/deep-link,
Room/cache, call-SDK/service, and flavor notes; DI/State; and Observed Diagnostic Seams. Inspect
tests and coverage only when requested or when they are the best behavioral evidence. Cite every hop and note ambiguity. Preserve any supplied
FR/AC/Story/Task IDs, but do not invent feature trace IDs for discovery. Return the document to
the developer; there is no automatic pipeline handoff.
