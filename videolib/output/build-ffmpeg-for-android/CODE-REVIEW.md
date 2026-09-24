AUTOMATION: CONTINUE

# CODE-REVIEW — build-ffmpeg-for-android

Impl-flow review of the `:videolib` FFmpeg-from-source build tooling. Read-only. All five
predecessors begin `AUTOMATION: CONTINUE`. Verdict is decided last (§5).

**Scope.** Reviewed the ticket's production diff: untracked `videolib/ffmpeg-build/` —
`config.sh`, `build-ffmpeg.sh`, `check-alignment.sh`, `.gitignore`, `README.md` — plus the pipeline
artifacts. **Excluded and disclosed as unrelated pre-existing user changes** (not this ticket, not
judged): `D .claude/settings.json`, `D .claude/skills/*`, `M CLAUDE.md`, `?? .aidlc/generated-adapters/*`,
`?? .claude/`.

**Note on drift.** The scripts were extended after the CHANGESET/INTEGRATION-TEST-REPORT were written
(hardening, `--disable-debug`, opt-in `COMPONENTS`, provenance manifest, `ABIS` array). This review
judges the **current** diff and records the resulting artifact staleness as Finding 3.

## 1. Findings

| severity | category | file:line | failure scenario | recommendation |
|---|---|---|---|---|
| Minor | portability | `check-alignment.sh:37,39` | Host-tag is hardcoded `-x86_64` (`HOST_TAG="$(uname…)-x86_64"`). `build-ffmpeg.sh:35` was hardened to glob `prebuilt/*/`, but the gate was not. On a host whose NDK prebuilt dir is not `<os>-x86_64` (e.g. a future `darwin-arm64` NDK), `TC` resolves to a non-existent path; the gate then falls back to a PATH `llvm-readelf`/`readelf` if present, else exits 2 — and `abi_clang` finds no compiler, so the probe link reports `!! no clang … cannot prove alignment` and the gate FAILs. Inconsistent with the producer. | Resolve `TC` by the same glob as `build-ffmpeg.sh` (`prebuilt/*/bin`) instead of composing `-x86_64`. |
| Minor | correctness/reporting | `check-alignment.sh:107` | The `*.so` count in the artifacts line is interpolated inside the same line via a command substitution; if `find` returns multiple `.so` the `wc -l` is correct, but when `so_list` is empty the trailing `$( … )` yields an empty token, leaving a dangling two-space suffix. Cosmetic only — never affects pass/fail. | Optional: guard the whole `*.so` clause or trim; no functional impact. |
| Minor (obs) | traceability | `output/CHANGESET.md`, `output/INTEGRATION-TEST-REPORT.md` | Both artifacts predate the `--disable-debug`, `COMPONENTS` slim profile, `MANIFEST.txt`/`SHA256SUMS`, and `ABIS`-array changes now in the scripts (grep confirms zero mentions). A reader trusting the CHANGESET manifest would not know the current tooling behavior/outputs. Not a code defect. | Refresh CHANGESET §2 + INTEGRATION §3 to cover the added flags/outputs, or note them as post-review enhancements. |

No Critical/Major findings. Security: no secrets, no hardcoded credentials; `FFMPEG_REPO` is a config
constant, not a hardcoded secret. License posture (LGPL, no gpl/nonfree) is explicit and correct.

## 2. Runtime / PR readiness

- **Executed evidence (this session, real host):** `build-ffmpeg.sh` → exit 0, 14 `.a` + headers both
  ABIs; `check-alignment.sh` → PASS (both ABIs probe-linked, LOAD align `0x4000`); `SHA256SUMS`
  verified `-c` exit 0; `bash -n` clean on all three scripts. These match INTEGRATION-TEST-REPORT
  Check-1/Check-2/Check-3 (PASS) and exceed the CHANGESET (which honestly recorded the build as
  `not run` at android-dev time).
- **`set -euo pipefail`** on all scripts; fail-fast guards for missing NDK, unknown ABI, missing
  clang, empty install (AC-1 self-check), and invalid `COMPONENTS`.
