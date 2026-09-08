---
name: API Analysis
description: Observe existing API contracts during discovery, feature-analysis, or solution-design when work touches networking. Return sourced contracts and gaps; write nothing and never design endpoints.
---

## API Analysis

Use this only to observe contracts that already exist. Use the networking/data-access topic from
the stage packet, then inspect referenced API documentation and service/client/DTO paths already
identified by the routing agent. If no relevant path is supplied, report the gap rather than load
another skill. Keep inspection to the smallest evidence set under the routing stage's budget; a
service, request/response, wrapper, mapper, and repository may all be required for one operation.

For each relevant operation, record:

`operation/event | transport/method | auth/headers | request/event shape | response shape | errors | pagination/version/environment | source`

Anchor code facts to `file:line` and document facts to their section. Mark conflicts,
version-sensitive details, and missing contracts as open questions.

Return `observed_contracts` and `gaps`. Do not infer undocumented behavior or propose an
endpoint, payload, or error model. In solution-design, the loading agent may make a separate
design decision after treating a gap as unresolved input.
