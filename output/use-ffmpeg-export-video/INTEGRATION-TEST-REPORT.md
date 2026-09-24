AUTOMATION: CONTINUE

# INTEGRATION-TEST-REPORT — use-ffmpeg-export-video

Dependency-closure verification for the export feature. Verdict: **PASS WITH LIMITATIONS** — every
executable required check passes; only the device-only runtime check is unexecuted (non-blocking for
this stage, required before release).

> **Re-verification note.** An earlier run of this stage returned **FAIL**: the export error taxonomy
> had been implemented by extending the shared `PlaybackError` enum (`+ENCODE/MUX/OUTPUT`), which
> broke the `:app` consumer's exhaustive `when` at `MainActivity2.kt:398` (`:app:assembleDebug`
> compile error). The compatibility-preserving fix was applied (android-dev): `PlaybackError` was
> reverted to its original four values and a **separate `ExportError`** enum now carries the seven
> export categories (`ExportListener`/`VideoExporter` use it; the native `ExportErrorCode` 1..7 wire
> contract is unchanged). All checks below were re-run after that fix.

## 1. Scope reconciliation

| Item | Finding |
|---|---|
| Changed modules (actual diff) | `videolib` only — `src/main/cpp/**` (6 new units + CMake + JNI), `src/main/java/com/cii/videolib/**` (VideoExporter, ExportListener, **ExportError**, PlaybackError reverted), plus test sources. No other module touched (FR-5 preserved by the fix — the alternative of editing `:app` was avoided). |
| Primary owner | `videolib` (registry + source agree). |
| Registry vs source edges | Registry edge `videolib → app` validated in source: `app/build.gradle.kts:55` `implementation(project(":videolib"))`; `settings.gradle.kts:79` `include(":videolib")`. No other in-repo module depends on `:videolib`. |
| Affected consumers (in-repo) | **`:app`** — consumes `PlaybackError` at `MainActivity2.kt`. After the fix `PlaybackError` is unchanged, so the consumer is unaffected (re-verified below). `:app` does not reference the new `ExportError`/`VideoExporter` types. |
| External consumers | `:videolib` is "public; external consumers unknown". The fix keeps `PlaybackError` binary/source compatible, so no external consumer breaks; `ExportError`/`VideoExporter` are purely additive new types. |
| Unexpected scope | None. |

## 2. Contract matrix

| Contract-ID | boundary / type | producer | consumer(s) | compatibility obligation | evidence required | status |
|---|---|---|---|---|---|---|
| C-1 | JNI / native / ABI / packaging | `videolib` (`libvideolib.so`) | Kotlin `VideoExporter`, any host | New `mediandk` link + export JNI symbols resolve; ARM ABIs + 16 KB alignment; existing preview JNI unchanged | AAR packaging + `.so` symbol/NEEDED/align inspection | ✅ PASS (Check-1) |
| C-2 | Public Kotlin API — `PlaybackError` | `videolib` | `:app` + external (unknown) | **Unchanged** — no consumer break | `:app:assembleDebug` + source inspection | ✅ PASS (Check-2) — reverted to original 4 values |
| C-3 | Public Kotlin API — new `VideoExporter`, `ExportListener`, `ExportError` | `videolib` | external (unknown); `:app` doesn't use them | Purely additive new types | producer compile + JVM enum test + source inspection | ✅ PASS (additive) |
| C-4 | Native runtime (encode/mux/GL/audio/timeline) | `videolib` | device | Behavior correct on a supported device | supported-device instrumented run | ⏳ NOT EXECUTED (no device) — non-blocking here |

## 3. Verification matrix

