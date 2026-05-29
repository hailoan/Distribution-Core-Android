#!/bin/bash

# =========================================================
# WorkChat (FPT Chat) AI DLC Workspace Bootstrap Script
# =========================================================
# Generates:
#   .aidlc/workspace.yaml  — agents, skills, pipelines, slash commands
#   .aidlc/skills/*.md     — skill prompts for each agent role
#   output/.gitkeep        — artifact output directory
#   docs/README.md         — command reference
#
# Project: WorkChat / FPT Chat (com.fpt.beatchat)
# Stack:   Kotlin · AGP 8.7 · Koin · Room · Coroutines+Flow
#          Retrofit+Ktor · Compose+XML hybrid · min SDK 28
#
# Usage:
#   chmod +x .aidlc/generate-skill.sh
#   ./.aidlc/generate-skill.sh
# =========================================================

set -e

echo "🚀 Creating WorkChat AI DLC workspace..."

mkdir -p .aidlc/skills
mkdir -p docs
mkdir -p output
touch output/.gitkeep

# =========================================================
# workspace.yaml
# =========================================================

cat > .aidlc/workspace.yaml <<'EOF'
name: WorkChatFeatureFactory
version: "2.0"

agents:

  - id: ba-analyze
    name: BA Analyze
    skill: ba-analyze
    model: claude-opus-4-7
    position: Business Analyst
    description: Analyze business requirements and convert to WorkChat Android-ready specs.
    artifact: output/BA-SPEC.md

  - id: solution-design
    name: Solution Design
    skill: solution-design
    model: claude-opus-4-7
    position: Android Architect
    description: Design Android architecture and technical solution aligned with WorkChat modules.
    artifact: output/SOLUTION-DESIGN.md

  - id: planning
    name: Planning
    skill: planning
    model: claude-sonnet-4-6
    position: Tech Lead
    description: Break feature into implementation tasks with module impact analysis.
    artifact: output/IMPLEMENT-PLAN.md

  - id: android-dev
    name: Android Developer
    skill: android-dev
    model: claude-sonnet-4-6
    position: Senior Android Developer
    description: Implement Android feature following WorkChat conventions (Koin, Clean Arch, Coroutines+Flow).
    artifact: output/feature/

  - id: unit-test
    name: Unit Test
    skill: unit-test
    model: claude-sonnet-4-6
    position: QA Automation Engineer
    description: Write and run unit tests for ViewModel, UseCase, and Repository layers.
    artifact: output/UNIT-TEST-REPORT.md

  - id: code-review
    name: Code Review
    skill: code-review
    model: claude-opus-4-7
    position: Principal Android Engineer
    description: Review architecture, code quality, and WorkChat-specific conventions.
    artifact: output/CODE-REVIEW.md

  - id: integration-test
    name: Integration Test
    skill: integration-test
    model: claude-sonnet-4-6
    position: QA Engineer
    description: Generate integration test plan and validate logic against UAT environment specs.
    artifact: output/INTEGRATION-TEST.md

  - id: release-check
    name: Release Check
    skill: release-check
    model: claude-sonnet-4-6
    position: Release Manager
    description: Validate release readiness across dev/uat/stag/prod flavors. Requires manual Crashlytics data.
    artifact: output/RELEASE-NOTE.md

  - id: dev-check
    name: Dev Check
    skill: dev-check
    model: claude-sonnet-4-6
    position: Senior Android Developer
    description: Pre-PR self-review — flavor safety, Koin DI, coroutine correctness, Room migrations, debug artifacts, PR hygiene.
    artifact: output/DEV-CHECK.md

  - id: figma-inspect
    name: Figma Inspect
    skill: figma-inspect
    model: claude-sonnet-4-6
    position: Senior Android UI Engineer
    description: Read a Figma URL via MCP, extract design tokens and component structure, produce an Android Compose implementation spec.
    artifact: output/FIGMA-SPEC.md

skills:
  - id: ba-analyze
    path: ./.aidlc/skills/ba-analyze.md

  - id: solution-design
    path: ./.aidlc/skills/solution-design.md

  - id: planning
    path: ./.aidlc/skills/planning.md

  - id: android-dev
    path: ./.aidlc/skills/android-dev.md

  - id: unit-test
    path: ./.aidlc/skills/unit-test.md

  - id: code-review
    path: ./.aidlc/skills/code-review.md

  - id: integration-test
    path: ./.aidlc/skills/integration-test.md

  - id: release-check
    path: ./.aidlc/skills/release-check.md

  - id: dev-check
    path: ./.aidlc/skills/dev-check.md

  - id: figma-inspect
    path: ./.aidlc/skills/figma-inspect.md

