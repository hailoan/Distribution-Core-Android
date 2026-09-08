---
name: Regression Analysis
description: During bug-investigation, testing, or review, derive the reachable behavior that a change could regress. Return a scoped, evidence-backed re-verification checklist; write nothing.
---

## Regression Analysis

Use high-risk/testing packet topics and changed-symbol/reachability evidence supplied by the
routing agent; do not open source context files or load another skill. Do not list generic app
areas when reachability is unknown.

For each changed mechanism:

1. include reachable callers/callees and shared infrastructure;
2. add only touched high-risk paths such as sync, concurrency, navigation, or migration;
3. identify contracts, state ordering, cache reconciliation, or shared state the mechanism could
   disturb;
4. map the risk to the project's verifying layer and an existing or needed automated/manual
   check.

Include applicable non-call-graph surfaces: resources, manifests, navigation/deep links,
serialization/reflection names, Room schemas, notifications, flavor configuration, ProGuard/R8,
and SDK callback registration.

Return:

`could_regress | causal link to change | verification | source/owner`

Keep unknown coverage explicit. Exclude unrelated screens and generic checks.
