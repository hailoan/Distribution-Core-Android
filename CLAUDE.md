# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 0. Ground rules (read before touching anything)

1. Use a current project knowledge graph first for structural questions when it is available and
   covers the target module. Validate important results against source. Otherwise use focused text/
   symbol search and source inspection; never treat a missing graph edge as proof of no impact.
2. An authorized implementation or testing stage may run the smallest relevant compile, unit-test,
   or static-check command. Commit, push, distribution, signing, upload, and publishing always need
   separate explicit authorization.
3. Match the surrounding architecture, naming, and patterns. Project-specific invariants live in
   `.aidlc/context.md`; executable module ownership, dependencies, risk tags, and verification
   defaults live in `.aidlc/modules.json`.
4. Before planning or changing code, name the owning module, changed modules, dependency closure,
   public/native/build contracts, and verification closure. Validate registry edges against
   Gradle/source and keep external consumers visible for public libraries.

## 0. Project ground rules

- This is a library-oriented, multi-module Android workspace with a sample app. It is not a
  clean-architecture product application and it has no single feature module. Put changes in the
  narrowest owning module and do not introduce domain/data/UI layers that the local code does not
  use.
- Treat Kotlin public APIs, Hilt bindings, Gradle plugin/publication behavior, JNI declarations,
  exported JNI names, C++ ownership, CMake linkage, ABI packaging, and bundled native libraries as
  cross-boundary contracts. Inspect every side of the relevant contract before editing it.
- The code graph is useful for first-party Kotlin/C++ relationships, but its aggregate view is
  dominated by vendored FFmpeg headers and also includes AI-DLC JavaScript. Validate graph results
  against source and do not treat a missing edge as proof that a library API has no external
  consumers.
- Treat `camera/src/main/cpp/ffmpegv2` as vendored headers and prebuilt binaries. Ignore `build/`,
  `.gradle/`, `.cxx/`, and `.externalNativeBuild/` during source analysis. Modify vendored or
  generated content only for an explicitly scoped upgrade or packaging task.
- Preserve the surrounding implementation style and existing module ownership. Do not combine a
  requested change with opportunistic renames, architectural rewrites, dependency upgrades, or
  publication changes.
- The smallest relevant compile or test is normal verification. Committing, pushing, signing,
  publishing, uploading, or distributing an artifact always requires separate explicit approval.

## 1. What the app is

`Distribution-Core-Android` is a Kotlin-first collection of reusable Android experiments and
libraries, plus a small XML/AppCompat host application. The substantive reusable areas are:

- coroutine-based MVVM/state primitives in `core`;
- Ktor/OkHttp networking and token hooks in `network`; and
- Camera2 NDK capture, JNI, EGL/OpenGL ES filtering, frame capture, and MP4 recording in `camera`.

The repository also contains a Gradle publication plugin, an app startup benchmark, and a newly
scaffolded `videolib` library. Current source is mixed Kotlin, Java, and C++; GLSL assets and vendored
FFmpeg C headers/shared objects support the camera pipeline. There is no Compose UI in source, no
Room/database layer, and no product-level clean-architecture feature stack.

The shared catalog declares Kotlin 2.2.10, AGP 8.9.1, compile SDK 36, target SDK 35, and min SDK 24.
Current module exceptions are: `core` compiles with SDK 35, `videolib` has min SDK 21, and `camera`
uses Java/Kotlin 17 while the other Android modules use JVM 11.

---

## Project context → `.aidlc/context.md` · modules → `.aidlc/modules.json` · AI-DLC machinery → `.aidlc/context-collection.md`

Project facts and invariants live in **`.aidlc/context.md`**. Executable module ownership,
dependency edges, risk tags, and default verification live in **`.aidlc/modules.json`**. Stages
load a compact combined packet through `.aidlc/lib/stage-context.js`; section numbers may differ
between projects.

The generic AI-DLC machinery lives in **`.aidlc/context-collection.md`**: ground rules
(§0), planning conventions (§10), the feature→review workflow + per-stage artifact &
load contract (§11), and the testing process (§12).

Before a non-trivial change, generate the compact stage packet so the relevant
project conventions are present without loading both context files in full.

<!-- code-review-graph MCP tools -->
## MCP Tools: code-review-graph

**IMPORTANT: This project has a knowledge graph. ALWAYS use the
code-review-graph MCP tools BEFORE using Grep/Glob/Read to explore
the codebase.** The graph is faster, cheaper (fewer tokens), and gives
you structural context (callers, dependents, test coverage) that file
scanning cannot.

### When to use graph tools FIRST

- **Exploring code**: `semantic_search_nodes` or `query_graph` instead of Grep
- **Understanding impact**: `get_impact_radius` instead of manually tracing imports
- **Code review**: `detect_changes` + `get_review_context` instead of reading entire files
- **Finding relationships**: `query_graph` with callers_of/callees_of/imports_of/tests_for
- **Architecture questions**: `get_architecture_overview` + `list_communities`

Fall back to Grep/Glob/Read **only** when the graph doesn't cover what you need.

### Key Tools

| Tool | Use when |
| ------ | ---------- |
| `detect_changes` | Reviewing code changes — gives risk-scored analysis |
| `get_review_context` | Need source snippets for review — token-efficient |
| `get_impact_radius` | Understanding blast radius of a change |
| `get_affected_flows` | Finding which execution paths are impacted |
| `query_graph` | Tracing callers, callees, imports, tests, dependencies |
| `semantic_search_nodes` | Finding functions/classes by name or keyword |
| `get_architecture_overview` | Understanding high-level codebase structure |
| `refactor_tool` | Planning renames, finding dead code |

### Workflow

1. The graph auto-updates on file changes (via hooks).
2. Use `detect_changes` for code review.
3. Use `get_affected_flows` to understand impact.
4. Use `query_graph` pattern="tests_for" to check coverage.