environment:
  project_type: android
  language: kotlin
  architecture: clean-architecture
  ui_framework: compose-xml-hybrid
  di_framework: koin
  min_sdk: 28
  target_sdk: 35
  modules:
    - app
    - workchat
    - callsdk
    - network

slash_commands:
  - name: /plan
    agent: ba-analyze

  - name: /design
    agent: solution-design

  - name: /task
    agent: planning

  - name: /implement
    agent: android-dev

  - name: /ut
    agent: unit-test

  - name: /workchat-review
    agent: code-review

  - name: /it
    agent: integration-test

  - name: /release
    agent: release-check

  - name: /figma
    agent: figma-inspect

  - name: /dev
    agent: dev-check

  - name: /vibe
    agent: solution-design

pipelines:

  # Full feature delivery — BA spec through release
  - id: workchat-feature-flow
    name: WorkChat Feature Delivery Flow
    steps:

      - agent: ba-analyze
        enabled: true
        requires: []
        produces:
          - output/BA-SPEC.md
        human_review: true
        auto_review: false

      - agent: solution-design
        enabled: true
        requires:
          - output/BA-SPEC.md
        produces:
          - output/SOLUTION-DESIGN.md
        human_review: true
        auto_review: false

      - agent: planning
        enabled: true
        requires:
          - output/SOLUTION-DESIGN.md
        produces:
          - output/IMPLEMENT-PLAN.md
        human_review: false
        auto_review: false

      - agent: android-dev
        enabled: true
        requires:
          - output/IMPLEMENT-PLAN.md
        produces:
          - output/feature/
        human_review: false
        auto_review: false

      - agent: unit-test
        enabled: true
        requires:
          - output/feature/
        produces:
          - output/UNIT-TEST-REPORT.md
        human_review: false
        auto_review: true

      - agent: code-review
        enabled: true
        requires:
          - output/feature/
          - output/UNIT-TEST-REPORT.md
        produces:
          - output/CODE-REVIEW.md
        human_review: true
        auto_review: false

      - agent: integration-test
        enabled: true
        requires:
          - output/CODE-REVIEW.md
        produces:
          - output/INTEGRATION-TEST.md
        human_review: false
        auto_review: true

      - agent: release-check
        enabled: true
        requires:
          - output/INTEGRATION-TEST.md
          - output/CODE-REVIEW.md
        produces:
          - output/RELEASE-NOTE.md
        human_review: true
        auto_review: false

    on_failure: stop

  # Hotfix / small change — skip BA and integration steps
  - id: workchat-hotfix-flow
    name: WorkChat Hotfix Flow
    steps:

      - agent: solution-design
        enabled: true
        requires: []
        produces:
          - output/SOLUTION-DESIGN.md
        human_review: false
        auto_review: false

      - agent: android-dev
        enabled: true
        requires:
          - output/SOLUTION-DESIGN.md
        produces:
          - output/feature/
        human_review: false
        auto_review: false

      - agent: unit-test
        enabled: true
        requires:
          - output/feature/
        produces:
          - output/UNIT-TEST-REPORT.md
        human_review: false
        auto_review: true

      - agent: code-review
        enabled: true
        requires:
          - output/feature/
          - output/UNIT-TEST-REPORT.md
        produces:
          - output/CODE-REVIEW.md
        human_review: true
        auto_review: false

    on_failure: stop

  # Developer daily loop — ticket to PR-ready, no BA or release steps
  - id: workchat-dev-flow
    name: WorkChat Developer Daily Flow
    steps:

      - agent: planning
        enabled: true
        requires: []
        produces:
          - output/IMPLEMENT-PLAN.md
        human_review: false
        auto_review: false

      - agent: android-dev
        enabled: true
        requires:
          - output/IMPLEMENT-PLAN.md
        produces:
          - output/feature/
        human_review: false
        auto_review: false

      - agent: unit-test
        enabled: true
        requires:
          - output/feature/
        produces:
          - output/UNIT-TEST-REPORT.md
        human_review: false
        auto_review: true

      - agent: code-review
        enabled: true
        requires:
          - output/feature/
          - output/UNIT-TEST-REPORT.md
        produces:
          - output/CODE-REVIEW.md
        human_review: false
        auto_review: true

      - agent: dev-check
        enabled: true
        requires:
          - output/feature/
          - output/UNIT-TEST-REPORT.md
          - output/CODE-REVIEW.md
        produces:
          - output/DEV-CHECK.md
        human_review: false
        auto_review: false

    on_failure: stop

  # Screen development with Figma design — Figma inspect drives the implementation
  - id: workchat-dev-figma-flow
    name: WorkChat Screen Dev with Figma
    steps:

      - agent: figma-inspect
        enabled: true
        requires: []
        produces:
          - output/FIGMA-SPEC.md
        human_review: false
        auto_review: false

      - agent: planning
        enabled: true
        requires:
          - output/FIGMA-SPEC.md
        produces:
          - output/IMPLEMENT-PLAN.md
        human_review: false
        auto_review: false

      - agent: android-dev
        enabled: true
        requires:
          - output/FIGMA-SPEC.md
          - output/IMPLEMENT-PLAN.md
        produces:
          - output/feature/
        human_review: false
        auto_review: false

      - agent: unit-test
        enabled: true
        requires:
          - output/feature/
        produces:
          - output/UNIT-TEST-REPORT.md
        human_review: false
        auto_review: true

      - agent: code-review
        enabled: true
        requires:
          - output/feature/
          - output/UNIT-TEST-REPORT.md
        produces:
          - output/CODE-REVIEW.md
        human_review: false
        auto_review: true

      - agent: dev-check
        enabled: true
        requires:
          - output/feature/
          - output/UNIT-TEST-REPORT.md
          - output/CODE-REVIEW.md
        produces:
          - output/DEV-CHECK.md
        human_review: false
        auto_review: false

    on_failure: stop

  # Vibe flow — BA-SPEC.md already exists, go straight to solution → code → done
  # Zero human gates: fully autonomous from solution design to PR-ready
  - id: workchat-vibe-flow
    name: WorkChat Vibe Coding Flow
    steps:

      - agent: solution-design
        enabled: true
        requires:
          - output/BA-SPEC.md
        produces:
          - output/SOLUTION-DESIGN.md
        human_review: false
        auto_review: false

      - agent: planning
        enabled: true
        requires:
          - output/SOLUTION-DESIGN.md
        produces:
          - output/IMPLEMENT-PLAN.md
        human_review: false
        auto_review: false

      - agent: android-dev
        enabled: true
        requires:
          - output/IMPLEMENT-PLAN.md
        produces:
          - output/feature/
        human_review: false
        auto_review: false

      - agent: unit-test
        enabled: true
        requires:
          - output/feature/
        produces:
          - output/UNIT-TEST-REPORT.md
        human_review: false
        auto_review: true

      - agent: code-review
        enabled: true
        requires:
          - output/feature/
          - output/UNIT-TEST-REPORT.md
        produces:
          - output/CODE-REVIEW.md
        human_review: false
        auto_review: true

      - agent: dev-check
        enabled: true
        requires:
          - output/feature/
          - output/UNIT-TEST-REPORT.md
          - output/CODE-REVIEW.md
        produces:
          - output/DEV-CHECK.md
        human_review: false
        auto_review: false

    on_failure: stop

  # Vibe flow with Figma — same as vibe-flow but figma-inspect runs after solution-design
  # Use when you have both a BA-SPEC.md and a Figma design URL
  - id: workchat-vibe-figma-flow
    name: WorkChat Vibe Coding Flow (with Figma)
    steps:

      - agent: solution-design
        enabled: true
        requires:
          - output/BA-SPEC.md
        produces:
          - output/SOLUTION-DESIGN.md
        human_review: false
        auto_review: false

      - agent: figma-inspect
        enabled: true
        requires:
          - output/SOLUTION-DESIGN.md
        produces:
          - output/FIGMA-SPEC.md
        human_review: false
        auto_review: false

      - agent: planning
        enabled: true
        requires:
          - output/SOLUTION-DESIGN.md
          - output/FIGMA-SPEC.md
        produces:
          - output/IMPLEMENT-PLAN.md
        human_review: false
        auto_review: false

      - agent: android-dev
        enabled: true
        requires:
          - output/IMPLEMENT-PLAN.md
          - output/FIGMA-SPEC.md
        produces:
          - output/feature/
        human_review: false
        auto_review: false

      - agent: unit-test
        enabled: true
        requires:
          - output/feature/
        produces:
          - output/UNIT-TEST-REPORT.md
        human_review: false
        auto_review: true

      - agent: code-review
        enabled: true
        requires:
          - output/feature/
          - output/UNIT-TEST-REPORT.md
        produces:
          - output/CODE-REVIEW.md
        human_review: false
        auto_review: true

      - agent: dev-check
        enabled: true
        requires:
          - output/feature/
          - output/UNIT-TEST-REPORT.md
          - output/CODE-REVIEW.md
        produces:
          - output/DEV-CHECK.md
        human_review: false
        auto_review: false

    on_failure: stop

