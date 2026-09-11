AUTOMATION: CONTINUE

# CHANGESET — use-ffmpeg-export-video

Implementation of FFmpeg + MediaCodec video export in `:videolib`, per `IMPLEMENT-PLAN.md` /
`SOLUTION-DESIGN.md`. All 12 android-dev tasks (T1–T12) complete. Change scope: `:videolib` only.

> **Addendum (post-integration fix — error taxonomy compatibility).** Integration testing found that
> the original C3 (extending the shared `PlaybackError` enum with `ENCODE`/`MUX`/`OUTPUT`) broke the
> `:app` consumer's exhaustive `when` (`:app:assembleDebug` compile error — the plan's Risk-5). Per
> the SOLUTION-DESIGN Risk-5 alternative, the fix keeps `PlaybackError` unchanged and introduces a
> **separate `ExportError`** enum for the seven export categories:
> - `PlaybackError.kt` — reverted to the original 4 playback values (C3 superseded).
> - `ExportError.kt` — **new** public enum (INPUT_OPEN, UNSUPPORTED_VIDEO, DECODE, RENDER, ENCODE,
>   MUX, OUTPUT); the native `ExportErrorCode` 1..7 wire contract is unchanged.
> - `ExportListener.onExportError` and `VideoExporter`'s native-code mapping now use `ExportError`.
> - `video_export.h` comments updated to reference `ExportError`; no native code/wire change.
> Re-verified: `:videolib:assembleDebug` PASS, **`:app:assembleDebug` PASS** (was FAIL),
> `:videolib:testDebugUnitTest` PASS (13), instrumented compile PASS. This addendum keeps the change
> `:videolib`-only (FR-5) — the alternative of editing `:app`'s `when` was deliberately not taken.

## 1. Implementation outcome

- **Status:** completed — T1–T12 (waves 0–8) implemented.
- **Deviations:** none material. One implementation-local sequencing decision recorded below
  (header-write timing in `VideoExport`); it stays within SOLUTION-DESIGN D-5 and the plan's task
  scope. Testing tasks (T-VAL, T-DEV, T-IT) are handed off, not executed here.
- **Authorized verification run:** `:videolib:compileDebugKotlin` + `:videolib:externalNativeBuildDebug`
  (both ABIs) — **BUILD SUCCESSFUL**, 0 warnings. See §4.

## 2. Actual change manifest

| FR | SC | AC | Work | Task | Change-ID | module | consumers/contracts | Design-Ref | planned action | actual path | symbol | diff | purpose | Test/Check | verification |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| FR-6 | SC-3 | AC-5 | W1 | T1 | C11 | videolib | build/packaging | none | extend | `videolib/src/main/cpp/CMakeLists.txt` | source list + `mediandk` link | done | Register 6 new units; link MediaCodec | Check-1 | native link OK (§4) |
| FR-1 | — | AC-7 | W1 | T2 | C3 | videolib | public enum (K-PLAYBACKERROR) | none | extend | `.../java/com/cii/videolib/PlaybackError.kt` | `PlaybackError` +`ENCODE`,`MUX`,`OUTPUT` | done | Export error taxonomy | T-VAL | kotlin compile OK |
| FR-1 | — | AC-7 | W1 | T2 | C9(enum) | videolib | native | none | create | `.../cpp/video_export.h` | `ExportErrorCode` | done | Native codes distinct from `PlaybackErrorCode` | — | compile OK |
| FR-2 | SC-2 | AC-2 | W1/W2 | T3 | C4 | videolib | native | none | create | `.../cpp/frame_source.{h,cpp}` | `FrameSource` | done | Decode→RGBA, PTS passthrough (no wall-clock) | T-EXPORT-VIDEO | compile OK; runtime device-only |
| FR-3,4 | SC-2 | AC-3,4 | W1/W2 | T4 | C5 | videolib | native | none | create | `.../cpp/offscreen_renderer.{h,cpp}` | `OffscreenRenderer` | done | Offscreen EGL pbuffer + reuse `GlProgram` + readback | T-EXPORT-VIDEO | compile OK; runtime device-only |
| FR-6 | SC-3 | AC-5 | W1 | T5 | C6 | videolib | native → mediandk | none | create | `.../cpp/h264_encoder.{h,cpp}` | `H264Encoder` | done | MediaCodec byte-buffer H.264; RGBA→NV12; SPS/PPS capture | T-EXPORT-VIDEO / Risk-1,3 | compile OK; runtime device-only |
| FR-6 | SC-3 | AC-5 | W1 | T6 | C7 | videolib | native → FFmpeg | none | create | `.../cpp/mp4_muxer.{h,cpp}` | `Mp4Muxer` | done | avformat MP4; avcC extradata; AnnexB→AVCC; delete-on-fail | T-EXPORT-VIDEO / Risk-1 | compile OK; runtime device-only |
| FR-1..4 | SC-1,2 | AC-1,2,3,4,7,8 | W1/W2 | T7 | C9 | videolib | native | none | create | `.../cpp/video_export.{h,cpp}` | `VideoExport` | done | Orchestrator; state machine; cancel/stop-before-release; 1 terminal event | T-EXPORT-VIDEO,T-CANCEL,T-ERR / Risk-4 | compile OK; runtime device-only |
| FR-1 | SC-1 | AC-1 | W1 | T8 | C10 | videolib | JNI (N-JNI) | none | extend | `.../cpp/videolib.cpp` | `ExportJniBridge`, `Java_..._VideoExporter_native*` | done | Hand-mangled JNI + terminal callback bridge (shared `gJvm`) | T-LINK | JNI symbols resolve (compile+link) |
| FR-1..6 | SC-1..3 | AC-1,5,7 | W1 | T9 | C1,C2 | videolib | public Kotlin API | none | create | `.../java/.../VideoExporter.kt`, `ExportListener.kt` | `VideoExporter`, `ExportListener` | done | Public facade; validation; main-thread callback; code→`PlaybackError` | T-VAL,T-EXPORT-VIDEO | kotlin compile OK; `external fun`↔JNI verified by link |
| FR-7 | SC-4 | AC-6 | W3 | T10 | C8 | videolib | native → FFmpeg | none | create | `.../cpp/audio_transcoder.{h,cpp}` | `AudioTranscoder` | done | Decode→atempo(speed)→AAC; no-audio→video-only (A-2) | T-AUDIO / Risk-2 | compile OK; runtime device-only |
| FR-7 | SC-4 | AC-6 | W3 | T11 | C7,C9(ext) | videolib | native | none | wire | `.../cpp/mp4_muxer.*`, `video_export.cpp` | `addAudioStream`/`writeAudioPacket`; audio in session | done | AAC stream + PTS-interleave; video-only fallback | T-AUDIO | compile OK; runtime device-only |
| FR-1 | SC-2 | AC-9 | W4 | T12 | C9,C10,C1(ext) | videolib | native+JNI+Kotlin | none | extend | `video_export.*`, `videolib.cpp`, `VideoExporter.kt` | segment vector + `nativeStartExport` + `exportTimeline` | done | Ordered concat; continuous PTS; fail-fast + segment index | T-TIMELINE | compile OK; runtime device-only |

