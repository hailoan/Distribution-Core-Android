---
name: Reuse Detection
description: During feature-analysis, solution-design, or android-dev, find semantically compatible existing scaffolds, UI, use cases, repositories, and mappers. Return stage-appropriate reuse evidence; write nothing.
---

## Reuse Detection

Use relevant reusable-component, base-scaffold, data-pattern, and naming topics from the stage
packet. Confirm candidates against entry-point/code evidence supplied by the routing agent; report
an evidence gap instead of loading another skill.

Compare behavior and extension seams, not names alone. Check screen scaffolds, components,
interactions, use cases, repository operations, and mappers. Reuse or extend a candidate only when
its contract fits without feature-specific conditionals or boundary violations.
Classify each candidate as a canonical pattern to copy, a reusable implementation, or a reachable
legacy pattern that should not be propagated.

Do not generalize prematurely:

- Keep genuinely feature-specific code in the feature.
- Put new code in a shared package only when project convention requires it or multiple concrete
  consumers justify a stable abstraction.
- Do not move a one-off into shared code for hypothetical future reuse.
- Prefer small duplication over a misleading abstraction when requirements are still diverging;
  record the tradeoff for later consolidation.

Return by mode:

- feature-analysis: `candidate | location | evidence | apparent fit | confidence`; flag possible
  duplication but make no placement/extension decision;
- solution-design/android-dev: `candidate | reuse/extend decision | extension seam | location`,
  plus justified feature-local or shared additions and unresolved fit questions.