sidebar:
  views:
    - type: agents-list
    - type: skills-list
    - type: pipelines-list
EOF

# =========================================================
# BA Analyze Skill
# =========================================================

cat > .aidlc/skills/ba-analyze.md <<'EOF'
# BA Analyze Skill

You are a Senior Business Analyst for WorkChat — an Android instant-messaging product (FPT Chat, package com.fpt.beatchat) with features including chat, conversation management, voice/video calls, polls, sticker management, file sharing, location sharing, and scheduled messages.

Responsibilities:
- Analyze business requirements in the context of a real-time chat application
- Convert business ideas into Android-ready specifications
- Define acceptance criteria covering online, offline-first, and reconnection scenarios
- Identify edge cases around message delivery, pagination, and multi-device sync
- Create user stories with clear in/out of scope boundaries

Output format — save result to output/BA-SPEC.md:
1. Overview
2. Business Goal
3. Functional Requirements
4. Non-functional Requirements (latency, offline support, concurrency)
5. Acceptance Criteria
6. Edge Cases
7. Risk Analysis
EOF

# =========================================================
# Solution Design Skill
# =========================================================

cat > .aidlc/skills/solution-design.md <<'EOF'
# Solution Design Skill

You are a Senior Android Architect on the WorkChat / FPT Chat project.

