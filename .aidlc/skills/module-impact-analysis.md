---
name: Module Impact Analysis
description: Resolve module ownership, dependency/consumer closure, crossed contracts, and verification scope for every code-affecting AI-DLC stage. Read-only.
---

## Module Impact Analysis

Use the module topology already embedded by `stage-context.js`; `.aidlc/modules.json` is its
authoritative generated source. Validate consequential edges against Gradle files, manifests,
source imports/callers, JNI/CMake declarations, and plugin resolution. A missing graph edge or
in-repository caller never proves that a public library has no consumer.

For the requested behavior or confirmed bug:

1. Map each target path/symbol to exactly one owning module. If ownership is ambiguous, return the
   competing candidates and the evidence needed to decide.
2. Compute the proposed changed-module set, its direct consumers, and only the transitive consumers
   whose compile, runtime, package, or behavior contract can change.
3. Identify every crossed boundary: Kotlin/Java API, resources/manifest, DI, serialization,
   coroutine/state behavior, JNI signature/ownership, CMake/native linkage, ABI packaging, Gradle
   plugin/publication, or benchmark target/package.
4. Classify compatibility as `internal`, `source`, `binary`, `runtime/behavior`, `native/ABI`, or
   `build/publication`. Preserve external-consumer risk as `unknown` when it cannot be inspected.
5. Derive a minimal verification closure from each module's defaults. Remove a default only with a
   concrete reason; add consumer/device/toolchain checks when a crossed contract needs them.

Return this compact contract to the routing stage:

- `primary owner` and evidence;
- `changed modules` with purpose;
- `dependency closure`: producer → consumer edge, semantics, evidence, impact;
- `contract matrix`: boundary, owner, consumers, compatibility obligation, risk;
- `verification closure`: module/contract → exact command or manual/device check → why required;
- unknowns and confidence.

Stage use:

- feature analysis records a bounded ownership/impact hypothesis, not a design;
- bug investigation traces the failing contract and affected consumers;
- solution design fixes module responsibilities and compatibility obligations;
- implementation planning turns the contract and verification closures into owned tasks;
- android-dev revalidates ownership before each task and after contract changes;
- testing targets changed behavior in the owning module;
- integration-testing executes the verification closure;
- review reconciles the actual diff, consumers, and executed evidence against the closure.

Do not add modules, dependencies, wrappers, or shared abstractions merely to make the graph look
clean. Do not broaden to every module when a consumer is provably unaffected.
