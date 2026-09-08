# FFmpeg-from-source build for `:videolib`

Cross-compiles FFmpeg from source for Android with the NDK, producing 16 KB-aligned
per-ABI artifacts. Ticket: `output/build-ffmpeg-for-android`.

This is **out-of-band build tooling** — FFmpeg uses autotools (`./configure && make`),
which Gradle's `externalNativeBuild`/CMake cannot drive. It runs on a build host / CI,
not as part of `:videolib:assembleDebug`. It does not modify `:videolib` runtime code;
wiring the artifacts into the module's CMake/JNI is a separate, later ticket.

## Locked build parameters (`config.sh`)

| Parameter | Value | Source |
| --- | --- | --- |
| FFmpeg version | 7.1 (tag `n7.1`) | AC-3 |
| ABIs | `arm64-v8a`, `armeabi-v7a` | D3 |
| API level | 21 per ABI (honors `minSdk 21`) | AC-4 / D4 |
| License | LGPL-safe (no `--enable-gpl`, no `--enable-nonfree`) | AC-5 / D5 |
| Linkage | static `.a` + headers | D6 |
| Page size | `max-page-size=16384` (16 KB) | AC-2 / FR-2 |

## Usage

```sh
export ANDROID_NDK_HOME=/path/to/ndk/29.0.14206865
./build-ffmpeg.sh        # fetch pinned source, cross-compile each ABI -> dist/<abi>/
./check-alignment.sh     # 16 KB alignment gate over dist/ (AC-2)
```

Output: `dist/<abi>/include` + `dist/<abi>/lib/*.a`, mirroring the `camera` module's
`ffmpegv2/<abi>` layout for easy reuse at integration. The build also writes
`dist/MANIFEST.txt` (FFmpeg version + commit, ABIs, API level, license, component profile,
page size, and the arm64 `-Bsymbolic` consumer note) and `dist/SHA256SUMS` (checksums of
every `.a`) as build provenance for the integration step. Verify with:

```sh
( cd dist && shasum -a 256 -c SHA256SUMS )   # or: sha256sum -c
```

## Component scope & size

The build defaults to `COMPONENTS=full` — all LGPL decoders, encoders, muxers, demuxers,
and filters — built with `--disable-debug` (FFmpeg's default `-g` debug info otherwise
dominates the `.a` size). Full archives are large because they contain every component; the
final consumer `.so` links in and strips only what it references, so shipped app size is far
smaller than the archive total.

Choosing which components to ship is a **deliberate feature decision** (which formats
`:videolib` must handle), so it is explicit in `config.sh`, not guessed by the script:

```sh
COMPONENTS=slim ./build-ffmpeg.sh          # use the allowlist in config.sh
# or supply your own set for one run:
COMPONENTS=slim \
COMPONENTS_SLIM_FLAGS="--disable-everything --enable-decoder=h264 --enable-demuxer=mov ..." \
  ./build-ffmpeg.sh
```

Measured on this machine (arm64-v8a `lib/`, FFmpeg 7.1, `--disable-debug`):

| Profile | arm64 lib | armeabi-v7a lib |
| --- | --- | --- |
| `full` (default) | ~30 MB | ~25 MB |
| `slim` (example allowlist: h264/aac/mp3 in mov/mp4/mkv) | ~5.6 MB | ~4.4 MB |

Edit `COMPONENTS_SLIM_FLAGS` in `config.sh` to match the module's real format needs before
relying on `slim`. 16 KB alignment (AC-2) holds for both profiles.

`check-alignment.sh` proves 16 KB alignment by linking a throwaway probe `.so` per ABI
from the produced archives (with the build's `max-page-size=16384` ldflag) and reading its
LOAD segment alignment. It needs `ANDROID_NDK_HOME`. `SKIP_PROBE_LINK=1` falls back to
presence-only reporting (does not prove alignment — not for release gating).

## Consumer link note (arm64-v8a)

When the deferred integration links these **static** archives into `libvideolib.so`, the
**arm64-v8a** link requires `-Wl,-Bsymbolic`. Without it the link fails with
`R_AARCH64_ADR_PREL_PG_HI21 cannot be used against symbol 'ff_tx_tab_*_float'` — FFmpeg's
aarch64 NEON assembly (`libavutil/aarch64/tx_float_neon.S`) makes PC-relative references to
local `.rodata` TX tables. `armeabi-v7a` is unaffected. The alignment gate applies this flag
for its arm64 probe and prints the reminder.

## Notes

- Requires bash, git, make, and NDK 29.0.14206865 on the build host.
- `src/`, `build/`, `dist/` are git-ignored (produced on demand).
- Do not modify `camera/src/main/cpp/ffmpegv2` — that is a separate vendored FFmpeg 3.2.12 tree.