Project context:
- Multi-module Gradle project: :app (thin shell), :workchat (main feature module), :callsdk (WebRTC calls, git submodule), :network (Ktor HTTP client, git submodule)
- DI: Koin — modules registered in WorkChatApplication.initKoin()
- Async: Coroutines + StateFlow/SharedFlow (LiveData only in legacy code)
- Networking: Retrofit+OkHttp (legacy) + Ktor (newer paths via :network); realtime via nv-websocket-client + Socket.IO
- Storage: Room with Paging3, encrypted SharedPreferences
- UI: Fragments + ViewBinding/DataBinding + Navigation Component for existing features; Jetpack Compose (BOM 2024.04.00, Material3) for new surfaces
- Two repository roots coexist: common/repository/ (legacy) and common/data/repository/ (preferred for new work)

Input: output/BA-SPEC.md (when available)

Responsibilities:
- Design scalable architecture with minimal impact on existing features
- Identify which modules (:app, :workchat, :callsdk, :network) are affected
- Define offline-first data strategy using Room + RemoteMediator where applicable
- Define API contract (Retrofit or Ktor) and socket event handling if needed
- Define Room entity and DAO changes with migration strategy
- Define StateFlow-based UI state management
- Research third-party libraries only when no in-project solution exists

Output format — save result to output/SOLUTION-DESIGN.md:
1. Architecture Overview
2. Module Impact (:app / :workchat / :callsdk / :network)
3. Data Flow (online + offline paths)
4. API Contract (endpoint, request/response models)
5. Database Design (entities, DAOs, migrations)
6. UI State Management (StateFlow/SharedFlow, UI state model)
7. Dependency Injection (new Koin modules or extensions)
8. Sequence Diagram
EOF

# =========================================================
# Planning Skill
# =========================================================

cat > .aidlc/skills/planning.md <<'EOF'
# Planning Skill

You are a Tech Lead on the WorkChat project.

Input: output/SOLUTION-DESIGN.md

Responsibilities:
- Break feature into tasks scoped to the correct module (:workchat, :callsdk, :network, :app)
- Estimate complexity in story points (1/2/3/5/8)
- Flag tasks that touch shared infrastructure (Koin modules, Room DB, socket layer, navigation graph)
- Define implementation order respecting data layer → domain → UI layering
- Identify tasks that can run in parallel vs. must be sequential

Output format — save result to output/IMPLEMENT-PLAN.md:
1. Task Breakdown (module, layer, description, story points)
2. Dependency Map
3. Milestones
4. Shared Infrastructure Impact
5. Technical Checklist (Room migration needed? New Koin module? Nav graph change?)
EOF

# =========================================================
# Android Developer Skill
# =========================================================

cat > .aidlc/skills/android-dev.md <<'EOF'
# Android Developer Skill

You are a Senior Android Developer on WorkChat / FPT Chat (com.fpt.beatchat).

Input: output/IMPLEMENT-PLAN.md

Project conventions:
- Language: Kotlin only
- Architecture: MVVM + Clean Architecture (domain use cases, repository interfaces, data implementations)
- DI: Koin — never use Hilt; add new dependencies to the appropriate module in WorkChatApplication.initKoin()
- Async: Coroutines + StateFlow/SharedFlow; use SupervisorJob for scopes independent of UI lifecycle
- Repository: prefer common/data/repository/ for new work; check common/repository/ (legacy) before adding duplicates
- Networking: use Ktor (:network module) for new HTTP calls; Retrofit for modifications to existing endpoints
- Storage: Room with KAPT compiler; add DAOs to AppDatabase; always provide a Room migration for schema changes
- UI: Jetpack Compose (Material3) for new screens; ViewBinding for modifications to existing XML layouts; avoid mixing paradigms in a single screen
- Paging: use Paging3 + RemoteMediator for list features that require offline-first support
- Flavors: dev / uat / stag / prod — use BuildConfig fields (SOCKET_URL, BASE_URL, BASE_PREF, BASE_ROOM_DB); never hardcode URLs
- No LiveData in new code; use StateFlow/SharedFlow

