---
name: Architecture Analysis
description: Analyze architectural fit during discovery, solution-design, or review. Return evidenced layers, modules, DI wiring, boundaries, and per-screen UI-toolkit decisions; write nothing.
---

## Architecture Analysis

Use the relevant module, layering, networking, UI-state, DI, and naming topics already present in
the stage packet. Observe actual boundaries and documented legacy exceptions before applying the
configured architecture. Follow established boundaries; introduce a pattern only when the approved
design requires it. Distinguish a pre-existing exception from a regression introduced or worsened
by the proposed change.

### Analyze the decomposition

- **Module owner** owns its public contract and implementation; consumer modules depend only on
  intentionally exposed behavior.
- **Internal layers** are recognized only when source actually contains them. Do not impose
  domain/data/repository boundaries on a technical library module.
- **UI/host** follows the touched module's state/event and View/Surface ownership; it must not
  absorb reusable library behavior merely because it is an app.
- **DI/composition** uses the DI framework and scopes configured for that module. Keep concrete
  construction at proven composition boundaries; no repository-wide DI framework is assumed.

Check focused responsibilities, substitutable implementations, narrow interfaces, dependency
direction, module ownership, and test seams against dependency/blast-radius evidence supplied by
the routing agent. Treat these as project targets, not proof that legacy code already conforms;
do not report an untouched legacy exception as a new defect. Report missing evidence instead of loading another skill. Prefer the smallest
decomposition that satisfies the approved requirements.

### Choose the UI toolkit only when UI changes

For each touched screen, retain its established toolkit unless a clean, approved migration
boundary exists. Evaluate interop/runtime cost, rewrite blast radius, and whether the new surface
is truly self-contained. Do not mix XML and Compose inside a subtree when project rules forbid
it. State the choice and evidence; do not default every new element to Compose.

Return:

- `existing_structure` for discovery/review, or `proposed_decomposition` for design;
- `module_impact`, `di_wiring`, and boundary/SOLID findings;
- `ui_toolkit_decisions` only for touched screens.
