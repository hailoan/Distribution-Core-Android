AUTOMATION: CONTINUE

# IMPLEMENT-PLAN — build-ffmpeg-for-android

Execution-ready plan for **compiling FFmpeg from source for Android with the NDK**, owned by
`:videolib`. Planning only — no production code, no architecture change. Source design:
`SOLUTION-DESIGN.md` (first line `AUTOMATION: CONTINUE`).

Trace chain preserved: `FR-1/FR-2 → SC-1/SC-2 → AC-1..AC-5 → W1/W2 → T1..T5`.

This is an **out-of-band build-tooling** deliverable. It adds FFmpeg cross-compile tooling + a
per-ABI artifact tree under `:videolib`. It does **not** modify `:videolib`'s runtime Kotlin/JNI/CMake
this ticket (integration is a separate, deferred ticket per design D1/scope).

---

## 1. Planning control

- **Source design revision:** `SOLUTION-DESIGN.md` (2026-09-08), guard `AUTOMATION: CONTINUE`.
- **Outcome:** `CONTINUE` — DAG valid, no planning blocker; deferred design defaults carried as task
  invariants + listed as non-blocking unresolved inputs.
- **Skills routed:** planning; module-impact-analysis, risk-analysis (reused); ffmpeg-guideline,
  ndk-cpp-guideline, gradle-module-guideline (reused from design). No delivery-backlog mode (no
  estimates/priority/sprint requested → sprint section omitted). `dependency-analysis` not loaded —
  no existing symbol is modified; the tooling is new and the runtime `:videolib` code is untouched.
- **Investigation ledger:** ~4 bounded lookups. Confirmed: `:videolib` has no existing
  scripts/tooling dir; `videolib/.gitignore` ignores `/build`; runtime scaffold files unchanged;
  `camera` 16 KB/FFmpeg precedent read-only. Two-consecutive-no-gain threshold not reached.
- **Owner-stage note:** `testing` (JVM/Android unit) is **N/A** — no Kotlin/JVM production symbol is
  added or changed. Verification is `integration-testing` (run the cross-compile + inspect output)
  plus source/static inspection, justified per task.

**Assumptions**

| ID | Assumption | Basis |
|---|---|---|
| PA1 | Build runs on a maintainer/CI host with NDK `29.0.14206865` + a POSIX toolchain (autotools). | design D1; repo NDK (packet §8) |
| PA2 | Artifact tree location and "committed vs produced-on-demand" are tooling choices; either satisfies AC-1. | design D7 + "explicitly unspecified"; `videolib/.gitignore` |

**Blockers:** none.

**Unresolved planning inputs** (non-blocking — each has an evidence-backed default set as a task
invariant; android-dev confirms or overrides at start of T1/T2, not mid-coding):

| UPI | Input | Default (invariant) | Owner |
|---|---|---|---|
| UPI-1 | FFmpeg version tag (AC-3) | one maintained stable tag, same for all ABIs (design D2) | user/maintainer |
| UPI-2 | ABI set (D3) | `arm64-v8a`, `armeabi-v7a` | user/maintainer |
| UPI-3 | License posture (AC-5) | LGPL-safe: no `--enable-gpl`/`--enable-nonfree` (design D5) | user/maintainer |
| UPI-4 | Linkage mode (D6) | static `.a` + headers | user/maintainer |

---

## 2. Change-surface inventory

All rows are **new** and confined to `:videolib`. Where SOLUTION-DESIGN left the exact filename
implementation-local (D1), a **bounded planned package + responsibility** is given per the planning
contract; android-dev owns the concrete filename within it.

| Change-ID | existing/new | action | exact path (bounded planned) | symbol/config key | Design-Ref | responsibility | evidence/decision | shared/collision key |
|---|---|---|---|---|---|---|---|---|
| C1 | new | create | `videolib/` FFmpeg build-tooling dir (planned, e.g. `videolib/ffmpeg-build/`) — source pin record | pinned FFmpeg tag | none — requirement/code-driven | Pin one FFmpeg source tag for all ABIs | design D2, AC-3 | `ffmpeg-build-tooling` |
| C2 | new | create | same tooling dir — build orchestrator + per-ABI profiles | `configure`/`make` driver; per-ABI triple, `--target-os=android`, API level, `--enable/--disable`, extra-cflags/ldflags, prefix | none — requirement/code-driven | Drive configure→make→install per ABI with all §5 contracts | design D1,D3–D7, AC-1,AC-4,AC-5 | `ffmpeg-build-tooling` |
| C3 | new | create | same tooling dir — alignment gate | ELF 16 KB check step | none — requirement/code-driven | Assert every produced ELF is 16 KB-aligned before "verified" | design §2/§6, AC-2 | `ffmpeg-build-tooling` |
| C4 | new | produce | `:videolib`-owned per-ABI output dir (planned, mirrors `camera` `ffmpegv2/<abi>/`) | `<abi>/include`, `<abi>/lib` | none — requirement/code-driven | The deliverable artifact tree (libs per D6 + headers) | design D7, AC-1 | `ffmpeg-artifact-tree` |