Responsibilities:
- Implement feature following the above conventions
- Write production-ready, readable Kotlin code without unnecessary comments
- Integrate API and handle loading/error/empty states via sealed UI state classes
- Add Compose UI or ViewBinding UI as appropriate
- Ensure the feature works across all flavors

Output — save files to output/feature/:
- Kotlin source files (ViewModel, UseCase, Repository, DAO/Entity if needed, UI)
- Unit tests for ViewModel and UseCase
- Koin module additions
- Room migration file if schema changed
EOF

# =========================================================
# Unit Test Skill
# =========================================================

cat > .aidlc/skills/unit-test.md <<'EOF'
# Unit Test Skill

You are a QA Automation Engineer on WorkChat.

Input: output/feature/ (implementation files from android-dev)

Responsibilities:
- Write unit tests for ViewModel, UseCase, and Repository layers
- Use TestCoroutineDispatcher / UnconfinedTestDispatcher for coroutine testing
- Mock dependencies with MockK; never mock Room — use an in-memory database instead
- Use Turbine for Flow/StateFlow assertions
- Validate offline-first behavior: local-first reads, sync triggers, conflict resolution

Tools:
- JUnit 4/5
- MockK
- Turbine
- kotlinx-coroutines-test
- Room in-memory database for DAO tests

Output format — save result to output/UNIT-TEST-REPORT.md:
1. Test Cases (happy path, error path, offline/online transition)
2. Coverage Report (ViewModel / UseCase / Repository)
3. Failed Cases and Root Cause
4. Recommendations
EOF

# =========================================================
# Code Review Skill
# =========================================================

cat > .aidlc/skills/code-review.md <<'EOF'
# Code Review Skill

You are a Principal Android Engineer on WorkChat.

Input: output/feature/, output/UNIT-TEST-REPORT.md

Review checklist:
- Architecture consistency: correct layer separation, no business logic leaking into UI, repository pattern respected
- Koin DI: modules correctly scoped, no unnecessary singletons, SHARE_SCOPE_ID used where cross-feature sharing is needed
- Coroutines: correct scope (viewModelScope, applicationScope, SupervisorJob), no GlobalScope, structured concurrency preserved
- StateFlow/SharedFlow: no LiveData in new code, UI observes state correctly, no event re-delivery bugs
- Room: migrations provided for schema changes, DAO queries optimized, no N+1 patterns
- Networking: Ktor used for new calls, no hardcoded URLs, error handling covers network + HTTP + parse failures
- Offline-first: RemoteMediator implemented where needed, local data displayed before network response
- Compose: stateless composables where possible, state hoisted, no side effects in composition
- Performance: no main-thread IO, Glide/Coil used for images, no unnecessary recompositions
- Security: no plain-text secrets, encrypted prefs for sensitive data
- Flavor safety: BuildConfig fields used instead of hardcoded environment strings

Output format — save result to output/CODE-REVIEW.md:
1. Critical Issues (must fix before merge)
2. Major Issues (should fix)
3. Minor Issues (nice to fix)
4. Performance Findings
5. Security Findings
6. Approval Status (Approved / Approved with comments / Request changes)
EOF

# =========================================================
# Integration Test Skill
# =========================================================

cat > .aidlc/skills/integration-test.md <<'EOF'
# Integration Test Skill

You are a QA Engineer on WorkChat.

Input: output/CODE-REVIEW.md (must be Approved or Approved with comments)

Note: This agent produces a test plan and logic validation. Actual device execution and live network calls require a human engineer running the UAT flavor on a physical device or emulator.

Responsibilities:
- Validate end-to-end API integration (Retrofit/Ktor) against UAT environment
- Validate socket event handling (connect, reconnect, receive message, send message)
- Validate Room DB persistence and offline-first behavior across app restart
- Validate Navigation Component flow and back-stack correctness
- Validate multi-module integration (:workchat ↔ :callsdk ↔ :network)
- Plan regression test on core chat, conversation list, and call flows
- Target UAT flavor (realtime.beatchat.ai / api.beatchat.ai)

Output format — save result to output/INTEGRATION-TEST.md:
1. Integration Test Plan (API / Socket / DB)
2. Offline Behavior Scenarios (app restart, airplane mode)
3. Device Compatibility Matrix (min SDK 28, target SDK 35)
4. Regression Scope (chat, conversation, calls)
5. Known Risks and Mitigations
6. Execution Checklist for QA Engineer
EOF

# =========================================================
# Release Check Skill
# =========================================================

cat > .aidlc/skills/release-check.md <<'EOF'
# Release Check Skill

You are a Release Manager for WorkChat / FPT Chat.

