# Project Context — {{PROJECT_NAME}} ({{PROJECT_SHORT}})

This file contains project-specific facts for the WorkChat repository; stages use
`.aidlc/lib/stage-context.js` to load only the relevant sections. Generation preserves an
existing `.aidlc/context.md` unless `AIDLC_REFRESH_CONTEXT=1` is set.

## 0. Project ground rules

- Preserve existing XML/Fragment versus Compose ownership per screen; migration requires an
  explicit boundary decision. Treat navigation, Koin modules, Room schemas, socket/realtime state,
  notifications, call services/SDK integration, and flavor/build configuration as shared-risk
  surfaces that require caller/configuration impact checks before edits.
- Use a current project knowledge graph when available and validate it against source. Otherwise
  use focused `rg`/symbol search; never infer no callers from missing graph data.

## 1. What the app is

**{{PROJECT_NAME}}** ({{PROJECT_SHORT}}) is a **{{ENV_PROJECT_TYPE}}** application.

- Language: **{{ENV_LANGUAGE}}**
- Architecture: **{{ENV_ARCHITECTURE}}**
- UI: **{{ENV_UI_FRAMEWORK}}**
- DI: **{{ENV_DI_FRAMEWORK}}**
- SDK: min **{{ENV_MIN_SDK}}**, target **{{ENV_TARGET_SDK}}**

WorkChat is an enterprise messaging application covering conversations, messages/media, search,
tasks, notifications, profile/settings, stories, calls, and realtime/offline synchronization. It
integrates backend REST/socket services, Firebase, Room, WorkManager, and the local `callsdk` and
`network` modules.

## 2. Modules / structure

- Primary feature module: **{{FEATURE_MODULE}}**
- Default source root: `{{ANDROID_DEV_ARTIFACT}}/{{PACKAGE_PATH}}`
- Declared modules:

{{ENV_MODULES_MD}}

- `app`: host/application packaging and integration.
- `workchat`: primary product UI, domain, data, services, notifications, tasks, and feature code.
- `callsdk`: call-package behavior and media/service integration.
- `network`: shared networking support.

New feature code normally belongs in the narrowest existing feature package under `workchat`.
Shared modules or packages require multiple concrete consumers or an explicit architecture decision.

## 3. Architecture

Configured pattern: **{{ENV_ARCHITECTURE}}**. Domain owns repository abstractions; data implements
them; UI does not depend on data implementations. Concrete wiring belongs in DI.

Use surrounding code as the canonical pattern. New code should keep domain contracts/models inward,
data implementations and mapping outward, and UI dependent on domain-facing behavior. The codebase
contains legacy exceptions and direct framework/service paths; preserve or isolate them rather than
claiming they already conform or expanding them without approval.

## 4. Networking / data access

Retrofit/service clients and remote/local data sources feed repository implementations; DTO/entity
mapping remains in data-facing packages and must not leak inward. Mirror the nearest established
repository error/result convention. Room-backed/offline and socket-backed features require an
explicit source-of-truth, ordering, deduplication, retry, and reconciliation decision.

## 5. UI state management

The repository contains conventional ViewModels plus feature-specific reducer/store and
state/effect patterns. Mirror the touched feature rather than imposing one global MVI shape. Keep
durable state separate from one-shot navigation/toast effects and collect lifecycle-aware.

## 6. View layer — {{ENV_UI_FRAMEWORK}}

WorkChat is hybrid: many flows use Fragments/XML and newer or isolated surfaces use Compose. Retain
the touched screen's toolkit unless an approved migration boundary exists. Reuse existing
navigation/scaffold/theme conventions, localize user-visible strings, and preserve accessibility,
state restoration, deep links, and back-stack behavior.

### Reusable UI and base scaffolding

Search the touched feature and common UI/component packages for a semantically compatible scaffold,
dialog, control, adapter/binder, or composable. Keep one-off UI feature-local; promote it only when
project convention or multiple concrete consumers justify a stable shared contract.

## 7. Dependency injection — {{ENV_DI_FRAMEWORK}}

Koin is the active DI system. Definitions use `module { single/factory/viewModel { ... } }` and are
loaded from `WorkChatApplication`/feature composition roots. Mirror nearby scope and constructor
patterns, verify each new injectable is loaded exactly once, and check worker/service/SDK entry
points that are created outside normal UI composition.

## 8. Storage, coroutines, flavors / build

Room is used for durable local data; production schema changes require non-destructive migrations,
schema compatibility, and upgrade tests. Preserve structured concurrency, cancellation, dispatcher
ownership, and lifecycle scopes. Gradle uses version catalogs plus project build logic and multiple
environment variants with `BuildConfig` values. Prefer the narrowest module/variant compile or test
task. Distribution scripts, signing material, publishing, and environment credentials are protected.

## 9. Naming conventions

Mirror the nearest feature for UseCase, Repository/Impl, Remote/LocalDataSource, mapper, ViewModel,
Koin module, resource, and test naming. Preserve public API names and intentional legacy spellings;
do not perform opportunistic renames in feature work.

## 10. Testing stack

Tests use JUnit 4 and MockK, with JVM tests under `src/test` and Android/instrumented tests under
`src/androidTest`; mirror neighboring coroutine/dispatcher rules, fakes, fixtures, and Room/Paging
patterns. An authorized implementation/testing stage may run the narrowest relevant Gradle test or
compile task. Device-dependent tests require an available device/emulator. Release/distribution
tasks always require separate authorization.

## 11. High-risk areas

High-risk surfaces include shared API/socket contracts, Koin registration, navigation/deep links,
Room migrations and message source-of-truth, realtime/offline ordering, notification intents and
channels, foreground call services/SDK callbacks, concurrency/cancellation, WebView/auth/privacy,
sensitive logs, flavor endpoints, and Gradle/release logic. Inspect reachable callers plus manifest,
resource, schema, DI, and configuration dependencies before changing them.
