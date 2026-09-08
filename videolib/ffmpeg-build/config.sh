# config.sh — pinned inputs for the :videolib FFmpeg-from-source Android build.
# Ticket: build-ffmpeg-for-android. Sourced by build-ffmpeg.sh and check-alignment.sh.
# This file is the single source pin (T1/C1) and the locked build parameters.

# --- FFmpeg source pin (T1/C1, AC-3) — one tag for every ABI -----------------
FFMPEG_VERSION="7.1"                                  # locked: latest stable (UPI-1)
FFMPEG_TAG="n7.1"                                     # FFmpeg git release tag for 7.1
FFMPEG_REPO="https://git.ffmpeg.org/ffmpeg.git"       # mirror: https://github.com/FFmpeg/FFmpeg.git

# --- Target ABIs (D3 / UPI-2) ------------------------------------------------
# Bash array so iteration ("${ABIS[@]}") never relies on word-splitting.
ABIS=(arm64-v8a armeabi-v7a)                           # locked: two ARM ABIs (mirrors camera)

# --- Per-ABI minimum API level — must honor :videolib minSdk 21 (AC-4 / D4) --
API_arm64_v8a=21                                      # 64-bit ABI floor is 21; matches minSdk
API_armeabi_v7a=21                                    # 32-bit; matches minSdk 21

# --- License posture (AC-5 / D5 / UPI-3) -------------------------------------
# LGPL-safe: NO --enable-gpl and NO --enable-nonfree are passed by the builder.
LICENSE="lgpl"

# --- Linkage (D6 / UPI-4) ----------------------------------------------------
LINKAGE="static"                                      # locked: static .a + headers

# --- Component scope (DEV-SPEC "enabled components" open item) ----------------
# Which FFmpeg components are built. This is a deliberate feature decision, so it
# is explicit here rather than guessed. Override per-run: `COMPONENTS=slim ./build-ffmpeg.sh`.
#   full  — every LGPL decoder/encoder/muxer/demuxer/filter (default; proven).
#   slim  — --disable-everything + the allowlist in COMPONENTS_SLIM_FLAGS below.
COMPONENTS="${COMPONENTS:-full}"

# Allowlist used only when COMPONENTS=slim. EDIT THIS to match what :videolib must
# handle. The example below covers H.264/AAC playback in an MP4 container. Override
# per-run with COMPONENTS_SLIM_FLAGS="..." to avoid editing this file.
COMPONENTS_SLIM_FLAGS="${COMPONENTS_SLIM_FLAGS:-\
--disable-everything \
--enable-decoder=h264,aac,mp3 \
--enable-demuxer=mov,mp4,m4a,matroska \
--enable-parser=h264,aac \
--enable-protocol=file \
--enable-filter=aresample,scale}"

# --- 16 KB page alignment (FR-2 / AC-2) --------------------------------------
MAX_PAGE_SIZE=16384                                   # 0x4000

# --- Toolchain expectation (packet §8) ---------------------------------------
NDK_VERSION="29.0.14206865"                           # informational; NDK path resolved at runtime