Every planned Change-ID (C1–C11) is completed. No production path outside the approved Change-IDs
was modified (`git status` confirms only `videolib/**` + ticket `output/**`).

## 3. Task completion

| Task | Preconditions | Invariants checked | Done condition | Result | Evidence |
|---|---|---|---|---|---|
| T1 | — | ARM ABIs, 16 KB align, `-Bsymbolic`, C++17, one `.so`, FFmpeg IMPORTED untouched | native links | ✅ | §4 build |
| T2 | T1 | additive enum; native codes ≠ `PlaybackErrorCode`; `video_playback.h` untouched | codes declared | ✅ | compile |
| T3 | T1 | reuse `sws_scale`; **no** wall-clock scheduler; interrupt-based cancel | RGBA+PTS to sink | ✅ | compile; device T-EXPORT-VIDEO pending |
| T4 | T1 | one GL thread, context current; reuse `GlProgram`; no window surface | render+readback | ✅ | compile; device pending |
| T5 | T1 | byte-buffer input only (no `createInputSurface`, minSdk21/Risk-3); even dims; SPS/PPS via csd-0/1 + config-buffer | H.264 ES + config | ✅ | compile+link |
| T6 | T1,T5 | trailer before success; delete partial on fail; avcC from SPS/PPS; no libavcodec H.264 encode | MP4 muxed | ✅ | compile+link |
| T7 | T3–T6 | effect parity (PTS speed/adjust/filter); stop-before-release; 1 terminal event; partial-file delete | end-to-end single clip | ✅ | compile; device pending |
| T8 | T7,T2 | hand-mangled names ↔ Kotlin; cached `gJvm`; global-ref + attach/detach | JNI reachable | ✅ | link resolves symbols |
| T9 | T8,T2 | one active export; sync reject w/o listener; native validation authoritative | API compiles+validates | ✅ | kotlin compile |
| T10 | T1 | A/V sync via atempo=speed; video-only fallback (A-2) | AAC packets | ✅ | compile; device pending |
| T11 | T7,T10 | interleave by PTS; video-only when no source audio | audio muxed | ✅ | compile; device pending |
| T12 | T9,T11 | order preserved; fail-fast + segment index; reuse timeline shape | one concat MP4 | ✅ | compile; device pending |

**Implementation-local decision (recorded):** MediaCodec surfaces SPS/PPS only after the first frame
is encoded, but the FFmpeg muxer needs the header (all streams) written first. Resolved by writing the
MP4 header lazily on the first video sample: the encoder's config sink adds the video stream (avcC
from SPS/PPS), then `ensureHeader()` adds the audio stream (AAC context known up front) and writes the
header exactly once, before the first sample. Within D-5; no contract change.