- **Guarded invariant preserved:** `videolib/build.gradle.kts`, `videolib/src/**`, and all `camera/**`
  are unchanged (verified) — the runtime module and vendored FFmpeg 3.2.12 were not touched.
- **Provenance:** `dist/MANIFEST.txt` + `dist/SHA256SUMS` are generated under `dist/` and are
  git-ignored with the rest of `dist/`; they travel with the artifacts, not the repo. Consistent with
  "produced on demand".
- **Device runtime** of FFmpeg is not exercised — correctly deferred to the integration ticket; not a
  release blocker for this build-tooling ticket.

## 3. Change-scope reconciliation

| Change-ID | Task | Design-Ref | planned path/symbol/action | actual diff path/symbol/status | authorized | design evidence | review status |
|---|---|---|---|---|---|---|---|
| C1 | T1 | none | create source pin | `config.sh` (FFMPEG_TAG n7.1, ABIS array, API, LICENSE, LINKAGE, COMPONENTS) added | yes | AC-3/D2 | ✔ matches; superset (adds COMPONENTS opt-in) |
| C2 | T2 | none | create orchestrator | `build-ffmpeg.sh` added (per-ABI configure→make→install; `--disable-debug`; component_flags; write_manifest) | yes | AC-1/4/5, D3/D6 | ✔ matches; superset |
| C3 | T3 | none | create alignment gate | `check-alignment.sh` added (probe-link proof, `-Bsymbolic`) | yes | AC-2 | ✔ matches; stronger than planned readelf-only |
| C4 | T2/T3 | none | produce artifact tree | `dist/<abi>/{include,lib}` + MANIFEST/SHA256SUMS (git-ignored) | yes | AC-1/D7 | ✔ produced and verified |
| — | — | — | support | `.gitignore`, `README.md` added | yes | plan §2 | ✔ |

No unmapped production path; no unauthorized scope; no change to `videolib-runtime` or `camera`.
Every changed path resolves to an approved Change-ID.

## 4. Trace verification

| FR | SC | AC | Work | prod Task | Change/module/path | testing Task/Test | integration Check/consumer | executed result | review status |
|---|---|---|---|---|---|---|---|---|---|
| FR-1 | SC-1 | AC-3 | W1 | T1 | C1 / `:videolib` / `config.sh` | n/a (build tooling) | Check-3 | PASS (tag n7.1) | ✔ |
| FR-1 | SC-1 | AC-1 | W1 | T2 | C2 / `:videolib` / `build-ffmpeg.sh` | n/a | Check-1 | PASS (14 `.a`+headers, both ABIs) | ✔ |
| FR-1 | SC-1 | AC-4 | W1 | T2 | C2 / API=21 both ABIs | n/a | Check-3 | PASS (≥ minSdk 21) | ✔ |
| FR-1 | SC-1 | AC-5 | W1 | T2 | C2 / no gpl/nonfree | n/a | Check-3 | PASS (LGPL) | ✔ |
| FR-2 | SC-2 | AC-2 | W1 | T3 | C3 / `check-alignment.sh` | n/a | Check-2a/2b | PASS (LOAD align 0x4000 both ABIs) | ✔ |

`testing Task/Test = n/a` is justified: no JVM/Android unit-testable symbol exists (build tooling).
UNIT-TEST-REPORT.md absence is a disclosed, non-blocking process gap, not treated as passed.

## 5. Verdict

**Go — Approved with comments.**

Basis: every AC traces to executed, green native/build evidence (AC-1..AC-5 PASS); the diff is
confined to the authorized `videolib/ffmpeg-build/` package with the runtime module and vendored
`camera` FFmpeg untouched; no Critical/Major finding and no failed required check. The three Minor
items (a host-tag portability inconsistency in the gate, a cosmetic reporting nit, and staleness of
CHANGESET/INTEGRATION-TEST-REPORT vs. the enhanced scripts) are non-blocking comments for follow-up,
not defects that gate this build-tooling deliverable. On-device FFmpeg runtime remains correctly
deferred to the integration ticket.

Publishing, signing, upload, distribution, commit, and push remain separately authorized and were
not performed.
