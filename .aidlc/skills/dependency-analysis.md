---
name: Dependency Analysis
description: Measure confirmed symbol or diff impact during discovery, feature-analysis, bug-investigation, solution-design, or implementation-plan. Return a bounded stage-specific impact map; write nothing.
---

## Dependency Analysis

Use relevant module, DI, storage, and high-risk topics from the stage packet. Analyze
confirmed targets supplied by the routing agent; return unknown targets as evidence gaps rather
than loading another skill.

Use a current `ast-graph` index when available and validate its results against source:

- `changed-symbols` for a diff's structural surface;
- `blast-radius` for intended symbol changes;
- `call-chain` for affected flows;
- `dead-code` only when reference status matters.

When graph support is absent, stale, or incomplete, fall back to focused text/symbol search. Never
treat a missing graph edge as proof of no impact. Inspect applicable non-symbol dependencies:
Gradle project edges and `api`/`implementation` exposure, AndroidManifest/resources, DI bindings,
serialization/reflection names, JNI exports and callbacks, CMake linkage, native ABI packaging,
ProGuard/R8/consumer rules, version catalogs/plugin resolution, and build-variant assets.

Survey direct callers/callees plus one hop (`depth <= 2`). Go deeper only for a declared
high-risk boundary or an explicitly required end-to-end trace.

Return:

- for feature-analysis: affected areas, direct dependencies, shared infrastructure, and unknowns;
  omit speculative future symbols or waves;
- for discovery, bug-investigation, or solution-design: direct callers/callees, flow
  touchpoints, module boundaries, and high-risk exposure;
- for implementation-plan: the same evidence plus work that can run concurrently and work that
  must serialize because it edits a shared boundary.

When module ownership or consumer closure affects scope, delegate the canonical module contract and
verification matrix to `module-impact-analysis`; this skill supplies symbol/caller evidence for it.

Anchor findings to symbols and `file:line`; do not infer impact from filenames alone.
