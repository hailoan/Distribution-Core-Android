---
name: Review Checklist
description: During review, check the diff for architecture, runtime, security, performance, and delivery risks. Return prioritized sourced findings; write nothing.
---

## Review Checklist

Use supplied architecture, data, state, view, DI, storage, coroutine/flavor, naming, high-risk,
testing, and delivery topics.

Establish the local comparison: branch-to-integration diff plus unstaged/staged changes, or the
explicit branch pair. Fetch only with explicit user authorization; otherwise record a missing
local base as a limitation. Record the comparison and use supplied changed-symbol/blast-radius/
call-chain evidence where available.

Check only applicable rows:

- module ownership, domain-to-data dependency direction, focused responsibilities, and no
  DTO/entity leakage;
- project repository, mapping, error, UI-state, one-shot-event, navigation, and reuse patterns;
- every new injectable is resolvable through the project's configured DI framework, with the
  correct lifecycle/scope and composition boundary;
- coroutine cancellation, dispatchers, scopes, lifecycle, and no blocking main-thread work;
- environment-specific values from project configuration rather than hardcoded URLs/names;
- migrations for schema changes, parameterized queries, observable versus one-shot reads used
  appropriately, and no storage type crossing its boundary;
- localization, dead/debug code, credentials, UI state/effects, performance, security, and
  reachable-regression evidence supplied by the routing agent;
- delivery metadata and protected files only when the stage's PR workflow includes them.

For WorkChat, conditionally check Koin registration/duplicate definitions, exported components and
PendingIntent flags, notification channel/intent routing, WebView and deep-link handling, Room
migration/schema compatibility, socket ordering/deduplication, call foreground-service lifecycle,
sensitive message/user/token logging, and flavor-specific endpoints/configuration. Treat an
untouched pre-existing issue as a disclosed observation, not a blocking finding introduced by the
diff.

### Return

Return Critical, Major, and Minor findings, with separate Performance/Security labels when
applicable. Anchor each to `file:line`, name a concrete failure scenario, and filter false
positives. State `blockers` or `nits only`.

Treat tests as passed only when a recorded executed command is green; otherwise report
`not executed` as a verification gap.
