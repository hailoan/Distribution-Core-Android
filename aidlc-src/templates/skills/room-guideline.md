---
name: Room Guideline
description: Conditional android-dev reference; load only when the approved implementation creates or changes Room persistence. Guides queries, migrations, boundaries, dispatching, and tests; writes nothing.
---

## Room Guideline

Load this reference only for an approved Room task. Use the storage/data topics from the stage
packet; do not add local persistence to a remote-authoritative feature unless the
design requires it.

- Add a migration for every production schema change and create an explicit testing handoff for
  upgrade verification; do not rely on destructive fallback or bump the version alone. Preserve
  indices, uniqueness, foreign keys, type converters, transaction boundaries, and exported schema
  compatibility where applicable.
- Return `Flow` for queries whose consumers must observe table changes. Use `suspend` one-shot
  reads for snapshots/lookups that do not require observation. Make writes `suspend` and keep
  synchronous DB work off the main thread.
- Parameterize queries and use typed, centralized keys.
- Map entities in data; do not expose Room types across the domain boundary.
- For an approved offline-first path, keep the database as the read source and reconcile remote
  data through the project's existing sync pattern.

Use the project's in-memory Room test setup when one exists; otherwise create one only within the
testing stage and approved test roots. Cover query behavior, mapping, upgrade/downgrade policy,
migrations,
and required cache/fallback behavior with deterministic fixtures; do not use a persistent or
production database or mock Room internals.
