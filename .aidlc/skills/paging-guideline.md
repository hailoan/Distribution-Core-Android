---
name: Paging Guideline
description: Conditional android-dev reference; load only when the approved implementation uses Paging 3 for a large or unbounded list. Guides boundaries, load states, caching, and tests; writes nothing.
---

## Paging Guideline

Load this reference only for an approved Paging 3 task. Do not add Paging to a small fixed list.
Use networking, storage, and existing paging patterns from the stage packet.

Preserve the project's established Paging boundary. Place `PagingSource`, `Pager`,
`PagingData`, mapping, and caching where comparable code places them; do not make domain depend
on Android Paging types unless that is already the approved project pattern. If no pattern
exists, follow SOLUTION-DESIGN rather than inventing one here.

- Use the API's stable cursor when provided; for page-number APIs, preserve the observed page/base,
  end-of-list, invalidation, and refresh-key contract rather than inventing cursor semantics.
- Use `RemoteMediator` with Room as the read source only when offline-first behavior is required
  and the project uses that pattern; otherwise do not introduce a cache boundary.
- Map DTO/entity types before they cross the project's established boundary.
- Cache the stream in the lifecycle scope used by existing ViewModels.
- Define stable item identity and reconcile duplicates, invalidation, and applicable out-of-order
  socket updates with the existing source-of-truth policy. Render refresh, append, empty, error,
  and retry from `LoadState`; do not duplicate the whole
  list beside the pager or invent parallel booleans.

Test key progression, refresh/invalidation, append/error results, duplicate identity, mapping, and retry with fakes and coroutine
test tools. For a mediator path, use the project's Room test pattern and no real network.
