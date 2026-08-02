#!/usr/bin/env bash
#
# mcode installer.
#
# Usage:
#   curl -fsSL https://raw.githubusercontent.com/MohakGupta2004/mcode/master/install.sh | bash
#
# Installs build dependencies (cmake, a C++20 compiler, nlohmann-json, cpr),
# builds mcode from source, and installs the resulting binary onto PATH.
#
# Env overrides:
#   MCODE_INSTALL_DIR   Where to put the binary (default: /usr/local/bin, or
#                        $HOME/.local/bin if that isn't writable and there's
#                        no sudo).
#   MCODE_REPO_URL       Git URL to clone (default: upstream mcode repo).
#   MCODE_REF             Branch/tag to build (default: master).

set -euo pipefail

REPO_URL="${MCODE_REPO_URL:-https://github.com/MohakGupta2004/mcode.git}"
REF="${MCODE_REF:-master}"
BINARY_NAME="mcode"
INSTALL_DIR="${MCODE_INSTALL_DIR:-/usr/local/bin}"

BUILD_ROOT=""
CLONED=0

log()  { printf '\033[1;36m==>\033[0m %s\n' "$1"; }
warn() { printf '\033[1;33m!!\033[0m %s\n' "$1" >&2; }
die()  { printf '\033[1;31mERROR:\033[0m %s\n' "$1" >&2; exit 1; }

cleanup() {
  if [ "$CLONED" -eq 1 ] && [ -n "$BUILD_ROOT" ] && [ -d "$BUILD_ROOT" ]; then
    rm -rf "$BUILD_ROOT"
  fi
}
trap cleanup EXIT

require_sudo() {
  if [ "$(id -u)" -eq 0 ]; then
    echo ""
  elif command -v sudo >/dev/null 2>&1; then
    echo "sudo"
  else
    die "Root privileges needed and no sudo found. Re-run as root, install sudo, or set MCODE_INSTALL_DIR to a directory you own."
  fi
}

nproc_guess() {
  nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2
}

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------

build_cpr_from_source() {
  local sudo_cmd="$1"
  local cpr_dir
  cpr_dir="$(mktemp -d)"
  log "cpr isn't packaged here; building it from source..."
  git clone --depth 1 https://github.com/libcpr/cpr.git "$cpr_dir" >/dev/null 2>&1
  cmake -S "$cpr_dir" -B "$cpr_dir/build" -DCPR_USE_SYSTEM_CURL=ON -DCMAKE_BUILD_TYPE=Release >/dev/null
  cmake --build "$cpr_dir/build" -j"$(nproc_guess)" >/dev/null
  $sudo_cmd cmake --install "$cpr_dir/build" >/dev/null
  if command -v ldconfig >/dev/null 2>&1; then
    $sudo_cmd ldconfig || true
  fi
  rm -rf "$cpr_dir"
}

install_deps_macos() {
  if ! command -v brew >/dev/null 2>&1; then
    die "Homebrew not found. Install it from https://brew.sh, then re-run this script."
  fi
  log "Installing build dependencies via Homebrew (cmake, nlohmann-json, cpr)..."
  brew install cmake nlohmann-json cpr >/dev/null
}

install_deps_linux() {
  local sudo_cmd

  if command -v apt-get >/dev/null 2>&1; then
    sudo_cmd="$(require_sudo)"
    log "Installing build dependencies via apt..."
    $sudo_cmd apt-get update -y >/dev/null
    $sudo_cmd apt-get install -y cmake git build-essential pkg-config \
      libcurl4-openssl-dev nlohmann-json3-dev >/dev/null
    if ! $sudo_cmd apt-get install -y libcpr-dev >/dev/null 2>&1; then
      build_cpr_from_source "$sudo_cmd"
    fi

  elif command -v dnf >/dev/null 2>&1; then
    sudo_cmd="$(require_sudo)"
    log "Installing build dependencies via dnf..."
    $sudo_cmd dnf install -y cmake git gcc-c++ make libcurl-devel json-devel >/dev/null
    build_cpr_from_source "$sudo_cmd"

  elif command -v pacman >/dev/null 2>&1; then
    sudo_cmd="$(require_sudo)"
    log "Installing build dependencies via pacman..."
    $sudo_cmd pacman -Sy --noconfirm --needed cmake git base-devel curl nlohmann-json >/dev/null
    if ! $sudo_cmd pacman -Sy --noconfirm --needed cpr >/dev/null 2>&1; then
      build_cpr_from_source "$sudo_cmd"
    fi

  elif command -v zypper >/dev/null 2>&1; then
    sudo_cmd="$(require_sudo)"
    log "Installing build dependencies via zypper..."
    $sudo_cmd zypper --non-interactive install cmake git gcc-c++ make \
      libcurl-devel nlohmann_json-devel >/dev/null
    build_cpr_from_source "$sudo_cmd"

  else
    die "No supported package manager found (apt-get, dnf, pacman, zypper). Install cmake, a C++20 compiler, nlohmann-json and cpr manually, then re-run."
  fi
}

