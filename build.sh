#!/usr/bin/env bash
# build.sh — Android build script for OpenTyrian2000
set -euo pipefail

# ---------------------------------------------------------------------------
# Defaults
# ---------------------------------------------------------------------------
PLATFORM="android"
JOBS=$(nproc 2>/dev/null || sysctl -n hw.logicalcpu 2>/dev/null || echo 4)
DEBUG=1               # debug by default (signed, sideloadable)
CLEAN=0
VERBOSE=0

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
RED='\033[0;31m'; YELLOW='\033[1;33m'; GREEN='\033[0;32m'; RESET='\033[0m'
info()  { echo -e "${GREEN}[build]${RESET} $*"; }
warn()  { echo -e "${YELLOW}[warn]${RESET}  $*"; }
error() { echo -e "${RED}[error]${RESET} $*" >&2; exit 1; }

require() {
    for cmd in "$@"; do
        command -v "$cmd" &>/dev/null || error "Required tool not found: $cmd"
    done
}

usage() {
    cat <<EOF
Usage: $0 [options]

Builds an Android APK. Debug mode is the default (auto-signed, sideloadable).

Options:
  -j N        Parallel Gradle workers (default: $JOBS)
  -c          Clean before building
  --release   Build unsigned release APK instead of debug
  -v          Verbose Gradle output (--info)
  -h          This help

Examples:
  $0              # debug APK — ready to sideload
  $0 -c           # clean then build debug APK
  $0 --release    # unsigned release APK
EOF
    exit 0
}

# ---------------------------------------------------------------------------
# Argument parsing
# ---------------------------------------------------------------------------
while [[ $# -gt 0 ]]; do
    case "$1" in
        -j)         JOBS="$2"; shift 2 ;;
        -j*)        JOBS="${1#-j}"; shift ;;
        -c)         CLEAN=1; shift ;;
        --release)  DEBUG=0; shift ;;
        -v)         VERBOSE=1; shift ;;
        -h|--help)  usage ;;
        *)          error "Unknown option: $1  (use -h for help)" ;;
    esac
done

# ---------------------------------------------------------------------------
# Build: Android
# ---------------------------------------------------------------------------
build_android() {
    local android_dir="android-project"
    [[ -d "$android_dir" ]] || error "android-project/ directory not found"

    require java

    if [[ -z "${ANDROID_NDK_HOME:-}" ]] && [[ -z "${ANDROID_HOME:-}" ]]; then
        warn "ANDROID_HOME / ANDROID_NDK_HOME not set — Gradle may fail to locate NDK"
    fi

    local gradlew="$android_dir/gradlew"
    [[ -x "$gradlew" ]] || chmod +x "$gradlew"

    local gradle_flags=("--parallel" "--max-workers=$JOBS")
    [[ $VERBOSE -eq 1 ]] && gradle_flags+=("--info")

    local target="assembleDebug"
    [[ $DEBUG -eq 0 ]] && target="assembleRelease"

    info "Target: $target"

    pushd "$android_dir" >/dev/null

    if [[ $CLEAN -eq 1 ]]; then
        info "Cleaning..."
        ./gradlew "${gradle_flags[@]}" clean
    fi

    info "Running: ./gradlew ${gradle_flags[*]} $target"
    ./gradlew "${gradle_flags[@]}" "$target"

    local apk_subdir="debug"
    [[ $DEBUG -eq 0 ]] && apk_subdir="release"

    local apk_path
    apk_path="$(find "app/build/outputs/apk/$apk_subdir" -name '*.apk' 2>/dev/null | head -1 || true)"
    popd >/dev/null

    if [[ -n "$apk_path" ]]; then
        info "APK: $android_dir/$apk_path"
        if [[ $DEBUG -eq 0 ]]; then
            warn "Release APK is unsigned — sideloading will fail until you sign it"
            info "Sign with: apksigner sign --ks my-key.jks --out signed.apk $android_dir/$apk_path"
        fi
    else
        warn "Could not locate output APK — check Gradle output above"
    fi
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
cd "$(dirname "$0")"   # ensure we run from the repo root

build_android

info "Done."
