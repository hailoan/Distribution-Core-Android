---
name: Gradle Module Guideline
description: Protect module dependencies, plugin resolution, SDK/JVM/toolchain alignment, ABI packaging, and publication contracts for build-logic changes.
---

## Gradle Module Guideline

Load when editing `settings.gradle*`, any module build file, version catalogs, Gradle properties,
the `plugin` module, manifests that affect packaging, CMake configuration, consumer rules, or
publication configuration.

Confirm the intended module edge from both the consumer and producer. Use `api` only when the
dependency type must be exposed in the consumer's compile contract; otherwise preserve the local
`implementation` pattern. Check namespace/application IDs, min/compile/target SDK, JVM targets,
build types, benchmark target/package coupling, manifest/resource merging, R8/consumer rules, and
native ABI packaging only where touched.

This repository includes `:plugin` as a regular subproject, while published plugin resolution is a
separate mechanism. Do not assume editing `:plugin` changes modules applying
`com.chiistudio.plugin`. Validate the chosen local-resolution path without publishing. Preserve
environment-only repository credentials and never print secrets.

For public libraries and publication logic, record source/binary/POM/coordinate compatibility and
external-consumer risk. Build, test, dependency inspection, and plugin validation are normal
verification when authorized; Maven Local/private publishing, signing, upload, and distribution
always require separate approval.