## 4. Authorized command results

- **Command:** `JAVA_HOME=<Android Studio JBR 21> ./gradlew :videolib:compileDebugKotlin :videolib:externalNativeBuildDebug --console=plain`
- **Scope:** `:videolib` compile + native link only (module default verification; android-dev-authorized).
- **Outcome:** **BUILD SUCCESSFUL**, 0 warnings. Native `libvideolib.so` built for **arm64-v8a** and
  **armeabi-v7a**; Kotlin `compileDebugKotlin` passed.
- **Environment:** macOS; NDK 29.0.14206865; CMake 3.22.1. The system `JAVA_HOME` (JDK 17) cannot
  configure the project (`:camera` → published `com.chiistudio:plugin:1.0.0` requires JVM 21); the
  Android Studio bundled JBR 21 was used. This is a pre-existing environment constraint, unrelated to
  the change.
- **Proof level:** compile+link proves JNI symbol/signature sync, the new `mediandk` link, FFmpeg
  avformat/avcodec/avfilter/swscale/swresample usage, MediaCodec NDK API, ABI packaging, and 16 KB
  alignment config. It does **not** prove encode/mux/decode/GL/audio-sync runtime behavior — that is
  device/ABI-only (Testing Handoff below).
- **Not run — authorization required:** `:videolib:testDebugUnitTest` (JVM), `:videolib:assembleDebug`
  (full AAR), `:videolib:connectedDebugAndroidTest` (device). Suggested next command:
  `./gradlew :videolib:testDebugUnitTest` (JVM validation) then a device run for the instrumented set.

## 5. Testing Handoff

| testing Task | Work | AC/risk | Test-ID | level | target/contract | behavior/error scope | fake/fixture | changed paths | depends on | execution expectation |
|---|---|---|---|---|---|---|---|---|---|---|
| T-VAL | W1 | AC-7 | T-VAL | JVM unit | `VideoExporter` API | reject blank path/bad speed/bad interval/non-writable output w/o firing listener; single active export | none (pure Kotlin, no native load) | `VideoExporter.kt` | T9 | runnable `:videolib:testDebugUnitTest` (create `VideoExporterTest.kt`, C12) |
| T-DEV | W1 | AC-1,2,3,4,5 | T-EXPORT-VIDEO | instrumented | end-to-end single-clip | playable MP4, H.264 track, effect parity (speed PTS + adjust + filter) | `TestVideoFixture` | `video_export.*`,`frame_source.*`,`offscreen_renderer.*`,`h264_encoder.*`,`mp4_muxer.*` | T9 | device/ABI only |
| T-DEV | W1 | AC-8 | T-CANCEL | instrumented | `VideoExport` lifecycle | cancel mid-export stops promptly; partial file deleted; no completed callback | `TestVideoFixture` | `video_export.cpp` | T9 | device only |
| T-DEV | W1 | AC-7 | T-ERR | instrumented | error path | unreadable/non-video source → typed `PlaybackError`; no partial success | invalid/short fixture | `frame_source.cpp`,`video_export.cpp` | T9 | device only |
| T-DEV | W3 | AC-6 | T-AUDIO | instrumented | audio + mux | AAC track in sync with sped video; no-audio source → video-only (A-2) | fixtures w/ + w/o audio | `audio_transcoder.*`,`mp4_muxer.*` | T11 | device only |
| T-DEV | W4 | AC-9 | T-TIMELINE | instrumented | timeline orchestration | ordered segments → one MP4; per-segment effects; segment failure → error+index | multi-clip fixtures | `video_export.cpp`,`videolib.cpp`,`VideoExporter.kt` | T12 | device only |
| T-DEV | build | Risk-3 | T-LINK | instrumented | `libvideolib.so` load | `mediandk` + new exports resolve at runtime on a 21-floor device | none | `CMakeLists.txt`,`videolib.cpp` | T8 | device only |

## 6. Integration Handoff

| integration Task | Check-ID | changed module | affected consumer/external contract | boundary | exact command / device check | environment | blocking policy |
|---|---|---|---|---|---|---|---|
| T-IT | Check-1 | videolib | build/packaging | CMake link, ABI, 16 KB | `:videolib:assembleDebug` + AAR `.so` inspection (`mediandk` resolvable) | JVM 21 + NDK 29 | blocking — native link must pass (already green for native-build subtask) |
| T-IT | Check-2 | videolib | `app` (project dep) + external (unknown) | additive public Kotlin API | `:app:assembleDebug` | JVM 21 | blocking — confirm additive API does not break in-repo consumer |
| T-IT | Check-3 | videolib | native runtime | encode/mux/GL/audio/timeline | supported-device/ABI export run (T-DEV set) | physical device, arm ABI | blocking for release — compile cannot prove runtime |

Handoff to testing on `AUTOMATION: CONTINUE`. Publishing/signing/upload/commit/push remain separately
authorized and were not performed.