**Must-not-touch this ticket (runtime `:videolib`, integration deferred):**
`videolib/build.gradle.kts`, `videolib/src/main/cpp/CMakeLists.txt`,
`videolib/src/main/java/com/cii/videolib/NativeLib.kt`, `videolib/src/main/cpp/videolib.cpp`.
Collision key `videolib-runtime` — no task in this plan writes these.
**Read-only reference:** `camera/build.gradle.kts`, `camera/src/main/cpp/CMakeLists.txt`,
`camera/src/main/cpp/ffmpegv2/**` (vendored 3.2.12 — do not modify).

---

## 3. Work-item backlog

| FR-ID | SC-ID | AC-ID | Work-ID | outcome | module | depends on |
|---|---|---|---|---|---|---|
| FR-1, FR-2 | SC-1, SC-2 | AC-1, AC-2, AC-3, AC-4, AC-5 | **W1** | Running the build produces 16 KB-aligned FFmpeg artifacts (libs + headers) per selected ABI, from a pinned source, at an API level honoring `minSdk 21`, with the chosen license posture | `:videolib` | — |
| FR-1, FR-2 | SC-1, SC-2 | AC-1, AC-2 | **W2** | Closure verification: the build actually emits the per-ABI artifact tree and every produced ELF is 16 KB-aligned | `:videolib` | W1 |

One implementation slice (W1) + one native/build closure-verification slice (W2, warranted because
this is a native/build/publication surface per planning skill).

---

## 4. Task backlog

| Task-ID | Work-ID | owning module | owner stage | objective | Change-IDs / scope | Design-Ref | preconditions/inputs | invariants | done condition | verification | depends on | collision key |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| **T1** | W1 | `:videolib` | android-dev | Pin/acquire FFmpeg source at one tag for all ABIs | C1 | none | UPI-1 confirmed-or-default | single tag across all ABIs; no modification of `camera/ffmpegv2` | Pinned FFmpeg source available to the build; tag recorded | source inspection (pin record present, single tag) | — | `ffmpeg-build-tooling` |
| **T2** | W1 | `:videolib` | android-dev | Author build orchestrator + per-ABI build profiles driving `configure→make→install` | C2, produces C4 | none | T1; UPI-2/3/4 confirmed-or-default; PA1 host toolchain | per-ABI isolated toolchain (no cross-ABI bleed); API level ≥ `minSdk 21` per ABI (AC-4/D4); license flags = posture (AC-5/D5); ABI set = UPI-2 (D3); linkage = UPI-4 (D6); 16 KB ldflags `-Wl,-z,max-page-size=16384` applied to every produced ELF (AC-2); output layout `<abi>/{include,lib}` (D7); does not touch `videolib-runtime` files | Build driver + profiles exist; a per-ABI run installs libs+headers under C4 layout | source inspection + T4 (Check-1) | T1 | `ffmpeg-build-tooling` |
| **T3** | W1 | `:videolib` | android-dev | Add 16 KB alignment gate to the build (verifier component) | C3 | none | T2 | fails the build if any produced ELF lacks LOAD align `0x4000`; covers every ABI and every produced lib | Alignment check wired as a build gate | source inspection + T5 (Check-2) | T2 | `ffmpeg-build-tooling` |
| **T4** | W2 | `:videolib` | integration-testing | Execute cross-compile for the selected ABI set; confirm artifact tree | verify C2/C4 | none | T2 (T3 present); PA1 | every selected ABI yields `<abi>/include` + libs (per D6) under C4; no partial tree on failure | Artifacts present for each selected ABI | Check-1 (see §7) | T2, T3 | `ffmpeg-artifact-tree` |
| **T5** | W2 | `:videolib` | integration-testing | Run ELF 16 KB alignment check on every produced ELF | verify C3/C4 | none | T4 | every produced ELF LOAD segment align = `0x4000` | Alignment check passes for all produced ELFs | Check-2 (see §7) | T4 | `ffmpeg-artifact-tree` |

No `testing`-stage tasks: no JVM/Android unit-testable production symbol is introduced (justified,
§1). All verification is integration-testing + source/static inspection.

---

## 5. Dependency map (DAG)

```
T1 ──contract──▶ T2 ──contract──▶ T3
                  │                 │
                  └──test──▶ T4 ◀──wiring── T3
                                     │
                                     └──test──▶ T5
```

Typed edges:
- `T1 → T2` **contract**: orchestrator needs the pinned source (AC-3).
- `T2 → T3` **contract**: alignment gate wraps the build output.
- `T2 → T4` **test**: integration run exercises the orchestrator (AC-1).
- `T3 → T4` **wiring**: T4 runs the build with the gate present.
- `T4 → T5` **test**: alignment check runs on the produced tree (AC-2).

