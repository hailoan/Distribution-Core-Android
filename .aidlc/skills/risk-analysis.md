---
name: Risk Analysis
description: During feature-analysis, bug-investigation, solution-design, implementation-plan, or review, identify evidence-backed risks in touched code and proposed changes. Return prioritized stage-specific risks; write nothing.
---

## Risk Analysis

Use only high-risk/touched-system packet topics and ticket, design, contract, symbol, or
blast-radius evidence supplied by the routing agent. Report an evidence gap rather than loading
search or dependency skills.

Consider:

- existing fragility in reached code, concurrency, identifiers, schemas, and contracts;
- declared high-risk sync, DI, release, storage, or shared boundaries;
- introduced coupling, blast radius, migration, design/spec mismatch, or UI-paradigm interop.

Omit generic risks without evidence.

Return by mode:

- feature-analysis:
  `risk | evidence | potentially affected area | severity | introduced by change (yes/no/uncertain) | owner | needs design decision`;
- bug-investigation, solution-design, implementation-plan, or review:
  `risk | evidence | blast radius | severity | introduced by change (yes/no/uncertain) | owner | mitigation`.

Mitigations may require an impact trace, focused test, serialized edit, rollback, monitoring, or
explicit design decision. Do not hide an unmitigated high-severity risk or solve unrelated work.
