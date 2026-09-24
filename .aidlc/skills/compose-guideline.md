---
name: Compose Guideline
description: Conditional android-dev reference; load only when the approved implementation creates or changes Compose UI. Guides state, effects, recomposition, and design-to-Compose translation; writes nothing.
---

## Compose Guideline

Load this reference only for a Compose implementation task. Use the UI-state and view-layer
topics from the stage packet: the project's scaffold, event/state types, theme, navigation,
and lifecycle collection APIs. Use routing-agent reuse evidence before adding a shared composable.

### Implement Compose behavior

- Pass immutable state and callbacks into stateless UI; keep repositories, storage, and DI out of
  composables.
- Keep business/screen state in the project's ViewModel/state holder. Use `remember` or
  `rememberSaveable` only for transient UI state.
- Send user events upward and render loading, empty, success, and error from the established
  state model. Represent one-shot effects using the project's pattern.
- Collect observable state with the project's lifecycle-aware API and preserve accessibility
  semantics, content descriptions, focus, and touch targets.
- Use keyed UI-scoped effects; do not hide business initialization in `LaunchedEffect`.
- Key lazy items, avoid hot-path allocation, and preserve stable inputs. Do not mix UI paradigms
  where project rules prohibit it.
- When Compose is hosted by a Fragment/View screen, keep one navigation and state owner and follow
  the existing interop/disposal strategy; do not create a parallel Compose-only flow.

### Implement a supplied design

For prepared Figma sources, open the supplied `output/<ticket>/figma/manifest.json`. Select the
frame or state set by the UI task's exact `Design-Ref`/`Design-Refs`, then open only those manifest
entries' `digest` and `screenshot` paths. Never choose a visually similar frame, derive a filename from a node ID, glob
raw frames, or load raw JSON when a digest is available. If the Design-Ref is missing, ambiguous,
or points to unreadable evidence, stop the UI task and report the broken planning/design handoff.

Use the digest's measured padding, spacing, radii, typography, fills/strokes, constraints, and
FILL/HUG/FIXED semantics. Map:

- FILL/grow to available-space or weight modifiers;
- HUG to wrap-content behavior;
- FIXED to literal dimensions only where the design fixes them;
- alignment/constraints to Compose arrangement/alignment.

Preserve proportions instead of blanket-scaling dp. Map design colors to approved theme roles and
put user-facing text in localized resources. For TSX, images, or other sources, use only
measurements they explicitly provide and record missing values rather than inventing them.

Before completing the task, compare the implemented state(s) with the referenced screenshot/digest
for hierarchy, content, spacing, sizing, typography, colors/theme mapping, enabled/disabled/loading/
empty/error states, accessibility semantics, and responsive constraints. Record this as design
conformance evidence in CHANGESET; do not claim pixel-perfect or screenshot verification unless an
actual render/comparison was executed.
