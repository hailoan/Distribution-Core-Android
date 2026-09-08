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

- **Domain** owns business models, use cases, and repository abstractions. It must not depend on
  data-layer types or implementations.
- **Data** implements domain abstractions through services, local sources, DTO/entity mappers,
  and persistence when required.
- **UI** depends on domain-facing contracts and follows the project's state/event model; it must
  not import data implementations.
- **DI/composition** binds implementations using the project's configured DI framework (Koin in
  WorkChat) and
  scopes. Keep concrete construction at composition boundaries.

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
