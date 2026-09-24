#!/usr/bin/env bash
# check-alignment.sh — 16 KB page-alignment gate for the produced FFmpeg artifacts.
# Ticket: build-ffmpeg-for-android  (Task T3/C3, AC-2 / FR-2 / SC-2)
#
# Asserts every produced runtime-loadable ELF has all LOAD segments aligned to
# 0x4000 (16 KB). Exits non-zero on the first misaligned ELF so it can gate the build.
#
# LINKAGE=static (D6): the deliverable is .a archives, which have no LOAD segments
# of their own — alignment can only be proven on a shared object. So for each ABI
# this gate links a throwaway probe .so from the produced libavutil.a using the SAME
# max-page-size=16384 ldflag the build uses, then verifies its LOAD alignment. The
# aarch64 archives need -Wl,-Bsymbolic (FFmpeg's tx_float_neon.S makes PC-relative
# refs to local .rodata TX tables); this gate applies it and records the constraint
# for the downstream consumer .so link.
#
# LINKAGE=shared: any produced *.so is checked directly.
#
# Requires ANDROID_NDK_HOME for the static-probe link. Set SKIP_PROBE_LINK=1 to fall
# back to presence-only reporting (does NOT prove alignment; not for release gating).
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=./config.sh
source "$HERE/config.sh"

DIST_ROOT="$HERE/dist"
SKIP_PROBE_LINK="${SKIP_PROBE_LINK:-0}"
WANT_ALIGN_HEX="4000"   # 16384 == 0x4000

if [ ! -d "$DIST_ROOT" ]; then
  echo "ERROR: no dist/ artifact tree. Run ./build-ffmpeg.sh first." >&2
  exit 2
fi

# --- Resolve NDK toolchain (for readelf and the static probe link) -----------
: "${ANDROID_NDK_HOME:=${ANDROID_NDK_ROOT:-}}"
HOST_TAG="$(uname -s | tr '[:upper:]' '[:lower:]')-x86_64"
TC=""
[ -n "${ANDROID_NDK_HOME:-}" ] && TC="$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/$HOST_TAG/bin"

READELF="${READELF:-}"
if [ -z "$READELF" ]; then
  if [ -n "$TC" ] && [ -x "$TC/llvm-readelf" ]; then READELF="$TC/llvm-readelf"
  elif command -v llvm-readelf >/dev/null 2>&1; then READELF="llvm-readelf"
  elif command -v readelf     >/dev/null 2>&1; then READELF="readelf"
  fi
fi
if [ -z "$READELF" ] || ! command -v "$READELF" >/dev/null 2>&1; then
  echo "ERROR: no readelf/llvm-readelf available to verify alignment." >&2
  exit 2
fi

fail=0

# clang wrapper + extra link flags per ABI for the static probe link.
abi_clang() {
  case "$1" in
    arm64-v8a)   echo "$TC/aarch64-linux-android${API_arm64_v8a}-clang";;
    armeabi-v7a) echo "$TC/armv7a-linux-androideabi${API_armeabi_v7a}-clang";;
    *) echo "";;
  esac
}
abi_extra_ldflags() {
  # aarch64 static archives need -Bsymbolic to resolve local TX-table relocations.
  case "$1" in arm64-v8a) echo "-Wl,-Bsymbolic";; *) echo "";; esac
}

# Verify one shared ELF: every LOAD segment Align must be >= 0x4000.
check_so() {
  local f="$1" aligns bad=0 a
  aligns="$("$READELF" -l "$f" 2>/dev/null | awk '/LOAD/ { print $NF }')"
  if [ -z "$aligns" ]; then echo "    ?? $f — no LOAD segments"; fail=1; return; fi
  for a in $aligns; do
    if [ "$(( a ))" -lt "$(( 0x$WANT_ALIGN_HEX ))" ]; then bad=1; fi
  done
  if [ "$bad" -eq 0 ]; then echo "    OK  $(basename "$f") LOAD align >= 0x$WANT_ALIGN_HEX ($(echo "$aligns" | sort -u | tr '\n' ' '))"
  else echo "    FAIL $(basename "$f") LOAD align < 0x$WANT_ALIGN_HEX: $aligns"; fail=1; fi
}

# Static-linkage proof: link a throwaway probe .so from libavutil.a and check it.
probe_static_abi() {
  local abi="$1" clang extra probe
  clang="$(abi_clang "$abi")"
  extra="$(abi_extra_ldflags "$abi")"
  if [ -z "$clang" ] || [ ! -x "$clang" ]; then
    echo "    !! no clang for $abi at TC=$TC — cannot prove alignment"; fail=1; return
  fi
  probe="$(mktemp -d)/probe_${abi}.so"
  if "$clang" -shared -o "$probe" \
        -Wl,-z,max-page-size="$MAX_PAGE_SIZE" $extra \
        -Wl,--whole-archive "$DIST_ROOT/$abi/lib/libavutil.a" -Wl,--no-whole-archive \
        -lm -lz 2>"$probe.err"; then
    [ -n "$extra" ] && echo "    (linked probe .so with '$extra' — consumer .so must use this too)"
    check_so "$probe"
  else
    echo "    FAIL $abi probe link failed:"; sed 's/^/      /' "$probe.err" | head -4; fail=1
  fi
  rm -rf "$(dirname "$probe")"
}

for abi in "${ABIS[@]}"; do
  d="$DIST_ROOT/$abi"
  [ -d "$d" ] || { echo ">> [$abi] MISSING dir $d"; fail=1; continue; }
  echo ">> [$abi] $d"
  a_count="$(find "$d" -name '*.a' -type f 2>/dev/null | wc -l | tr -d ' ')"
  so_list="$(find "$d" -name '*.so' -type f 2>/dev/null)"
  echo "    artifacts: ${a_count} *.a  $( [ -n "$so_list" ] && echo "$(echo "$so_list" | wc -l | tr -d ' ') *.so" )"

  if [ "$a_count" -eq 0 ] && [ -z "$so_list" ]; then
    echo "    FAIL no libraries produced"; fail=1; continue
  fi

  # Any real shared objects get a direct check.
  if [ -n "$so_list" ]; then
    while IFS= read -r so; do check_so "$so"; done <<< "$so_list"
  fi

  # Static archives: prove alignment via a probe link unless explicitly skipped.
  if [ "$a_count" -gt 0 ]; then
    if [ "$SKIP_PROBE_LINK" = "1" ]; then
      echo "    SKIP_PROBE_LINK=1 — presence only, alignment NOT proven (not for release gating)"
    else
      probe_static_abi "$abi"
    fi
  fi
done

if [ "$fail" -ne 0 ]; then echo ">> ALIGNMENT GATE: FAIL"; exit 1; fi
echo ">> ALIGNMENT GATE: PASS (AC-2) — all produced ELFs 16 KB-aligned"