**Cycle check:** none (strict topological order T1→T2→T3→T4→T5).

---

## 6. Execution waves

| Wave | Tasks | Concurrency/serialization | Ownership reason | Prereqs satisfied |
|---|---|---|---|---|
| 1 | T1 | single | source pin prerequisite | — |
| 2 | T2 | single | writes `ffmpeg-build-tooling` (collision key) | T1 |
| 3 | T3 | single | writes `ffmpeg-build-tooling` (same collision key as T2 → serialized) | T2 |
| 4 | T4 | single | integration run; reads `ffmpeg-artifact-tree` | T2, T3 |
| 5 | T5 | single | alignment closure check on `ffmpeg-artifact-tree` | T4 |

Fully serial: every implementation task shares the `ffmpeg-build-tooling` collision key, and the
verification tasks depend on real output. No safe within-wave concurrency.

---

## 7. Test scope and verification matrix

| Test-ID | AC-ID/risk | level | target component/contract | behavior/scope | fixture boundary | production Task-ID | depends on | execution expectation |
|---|---|---|---|---|---|---|---|---|
| — | — | JVM/Android unit | N/A | No unit-testable production symbol added (build tooling only) | — | — | — | authored-only: **N/A**, justified §1 |
| Check-1 | AC-1 / DEV-SPEC §9 (ABI coverage, reproducibility) | integration/build | Build orchestrator → artifact tree (C2/C4) | Cross-compile each selected ABI; libs+headers present in `<abi>/{include,lib}`; failure leaves no partial tree | real NDK host (PA1) | T2 | T2,T3 | device/env-dependent (build host); run in integration-testing, not now |
| Check-2 | AC-2 / DEV-SPEC §9 (16 KB alignment) | static/ELF inspection | Alignment gate (C3) over produced ELFs (C4) | `readelf -l` / `llvm-readelf -l` (or NDK `check_elf_alignment`) LOAD align = `0x4000` on every produced ELF | produced artifact tree | T3 | T4 | runnable static check once artifacts exist; run in integration-testing |

**Module integration matrix**

| Check-ID | changed module | affected consumer/external contract | boundary | command or device/manual check | why required | owner task |
|---|---|---|---|---|---|---|
| Check-1 | `:videolib` | external consumers **unknown** (public module); produced soname/version (AC-3) + license (AC-5) become downstream contract at integration | native/ABI packaging, build-tooling | run the cross-compile for the ABI set; confirm `<abi>/{include,lib}` populated | proves configure/linkage/soname resolution + AC-1 | T4 |
| Check-2 | `:videolib` | 16 KB-page Android devices (downstream loadability) | native/ABI (ELF alignment) | `readelf -l`/`check_elf_alignment` → LOAD align `0x4000` on all produced ELFs | proves AC-2; misalignment crashes consumers on 16 KB devices | T5 |

Proof-level note (ffmpeg/ndk guidelines): Check-1/Check-2 prove linkage, packaging, and alignment
only. Actual runtime FFmpeg behavior on a device is **out of scope** (deferred integration ticket) —
never claimed from build/static inspection.

---

## 8. Shared infrastructure and risk constraints

| Risk (design §9 / packet) | affected IDs | required serialization/verification | owning task |
|---|---|---|---|
| Misaligned ELF → crash on 16 KB devices | AC-2, C3, T3, T5 | Alignment gate (T3) + independent closure check (T5) | T3, T5 |
| Wrong/incomplete ABI coverage | D3/UPI-2, T2, T4 | ABI set fixed as T2 invariant; T4 confirms each selected ABI | T2, T4 |
| FFmpeg license (GPL/LGPL) on public module | AC-5/UPI-3, T2 | License posture as T2 invariant; publication remains separately authorized | T2 |
| Build API level vs `minSdk 21` | AC-4, T2 | API-level ≥ `minSdk 21` per ABI as T2 invariant | T2 |
| Accidental edit to `videolib-runtime` or `camera/ffmpegv2` | collision keys | `ffmpeg-build-tooling`/`ffmpeg-artifact-tree` isolated from `videolib-runtime`; `camera` read-only | T1–T5 |
| Reproducibility (out-of-band build) | AC-1, T2, T4 | Pinned source (T1) + deterministic per-ABI profiles (T2); T4 re-run confirms | T1, T2, T4 |

**Separately-authorized (not in this plan):** publishing/signing/upload/distribution of the produced
artifacts, and any GPL/nonfree licensing election, per project ground rules.

---

Every AC maps to a task and a verification (AC-1→T2/Check-1; AC-2→T3/Check-2; AC-3→T1; AC-4→T2;
AC-5→T2). DAG is acyclic, ownership is bounded, no planning blocker remains → `AUTOMATION: CONTINUE`.
