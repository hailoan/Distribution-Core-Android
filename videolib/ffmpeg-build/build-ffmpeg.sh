#!/usr/bin/env bash
# build-ffmpeg.sh — cross-compile FFmpeg from source for Android with the NDK.
# Ticket: build-ffmpeg-for-android  (Task T2/C2 -> produces artifact tree C4)
#
# Out-of-band build tooling for :videolib. Autotools (./configure && make) cannot
# be driven by Gradle externalNativeBuild/CMake, so this runs on a build host / CI.
# It does NOT modify :videolib runtime code (build.gradle.kts, CMakeLists.txt,
# NativeLib.kt, videolib.cpp) nor camera/ffmpegv2 (read-only vendored input).
#
# Requirements: bash, git, make, and Android NDK 29.0.14206865 (autotools clang).
# Usage:
#   ANDROID_NDK_HOME=/path/to/ndk/29.0.14206865 ./build-ffmpeg.sh
#
# Output (C4 / D7), per ABI, mirroring the camera ffmpegv2/<abi> layout:
#   dist/<abi>/include/...        FFmpeg headers
#   dist/<abi>/lib/*.a            static libraries (LINKAGE=static, D6)
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=./config.sh
source "$HERE/config.sh"

SRC_DIR="$HERE/src/ffmpeg"       # pinned FFmpeg checkout (T1)
BUILD_ROOT="$HERE/build"         # per-ABI intermediate build dirs
DIST_ROOT="$HERE/dist"           # per-ABI artifact tree (C4, the deliverable)

# --- Resolve the NDK ---------------------------------------------------------
: "${ANDROID_NDK_HOME:=${ANDROID_NDK_ROOT:-}}"
if [ -z "${ANDROID_NDK_HOME:-}" ] || [ ! -d "$ANDROID_NDK_HOME" ]; then
  echo "ERROR: set ANDROID_NDK_HOME to the NDK $NDK_VERSION toolchain." >&2
  exit 2
fi
# The NDK ships exactly one prebuilt host toolchain (e.g. darwin-x86_64,
# linux-x86_64). Resolve it by glob rather than hardcoding the host tag.
TOOLCHAIN="$(echo "$ANDROID_NDK_HOME"/toolchains/llvm/prebuilt/*/)"
if [ ! -d "$TOOLCHAIN" ] || [ ! -x "${TOOLCHAIN}bin/llvm-ar" ]; then
  echo "ERROR: NDK prebuilt toolchain not found under $ANDROID_NDK_HOME/toolchains/llvm/prebuilt/" >&2
  exit 2
fi
TOOLCHAIN="${TOOLCHAIN%/}"   # strip trailing slash left by the glob

# --- Acquire pinned FFmpeg source (T1/C1) ------------------------------------
if [ ! -d "$SRC_DIR/.git" ]; then
  echo ">> Cloning FFmpeg $FFMPEG_TAG"
  git clone --depth 1 --branch "$FFMPEG_TAG" "$FFMPEG_REPO" "$SRC_DIR"
else
  echo ">> Reusing FFmpeg checkout at $SRC_DIR"
  git -C "$SRC_DIR" fetch --depth 1 origin "$FFMPEG_TAG"
  git -C "$SRC_DIR" checkout -q "$FFMPEG_TAG"
fi

# --- Map an ABI to its FFmpeg --arch/--cpu and clang target triple -----------
abi_arch()   { case "$1" in arm64-v8a) echo aarch64;; armeabi-v7a) echo arm;; *) echo "?";; esac; }
abi_triple() { # clang target prefix (without API level)
  case "$1" in
    arm64-v8a)   echo aarch64-linux-android;;
    armeabi-v7a) echo armv7a-linux-androideabi;;
    *) echo "?";;
  esac
}
abi_api() { # per-ABI API level from config.sh (honors minSdk 21, AC-4)
  local v="API_${1//-/_}"; echo "${!v}"
}