# ---------------------------------------------------------------------------
# Fetch + build
# ---------------------------------------------------------------------------

resolve_source_dir() {
  # Running as ./install.sh from an existing checkout: build it in place
  # instead of re-cloning.
  local script_dir=""
  if [ -n "${BASH_SOURCE:-}" ]; then
    script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
  fi
  if [ -n "$script_dir" ] && [ -f "$script_dir/CMakeLists.txt" ] && [ -d "$script_dir/src" ]; then
    echo "$script_dir"
    return
  fi

  BUILD_ROOT="$(mktemp -d)"
  CLONED=1
  log "Cloning mcode ($REF)..." >&2
  git clone --depth 1 --branch "$REF" "$REPO_URL" "$BUILD_ROOT/mcode" >/dev/null 2>&1
  echo "$BUILD_ROOT/mcode"
}

install_binary() {
  local bin_path="$1"
  local target_dir="$INSTALL_DIR"

  if [ ! -w "$target_dir" ] && [ "$(id -u)" -ne 0 ]; then
    if command -v sudo >/dev/null 2>&1 && sudo -n true 2>/dev/null; then
      : # passwordless sudo available, fall through to sudo install below
    elif ! command -v sudo >/dev/null 2>&1; then
      target_dir="$HOME/.local/bin"
      warn "$INSTALL_DIR isn't writable and there's no sudo; installing to $target_dir instead."
      mkdir -p "$target_dir"
    fi
  fi

  mkdir -p "$target_dir" 2>/dev/null || true

  if [ -w "$target_dir" ] || [ "$(id -u)" -eq 0 ]; then
    install -m 755 "$bin_path" "$target_dir/$BINARY_NAME"
  else
    log "Installing to $target_dir (needs sudo)..."
    sudo install -m 755 "$bin_path" "$target_dir/$BINARY_NAME"
  fi

  INSTALL_DIR="$target_dir"
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

main() {
  cat <<'BANNER'
  __  __  ____ ___  ____  _____
 |  \/  |/ ___/ _ \|  _ \| ____|
 | |\/| | |  | | | | | | |  _|
 | |  | | |__| |_| | |_| | |___
 |_|  |_|\____\___/|____/|_____|

BANNER

  local os
  os="$(uname -s)"
  case "$os" in
    Darwin) install_deps_macos ;;
    Linux)  install_deps_linux ;;
    *) die "Unsupported OS: $os. mcode currently supports macOS and Linux." ;;
  esac

  local src_dir
  src_dir="$(resolve_source_dir)"
  log "Building from $src_dir"

  log "Configuring..."
  cmake -S "$src_dir" -B "$src_dir/build" -DCMAKE_BUILD_TYPE=Release >/dev/null

  log "Compiling (this can take a minute)..."
  cmake --build "$src_dir/build" -j"$(nproc_guess)"

  local bin_path="$src_dir/build/$BINARY_NAME"
  [ -f "$bin_path" ] || die "Build finished but no $BINARY_NAME binary at $bin_path"

  install_binary "$bin_path"

  echo
  log "mcode installed to $INSTALL_DIR/$BINARY_NAME"

  case ":$PATH:" in
    *":$INSTALL_DIR:"*) ;;
    *)
      warn "$INSTALL_DIR is not on your PATH. Add this to your shell profile:"
      echo "    export PATH=\"$INSTALL_DIR:\$PATH\""
      ;;
  esac

  echo
  echo "Run it with: $BINARY_NAME"
}

main "$@"