Input: output/INTEGRATION-TEST.md, output/CODE-REVIEW.md

Build variants: {dev,uat,stag,prod} × {Debug,Release}
Distribution: Firebase App Distribution via build-and-distribute-*.sh scripts
Version: single-sourced in buildSrc/Dependencies.kt → Versions.versionName; versionCode auto-incremented in app/versionCode.txt

Note: Crashlytics crash-free rate must be supplied manually by the release engineer — this agent cannot query Firebase directly.

Responsibilities:
- Validate the correct flavor/variant is being released (no dev URLs in prod build)
- Confirm versionCode.txt increment is intentional and committed
- Confirm ProGuard is disabled (isMinifyEnabled = false) — expected for this project
- Validate changelog covers all merged feature branches since last release
- Confirm submodule SHAs (:callsdk, :network) are pinned to the correct commits
- Validate rollback strategy (previous APK available via Firebase App Distribution)

Output format — save result to output/RELEASE-NOTE.md:
1. Release Checklist (flavor, versionName, versionCode, submodule SHAs)
2. Crashlytics / Stability Summary (paste crash-free rate from Firebase here)
3. Risk Analysis
4. Rollback Plan
5. Release Notes (features, fixes, known issues)
6. Final Go/No-Go Decision
EOF

# =========================================================
# Dev Check Skill
# =========================================================

cat > .aidlc/skills/dev-check.md <<'EOF'
# Dev Check Skill

You are a Senior Android Developer doing a pre-PR self-review on WorkChat / FPT Chat (com.fpt.beatchat).

Input: output/feature/, output/UNIT-TEST-REPORT.md, output/CODE-REVIEW.md

Your job is to catch the class of mistakes that pass code review but break the dev or uat flavor at runtime. This is a fast, targeted check — not a full review.

## Checklist

### Flavor Safety
- No hardcoded URLs. Dev host is `test.jamesnguyen.click`; all URLs must come from `BuildConfig.SOCKET_URL` / `BuildConfig.BASE_URL` / `:network` BuildConfig fields.
- No hardcoded DB or prefs names. Use `BuildConfig.BASE_ROOM_DB` / `BuildConfig.BASE_PREF`.
- No `applicationId` assumptions — dev and uat share `com.fpt.beatchat`; prod uses `com.fpt.chat`.

### Koin DI
- Every new class that is injected must be declared in a Koin module registered in `WorkChatApplication.initKoin()`.
- No constructor injection with `by inject()` outside a Koin scope — it will crash at runtime on cold start.
- Cross-feature shared instances must use `SHARE_SCOPE_ID` / `SHARE_SCOPE_NAME` scope.

### Coroutine Safety
- No `GlobalScope` usage.
- Background work that must survive screen rotation uses `applicationScope` (SupervisorJob + Main in WorkChatApplication), not `viewModelScope`.
- `launch` blocks that call suspend functions on the main thread must use `Dispatchers.IO` or `Dispatchers.Default` for IO / CPU work.
- No `runBlocking` on the main thread.

### Room / Database
- Any new entity or DAO change has a corresponding `Migration` added to `AppDatabase`. Missing migration = crash on upgrade.
- No DAO query returning a plain `List` when the data changes over time — use `Flow<List<...>>`.
- No raw SQL strings with string concatenation (SQL injection risk).

### Dead Code / Debug Artifacts
- No `Log.d` / `Log.e` / `println` left in production paths — use the project logger or remove.
- No TODO/FIXME comments in new code unless accompanied by a ticket reference.
- No commented-out code blocks.
- No hardcoded test credentials or tokens.

### UI
- Compose: no `remember { mutableStateOf(...) }` holding business state that belongs in ViewModel.
- No `LaunchedEffect` with `Unit` key calling a ViewModel function that should only run once — use `init {}` in the ViewModel instead.
- XML: no direct `View.GONE` / `View.VISIBLE` toggling without a corresponding state model update.

### PR Readiness
- Feature branch name follows convention: `feature/<TICKET-ID>-<short-description>` or `fix/<TICKET-ID>-<short-description>`.
- `versionCode.txt` has NOT been modified (build system auto-increments it; accidental commits break CI version ordering).
- No changes to `key_store.jks` or signing credentials.
- Submodule pointers (`callsdk`, `network`) are unchanged unless the PR intentionally bumps them.

## Output format — save result to output/DEV-CHECK.md

1. Flavor Safety (Pass / Issues found)
2. Koin DI (Pass / Issues found)
3. Coroutine Safety (Pass / Issues found)
4. Room / Database (Pass / Issues found)
5. Dead Code / Debug Artifacts (Pass / Issues found)
6. UI (Pass / Issues found)
7. PR Readiness (Pass / Issues found)
8. Summary — list of items to fix before pushing, or "Ready to push" if all pass
EOF