build_one_abi() {
  local abi="$1"
  local arch triple api
  arch="$(abi_arch "$abi")"
  triple="$(abi_triple "$abi")"
  api="$(abi_api "$abi")"

  if [ "$arch" = "?" ] || [ "$triple" = "?" ] || [ -z "$api" ]; then
    echo "ERROR: unsupported ABI '$abi' (add mappings in build-ffmpeg.sh / config.sh)" >&2
    exit 2
  fi

  local cc="$TOOLCHAIN/bin/${triple}${api}-clang"
  local cxx="$TOOLCHAIN/bin/${triple}${api}-clang++"
  local build_dir="$BUILD_ROOT/$abi"
  local prefix="$DIST_ROOT/$abi"
  if [ ! -x "$cc" ]; then
    echo "ERROR: clang not found for $abi at $cc (check NDK API level $api)" >&2
    exit 2
  fi

  echo ">> [$abi] arch=$arch api=$api  ($triple)"
  rm -rf "$build_dir" "$prefix"          # no partial artifact carried over (AC-1 failure rule)
  mkdir -p "$build_dir"

  # 16 KB page alignment (FR-2/AC-2). NOTE: for LINKAGE=static this ldflag does
  # NOT alter the produced .a (no link happens); it takes effect — and is proven
  # (check-alignment.sh) — when the consumer links these archives into its .so.
  # It is passed here so a LINKAGE=shared build (or the probe link) is aligned.
  local ldflags="-Wl,-z,max-page-size=${MAX_PAGE_SIZE}"

  # Linkage (D6): static archives, no shared objects.
  local link_flags="--enable-static --disable-shared"

  # Component scope (config.sh): full build, or a slim allowlist. A deliberate
  # feature decision, not guessed here — see config.sh COMPONENTS / DEV-SPEC.
  local component_flags=""
  case "$COMPONENTS" in
    full) component_flags="" ;;
    slim) component_flags="$COMPONENTS_SLIM_FLAGS" ;;
    *) echo "ERROR: COMPONENTS='$COMPONENTS' invalid (use 'full' or 'slim')" >&2; exit 2 ;;
  esac
  echo ">> [$abi] components=$COMPONENTS"

  ( cd "$build_dir" && "$SRC_DIR/configure" \
      --prefix="$prefix" \
      --target-os=android \
      --arch="$arch" \
      --cpu="$([ "$abi" = armeabi-v7a ] && echo armv7-a || echo armv8-a)" \
      --enable-cross-compile \
      --cross-prefix="$TOOLCHAIN/bin/llvm-" \
      --cc="$cc" \
      --cxx="$cxx" \
      --ar="$TOOLCHAIN/bin/llvm-ar" \
      --nm="$TOOLCHAIN/bin/llvm-nm" \
      --ranlib="$TOOLCHAIN/bin/llvm-ranlib" \
      --strip="$TOOLCHAIN/bin/llvm-strip" \
      --sysroot="$TOOLCHAIN/sysroot" \
      $link_flags \
      $component_flags \
      --enable-pic \
      --disable-programs \
      --disable-doc \
      --disable-debug \
      --disable-nonfree \
      --extra-ldflags="$ldflags" \
      $([ "$abi" = armeabi-v7a ] && echo "--extra-cflags=-mfpu=neon") )
      # LICENSE=lgpl (AC-5): intentionally NO --enable-gpl / NO --enable-nonfree above.
      # --disable-debug strips FFmpeg's default -g debug info from the .a archives
      # (~debug info dominates archive size); the linked consumer .so is unaffected.
      # Component scope: this is a FULL FFmpeg build (all LGPL decoders/encoders/
      # muxers/demuxers). To shrink the deliverable, add a --disable-everything +
      # --enable-decoder/... allowlist here — that is a deliberate feature decision
      # (see DEV-SPEC "enabled components" open item), so it is intentionally NOT
      # guessed by this script.

  make -C "$build_dir" -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"
  make -C "$build_dir" install

  # AC-1 self-check: a successful ABI must leave real artifacts, never a partial tree.
  local n_libs n_hdrs
  n_libs="$(find "$prefix/lib" -name '*.a' -type f 2>/dev/null | wc -l | tr -d ' ')"
  n_hdrs="$(find "$prefix/include" -name '*.h' -type f 2>/dev/null | wc -l | tr -d ' ')"
  if [ "$n_libs" -eq 0 ] || [ "$n_hdrs" -eq 0 ]; then
    echo "ERROR: [$abi] install produced no artifacts (libs=$n_libs headers=$n_hdrs); removing partial tree" >&2
    rm -rf "$prefix"
    exit 1
  fi

  echo ">> [$abi] installed to $prefix ($n_libs *.a, $n_hdrs headers)"
}

# Emit build provenance so the deferred integration knows exactly what it links,
# and can verify artifact integrity. Deterministic content (no timestamps) so a
# reproducible rebuild yields an identical manifest.
write_manifest() {
  local ffmpeg_commit sha_tool manifest="$DIST_ROOT/MANIFEST.txt" sums="$DIST_ROOT/SHA256SUMS"
  ffmpeg_commit="$(git -C "$SRC_DIR" rev-parse HEAD 2>/dev/null || echo unknown)"

  {
    echo "# FFmpeg-from-source build manifest — :videolib (ticket build-ffmpeg-for-android)"
    echo "ffmpeg_version:  $FFMPEG_VERSION"
    echo "ffmpeg_tag:      $FFMPEG_TAG"
    echo "ffmpeg_commit:   $ffmpeg_commit"
    echo "ffmpeg_repo:     $FFMPEG_REPO"
    echo "abis:            ${ABIS[*]}"
    echo "api_level:       arm64-v8a=$API_arm64_v8a armeabi-v7a=$API_armeabi_v7a"
    echo "license:         $LICENSE (no --enable-gpl, no --enable-nonfree)"
    echo "linkage:         $LINKAGE"
    echo "components:      $COMPONENTS"
    [ "$COMPONENTS" = slim ] && echo "component_flags: $COMPONENTS_SLIM_FLAGS"
    echo "max_page_size:   $MAX_PAGE_SIZE (16 KB alignment, AC-2)"
    echo "ndk_version:     $NDK_VERSION"
    echo "consumer_note:   linking arm64-v8a static archives into a .so requires -Wl,-Bsymbolic"
    echo ""
    echo "# artifacts per ABI"
    for abi in "${ABIS[@]}"; do
      echo "[$abi]"
      ( cd "$DIST_ROOT/$abi/lib" 2>/dev/null && ls -1 *.a 2>/dev/null | sed 's/^/  lib\//' )
    done
  } > "$manifest"

  # SHA-256 over every produced library, path-relative to dist/ for portability.
  if command -v sha256sum >/dev/null 2>&1; then sha_tool="sha256sum"
  elif command -v shasum   >/dev/null 2>&1; then sha_tool="shasum -a 256"
  else sha_tool=""; fi
  if [ -n "$sha_tool" ]; then
    ( cd "$DIST_ROOT" && find . -name '*.a' -type f | LC_ALL=C sort | xargs $sha_tool > SHA256SUMS )
    echo ">> Wrote $manifest and $sums"
  else
    echo ">> Wrote $manifest (no sha256 tool found; skipped SHA256SUMS)" >&2
  fi
}

mkdir -p "$DIST_ROOT"
for abi in "${ABIS[@]}"; do
  build_one_abi "$abi"
done
write_manifest

echo ">> Build complete. Run ./check-alignment.sh to verify 16 KB alignment (AC-2)."
