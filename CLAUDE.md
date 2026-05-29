# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## ast-graph (managed by AIDLC extension — do not edit by hand)

This project has a pre-built AST graph at `.ast-graph/graph.db`, exposed via the
`ast-graph` MCP server (auto-registered by the AIDLC VS Code extension). The
graph stores every function/class/method/import in the codebase plus their
caller→callee edges, so structural questions can be answered without grepping.

**Prefer ast-graph tools over grep/read when the question is structural.** A
single MCP call is typically 10–50 tokens; the equivalent grep+read sweep across
a 500-file repo is 5k–50k.

Reach for ast-graph first for:
- "where is X defined / who calls X / what does X call" → ast-graph `symbol`
- "if I change X, what breaks" → ast-graph `blast-radius`
- "what does this PR touch structurally" → ast-graph `changed-symbols`
- "find unreferenced code" → ast-graph `dead-code`
- "list HTTP endpoints" → ast-graph `routes`
- "where are the architectural hotspots" → ast-graph `hotspots`
- "fuzzy find a symbol by partial name" → ast-graph `search`

Keep using grep/read/edit for:
- reading function bodies, comments, docstrings (graph stores skeletons, not source)
- editing or refactoring code
- following intent, naming, or non-AST signals (config files, prose)

If the graph looks stale, ask the user to run `AIDLC: Rescan AST Graph`. The
extension also rescans automatically a few seconds after any source file save.

## Build Commands

```bash
./gradlew assembleDebug           # Build all modules (debug)
./gradlew assembleRelease         # Build all modules (release)
./gradlew test                    # Run all unit tests
./gradlew connectedAndroidTest    # Run instrumented tests (requires device/emulator)

# Scope to a single module
./gradlew :network:test
./gradlew :core:test

# Run a single test class or method
./gradlew :core:test --tests "com.chiistudio.core.BaseViewModelUnitTest"
./gradlew :network:test --tests "com.chiistudio.network.RetryTokenUnitTest.test retry token"
```

**Required env vars** (for dependency resolution from the private Maven repo):
`GITHUB_USERNAME`, `GITHUB_ACCESS_TOKEN`, `GITHUB_PUBLISH`

## Project Structure

| Module | Purpose |
|---|---|
| `core` | `BaseViewModel` MVVM base + `ReentrantMutex`, `BaseAdapter` |
| `network` | Ktor HTTP client library with token management and Hilt DI |
| `camera` | OpenGL ES / Camera2 NDK library — see `camera/CLAUDE.md` |
| `plugin` | Custom Gradle publish plugin (`com.chiistudio.plugin`) |
| `benchmark` | Macrobenchmark module |
| `app` | Sample / host app wiring the modules together |

## `core` Module — BaseViewModel

`BaseViewModel<S, A, E>` enforces a Redux-style unidirectional data flow:

- **State `S`** — immutable `StateFlow` snapshot, the single source of truth.
- **Action `A`** — UI intents, dispatched via `sendAction()`.
- **Mutation `M`** — atomic state transforms, queued via `sendMutation()` and processed sequentially through a `Channel`. This guarantees no two mutations race against each other.
- **Effect `E`** — one-shot events (navigation, toasts) emitted via `sendEffect()`.

Override `handleAction(action, state)` to dispatch mutations or effects. Override `handleMutation(mutation, state): S` to return updated state. The base class wires the coroutine pipelines automatically in `init`.

`ReentrantMutex` in the same package is a coroutine-safe reentrant lock used where the standard `Mutex` would deadlock on recursive calls.

## `network` Module — HTTP Client Pattern

Built on Ktor 2 (OkHttp engine) + Hilt. Each API surface follows a four-class decorator chain:

```
BearXxxClient   (BaseClient subclass — owns the HttpClient)
    ↓ wrapped by
BearXxxAuth     (intercepts requests: pre-emptive + reactive token refresh via IRetryToken)
    ↓ wrapped by
BearXxxHeader   (appends static headers via defaultHeader map)
    ↓ composed into
BearXxxService  (the injectable facade — implements IClient by delegating to client)
```

All four are provided by a single Hilt `@Module` with qualifier annotations (`@XxxClient`, `@XxxAuth`, `@XxxHeader`).

**Token wiring** — the library is decoupled from auth implementation via two interfaces set on the `InitNetwork` singleton before Hilt initializes:
- `ITokenManager.getAccessToken(): Pair<String, String>` — supplies (access, refresh) tokens for the Ktor Bearer plugin.
- `IRetryToken` — drives pre-emptive refresh (`shouldRetryToken`) and reactive refresh on 401 (`needRetryToken`).

`RetryTokenManager` is a helper that deduplicates concurrent token refreshes: the second and third callers await the first caller's `Deferred` rather than starting their own refresh.

**Adding a new API service** — create `BearXxxClient`, `BearXxxAuth`, `BearXxxHeader`, `BearXxxService`, and a Hilt module following the weather service as a template (`network/src/main/java/com/chiistudio/network/service/weather/`). Register `InitNetwork.xxxTokenManager` and `InitNetwork.xxxRetryToken` in the application layer.

## Key Tech Versions

| Tech | Version |
|---|---|
| Kotlin | 2.2.10 |
| AGP | 8.9.1 |
| min SDK / compile SDK | 24 / 36 |
| Ktor | 2.3.9 |
| Hilt | 2.57.1 |
| Compose BOM | 2025.08.00 |
| kotlinx.serialization | 1.6.3 |