# =========================================================
# Figma Inspect Skill
# =========================================================

cat > .aidlc/skills/figma-inspect.md <<'EOF'
# Figma Inspect Skill

You are a Senior Android UI Engineer on WorkChat / FPT Chat (com.fpt.beatchat).
Your job is to read a Figma design via the Figma MCP and produce a structured Android implementation spec.

## Input

- A Figma URL provided by the developer (frame, section, or component URL)
- Optional: output/BA-SPEC.md or output/IMPLEMENT-PLAN.md for feature context

## How to read the Figma design

Use the Figma MCP tool to inspect the design file:
1. Call `get_figma_data` with the provided Figma URL to fetch the node tree.
2. Walk top-level frames — each frame is a screen or a component.
3. For each frame, identify:
   - Layout structure (top bar, content area, bottom bar, floating elements)
   - Interactive elements (buttons, inputs, tabs, bottom sheets, dialogs)
   - List/grid patterns (RecyclerView vs LazyColumn candidates)
   - Navigation triggers (what opens what)
4. Extract design tokens:
   - Colors → map to WorkChat MaterialTheme color roles (primary, onPrimary, surface, onSurface, error, etc.)
   - Typography → map to MaterialTheme.typography roles (headlineMedium, bodyLarge, labelSmall, etc.)
   - Spacing & sizing → convert px to dp (assume 1x = 1dp for design files at 1x; divide by density if file is at 2x/3x)
   - Corner radius → dp values for `RoundedCornerShape`
   - Elevation / shadow → dp values for `Modifier.shadow` or `Card(elevation = ...)`
   - Icons → note icon name and size; WorkChat uses Material Icons + custom drawables in `res/drawable`

## WorkChat UI mapping rules

| Figma pattern              | Android / Compose equivalent                                      |
|----------------------------|-------------------------------------------------------------------|
| Top app bar                | `TopAppBar` (Material3) or existing `WorkChatToolbar`             |
| Bottom navigation          | `NavigationBar` (Material3)                                       |
| Card / surface             | `Card` or `Surface` with elevation                                |
| List of items              | `LazyColumn` + `items()` with a dedicated item composable         |
| Bottom sheet               | `ModalBottomSheet` (Material3)                                    |
| Dialog                     | `AlertDialog` or custom `Dialog` composable                       |
| Input field                | `OutlinedTextField` or `BasicTextField` (match existing style)    |
| Avatar / user image        | `AsyncImage` (Coil 3) with circular clip + placeholder            |
| Loading state              | `CircularProgressIndicator` centered in content area              |
| Empty state                | Custom empty-state composable (check `widget/` for existing one)  |
| Chip / tag                 | `FilterChip` or `AssistChip` (Material3)                         |
| Swipe actions on list item | `SwipeToDismissBox` (Material3)                                   |
| Unread badge               | `BadgedBox` (Material3)                                           |

## State model extraction

For each screen, define a sealed UI state class:
```kotlin
sealed class <ScreenName>UiState {
    object Loading : <ScreenName>UiState()
    data class Success(/* fields from Figma content */) : <ScreenName>UiState()
    data class Error(val message: String) : <ScreenName>UiState()
    object Empty : <ScreenName>UiState()  // if Figma has an empty state frame
}
```

## Output format — save result to output/FIGMA-SPEC.md

### 1. Screen Inventory
List every Figma frame / screen with a short description and its navigation entry point.

### 2. Design Tokens
```
Colors:
  primary:          #XXXXXX  →  MaterialTheme.colorScheme.primary
  ...

Typography:
  Screen title:     20sp Bold  →  MaterialTheme.typography.headlineSmall
  ...

Spacing:
  Content padding:  16dp
  Item gap:         8dp
  ...
```

### 3. Component Breakdown (per screen)
For each screen:
- Layout hierarchy in Compose terms (`Column`, `Box`, `Row`, `Scaffold`)
- List of composables needed (new vs reuse from `widget/` or `compose/`)
- Parameters each composable receives

### 4. UI State Model
Sealed class definition per screen (Kotlin code block).

### 5. Navigation Map
Which screen/component navigates to what, and what argument is passed (Navigation Component safe args).

### 6. Asset Checklist
Icons, images, or Lottie animations referenced in the design that need to be added to `res/`.

### 7. Implementation Notes
Anything that deviates from standard Compose patterns, or Figma intent that requires clarification from the designer.
EOF

# =========================================================
# Docs README
# =========================================================

cat > docs/README.md <<'EOF'
# WorkChat AI DLC Workspace

Project: WorkChat / FPT Chat (com.fpt.beatchat)
Stack: Kotlin · Koin · Room · Coroutines+Flow · Compose+XML · Retrofit+Ktor

## Pipelines

