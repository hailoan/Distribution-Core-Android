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
AndroidManifest entries, navigation/resources, Koin registration, Room schemas, serialization or
reflection names, ProGuard/R8 rules, Gradle/version-catalog configuration, and flavor assets.

Survey direct callers/callees plus one hop (`depth <= 2`). Go deeper only for a declared
high-risk boundary or an explicitly required end-to-end trace.

Return:

- for feature-analysis: affected areas, direct dependencies, shared infrastructure, and unknowns;
  omit speculative future symbols or waves;
- for discovery, bug-investigation, or solution-design: direct callers/callees, flow
  touchpoints, module boundaries, and high-risk exposure;
- for implementation-plan: the same evidence plus work that can run concurrently and work that
  must serialize because it edits a shared boundary.

Anchor findings to symbols and `file:line`; do not infer impact from filenames alone.