| Check-ID | module/consumer | Contract/Test/risk | exact command / check | environment | executed | result | evidence |
|---|---|---|---|---|---|---|---|
| Check-1 | videolib | C-1 / Risk-3 | `./gradlew :videolib:assembleDebug` + AAR/`.so` inspection | JBR 21, NDK 29, host | ✅ yes | **PASS** | BUILD SUCCESSFUL; §4 native evidence |
| Check-2 | app (consumer) | C-2 / Risk-5 | `./gradlew :app:assembleDebug` | JBR 21, host | ✅ yes | **PASS** | BUILD SUCCESSFUL (was FAIL before the `ExportError` fix) |
| Check-UT | videolib | C-3 enum contract | `./gradlew :videolib:testDebugUnitTest` | host JVM | ✅ yes | **PASS** — 13 tests (ExportErrorTest 4, VideoSegmentTest 8, ExampleUnitTest 1), 0 failures | test-results XMLs |
| Check-AT | videolib | test compile | `./gradlew :videolib:compileDebugAndroidTestKotlin` | JBR 21 + NDK 29 | ✅ yes | **PASS** — instrumented suite compiles against `ExportError` | task log |
| Check-3 | videolib | C-4 / AC-1..9 | `./gradlew :videolib:connectedDebugAndroidTest` | supported ARM device | ❌ no | not executed | no device available (UNIT-TEST-REPORT §4) |

## 4. Native / build / package checks (Check-1 detail — triggered: JNI/native/ABI/build)

Extracted `jni/arm64-v8a/libvideolib.so` from `videolib-debug.aar`, inspected with NDK 29 tools:

- **Packaging:** `.so` present for **both** ABIs — `jni/arm64-v8a/` (~22.5 MB) and `jni/armeabi-v7a/` (~20.0 MB).
- **New link (C-1):** `readelf -d` → `NEEDED libmediandk.so` alongside `libandroid`, `liblog`,
  `libEGL`, `libGLESv3`, `libz`, `libm`, `libdl`, `libc` — MediaCodec link resolved.
- **Export JNI symbols exported (dynamic `T`):** `Java_com_cii_videolib_VideoExporter_nativeCreate`,
  `_nativeStartExport`, `_nativeCancelExport`, `_nativeDestroy` — match the Kotlin `external fun`s.
- **MediaCodec symbols undefined (`U`):** `AMediaCodec_createEncoderByType`, `AMediaFormat_getBuffer`,
  etc. resolve from `mediandk` at load.
- **16 KB alignment:** every `LOAD` segment `Align: 0x4000` — preserved.
- **No regression to preview JNI:** existing `VideoPreview_native*` symbols untouched.

The error-taxonomy fix is a Kotlin-only change (`ExportError` enum + mapping); the native
`ExportErrorCode` (1..7) wire contract is unchanged, so no native rebuild semantics changed — Check-1
remains green after the fix.

ABI/device limitation: symbol/packaging inspection proves linkage, ABI, and alignment; it does not
prove encode/mux/GL/audio runtime behavior — that is Check-3 (device).

## 5. Gaps

| Gap | impact | owner | smallest follow-up | blocks review/release? |
|---|---|---|---|---|
| Native runtime behavior (Check-3) unexecuted — no device. | Encode/mux/GL/audio-sync/timeline unproven at runtime. | integration-testing (on device) | `./gradlew :videolib:connectedDebugAndroidTest` on a supported arm64/armeabi device. | **Non-blocking for this stage.** Required before release sign-off; the authored instrumented suite is ready to run. |
| External `:videolib` consumers cannot be inspected. | With the `ExportError` fix, `PlaybackError` is unchanged and the new types are additive, so no known break. | product/release owner | Note the additive public API (`VideoExporter`/`ExportListener`/`ExportError`) in release notes. | Advisory. |

## 6. Integration verdict

**PASS WITH LIMITATIONS** — the previously-failing consumer contract (C-2) is fixed and re-verified
(`:app:assembleDebug` green via the separate `ExportError` type), and all native/build/packaging and
enum-contract checks pass; the only unexecuted check is the device-only runtime suite (Check-3),
which is explicitly non-blocking for this stage and required before release.

Per the guarded contract, all required executable checks passed, so this report hands off to review
with `AUTOMATION: CONTINUE`. No production/test source was edited by this stage; the `ExportError`
fix was applied by the re-opened android-dev step (see CHANGESET addendum). No
publish/sign/upload/commit/push performed.