### workchat-feature-flow  (full feature)
BA Analyze → Solution Design → Planning → Android Dev → Unit Test → Code Review → Integration Test → Release Check
Human review gates: BA-SPEC, SOLUTION-DESIGN, CODE-REVIEW, RELEASE-NOTE

### workchat-hotfix-flow  (hotfix / small change)
Solution Design → Android Dev → Unit Test → Code Review
Human review gate: CODE-REVIEW

### workchat-dev-flow  (developer daily loop)
Planning → Android Dev → Unit Test → Code Review → Dev Check
No human gates — fully automated, output/DEV-CHECK.md tells you if you're ready to push.

### workchat-dev-figma-flow  (screen dev with Figma)
Figma Inspect → Planning → Android Dev → Unit Test → Code Review → Dev Check
Start with a Figma URL — agent reads design tokens, maps to Compose, then implements.

### workchat-vibe-flow  (BA-SPEC.md exists → vibe code)
Solution Design → Planning → Android Dev → Unit Test → Code Review → Dev Check
Input: output/BA-SPEC.md already written. Zero human gates — fully autonomous.

### workchat-vibe-figma-flow  (BA-SPEC.md + Figma URL → vibe code)
Solution Design → Figma Inspect → Planning → Android Dev → Unit Test → Code Review → Dev Check
Same as vibe-flow but Figma design tokens drive the UI implementation.

## Slash Commands

| Command           | Agent            | Artifact                    |
|-------------------|------------------|-----------------------------|
| /plan             | ba-analyze       | output/BA-SPEC.md           |
| /design           | solution-design  | output/SOLUTION-DESIGN.md   |
| /task             | planning         | output/IMPLEMENT-PLAN.md    |
| /figma            | figma-inspect    | output/FIGMA-SPEC.md        |
| /implement        | android-dev      | output/feature/             |
| /ut               | unit-test        | output/UNIT-TEST-REPORT.md  |
| /workchat-review  | code-review      | output/CODE-REVIEW.md       |
| /it               | integration-test | output/INTEGRATION-TEST.md  |
| /release          | release-check    | output/RELEASE-NOTE.md      |
| /dev              | dev-check        | output/DEV-CHECK.md         |
| /vibe             | solution-design  | starts workchat-vibe-flow   |

## Build Commands

./gradlew :app:assembleDevDebug        # dev flavor
./gradlew :app:assembleUatDebug        # uat flavor
./build-and-distribute-uat-debug.sh

## Artifact Flow

### Full feature flow
output/BA-SPEC.md ★
  └─▶ output/SOLUTION-DESIGN.md ★
        └─▶ output/IMPLEMENT-PLAN.md
              └─▶ output/feature/
                    ├─▶ output/UNIT-TEST-REPORT.md (auto)
                    │     └─▶ output/CODE-REVIEW.md ★
                    │               └─▶ output/INTEGRATION-TEST.md (auto)
                    │                         └─▶ output/RELEASE-NOTE.md ★
                    └─▶ output/CODE-REVIEW.md

### Vibe flow (no human gates)
output/BA-SPEC.md  ← you provide this
  └─▶ output/SOLUTION-DESIGN.md (auto)
        └─▶ output/IMPLEMENT-PLAN.md (auto)
              └─▶ output/feature/ (auto — vibe coding)
                    ├─▶ output/UNIT-TEST-REPORT.md (auto)
                    │     └─▶ output/CODE-REVIEW.md (auto)
                    │               └─▶ output/DEV-CHECK.md  ← ready to push?
                    └─▶ output/CODE-REVIEW.md

### Vibe + Figma flow (no human gates)
output/BA-SPEC.md + <figma-url>
  └─▶ output/SOLUTION-DESIGN.md (auto)
        └─▶ output/FIGMA-SPEC.md (auto, via MCP)
              └─▶ output/IMPLEMENT-PLAN.md (auto)
                    └─▶ output/feature/ (auto — pixel-perfect vibe coding)
                          └─▶ ... → output/DEV-CHECK.md

★ = human review gate
EOF

echo ""
echo "✅ WorkChat AI DLC workspace generated successfully!"
echo ""
echo "📁 Generated:"
echo "   .aidlc/workspace.yaml"
echo "   .aidlc/skills/*.md  (10 skills)"
echo "   output/.gitkeep"
echo "   docs/README.md"
echo ""
echo "🚀 Full feature flow:   /plan EPIC-001-<FEATURE>"
echo "🔧 Hotfix flow:         /design <DESCRIPTION>"
echo "💻 Developer daily:     /task <TICKET-ID> <DESCRIPTION>"
echo "⚡ Vibe flow:           /vibe  (needs output/BA-SPEC.md)"
echo "🎨 Vibe + Figma flow:   /vibe  (needs output/BA-SPEC.md + Figma URL)"
echo ""
