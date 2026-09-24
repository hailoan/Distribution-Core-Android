---
name: Review Checklist
description: During review, check the diff for architecture, runtime, security, performance, and delivery risks. Return prioritized sourced findings; write nothing.
---

## Review Checklist

Use supplied module topology, architecture, data, state, view, DI, native/build, naming, high-risk,
testing, integration, and delivery topics.

Establish the local comparison: branch-to-integration diff plus unstaged/staged changes, or the
explicit branch pair. Fetch only with explicit user authorization; otherwise record a missing
local base as a limitation. Record the comparison and use supplied changed-symbol/blast-radius/
call-chain evidence where available.

Check only applicable rows:

- every changed path belongs to its declared owning module; actual Gradle/source edges match the
  module-impact report; public/external consumers are not silently treated as absent;
- producer contracts precede consumer wiring, dependency direction is intentional, and no
  implementation type leaks through a public boundary accidentally;
- project repository, mapping, error, UI-state, one-shot-event, navigation, and reuse patterns;
- every new injectable is resolvable through the project's configured DI framework, with the
  correct lifecycle/scope and composition boundary;
- coroutine cancellation, dispatchers, scopes, lifecycle, and no blocking main-thread work;
- environment-specific values from project configuration rather than hardcoded URLs/names;
- native/JNI signature symmetry, lifecycle/thread/resource ownership, ABI/package/linkage, and
  device evidence when native surfaces changed;
- Gradle dependency exposure (`api` versus `implementation`), SDK/JVM/toolchain alignment,
  manifest/resource merging, plugin resolution, and publication compatibility when build surfaces changed;
- localization, dead/debug code, credentials, UI state/effects, performance, security, and
  reachable-regression evidence supplied by the routing agent;
- delivery metadata and protected files only when the stage's PR workflow includes them.

For Distribution Core Android, conditionally check `BaseViewModel` mutation ordering/cancellation,
`RetryTokenManager` single-flight/reset/error behavior, Ktor client/plugin lifetime and token/header
logging, Hilt qualifier/host aggregation, Kotlin↔JNI signatures, Camera2 request control reapply
order, EGL context/thread ownership, filtered recording path, MediaCodec/Muxer finalization,
FFmpeg/ABI/16 KB packaging, app/benchmark package coupling, and publication plugin resolution.
Treat an untouched pre-existing issue as a disclosed observation, not a blocking finding introduced
by the diff.

### Return

Return Critical, Major, and Minor findings, with separate Performance/Security labels when
applicable. Anchor each to `file:line`, name a concrete failure scenario, and filter false
positives. State `blockers` or `nits only`.

Treat tests and integration checks as passed only when a recorded executed command is green;
otherwise report `not executed` as a verification gap. Recompute the verification closure when the
actual diff differs from the plan.
