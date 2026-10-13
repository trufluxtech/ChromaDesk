#!/bin/bash
set -euo pipefail

pause_before_close() {
  if [[ -t 0 ]]; then read -r -p "Press Return to close…"; fi
}

fail() {
  echo
  echo "Installation stopped: $1" >&2
  pause_before_close
  exit 1
}

if [[ "$(uname -s)" != "Darwin" ]]; then
  fail "This installer must be run on macOS."
fi

INSTALLER_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$INSTALLER_DIR"

echo "ChromaDesk macOS Installer"
echo "==========================="
echo

if ! /usr/bin/xcode-select -p >/dev/null 2>&1; then
  echo "Apple Command Line Tools are required. macOS will now open their installer."
  /usr/bin/xcode-select --install || true
  fail "Complete the Command Line Tools installation, then double-click this installer again."
fi

if ! command -v brew >/dev/null 2>&1; then
  echo "Homebrew is required to install Qt, CMake, and Ninja."
  echo "Install it from https://brew.sh and then run this installer again."
  fail "Homebrew was not found."
fi

MISSING_FORMULAE=()
for formula in qt cmake ninja; do
  if ! brew list --versions "$formula" >/dev/null 2>&1; then
    MISSING_FORMULAE+=("$formula")
  fi
done

if [[ ${#MISSING_FORMULAE[@]} -gt 0 ]]; then
  echo "Required components are missing: ${MISSING_FORMULAE[*]}"
  read -r -p "Install them with Homebrew now? [y/N] " approval
  case "$approval" in
    y|Y|yes|YES)
      brew install "${MISSING_FORMULAE[@]}"
      ;;
    *)
      fail "Required components were not installed."
      ;;
  esac
fi

ARCHITECTURE="$(uname -m)"
case "$ARCHITECTURE" in
  arm64|x86_64) ;;
  *) fail "Unsupported Mac architecture: $ARCHITECTURE" ;;
esac

QT_ROOT="$(brew --prefix qt)"
/bin/chmod +x scripts/build-macos.sh scripts/create-macos-shortcut.command

echo
echo "Building ChromaDesk for $ARCHITECTURE. This may take several minutes…"
./scripts/build-macos.sh "$QT_ROOT" "$ARCHITECTURE"

BUILT_APP="$INSTALLER_DIR/dist/macos/ChromaDesk.app"
[[ -d "$BUILT_APP" ]] || fail "The build completed without producing ChromaDesk.app."

USER_APPLICATIONS="$HOME/Applications"
TARGET_APP="$USER_APPLICATIONS/ChromaDesk.app"
/bin/mkdir -p "$USER_APPLICATIONS"

INSTALL_STAGE="$(/usr/bin/mktemp -d "${TMPDIR:-/tmp}/chromadesk-install.XXXXXX")"
trap '/bin/rm -rf "$INSTALL_STAGE"' EXIT
STAGED_APP="$INSTALL_STAGE/ChromaDesk.app"
COPYFILE_DISABLE=1 /usr/bin/ditto --norsrc --noextattr --noqtn \
  "$BUILT_APP" "$STAGED_APP"
/usr/bin/codesign --verify --deep --strict --verbose=2 "$STAGED_APP"

BACKUP_APP=""
if [[ -e "$TARGET_APP" ]]; then
  BACKUP_APP="$USER_APPLICATIONS/ChromaDesk.app.backup-$(/bin/date +%Y%m%d-%H%M%S)"
  /bin/mv "$TARGET_APP" "$BACKUP_APP"
  echo "Previous installation backed up at: $BACKUP_APP"
fi

if ! /bin/mv "$STAGED_APP" "$TARGET_APP"; then
  if [[ -n "$BACKUP_APP" && -e "$BACKUP_APP" ]]; then
    /bin/mv "$BACKUP_APP" "$TARGET_APP"
  fi
  fail "The application could not be installed."
fi

DESKTOP_SHORTCUT="$HOME/Desktop/ChromaDesk.app"
if [[ ! -e "$DESKTOP_SHORTCUT" && ! -L "$DESKTOP_SHORTCUT" ]]; then
  /bin/ln -s "$TARGET_APP" "$DESKTOP_SHORTCUT"
  echo "Desktop shortcut created: $DESKTOP_SHORTCUT"
else
  echo "Desktop shortcut already exists; it was not changed."
fi

echo
echo "ChromaDesk was installed successfully at: $TARGET_APP"
PACKAGE_VERSION="$(/usr/bin/awk -F= \
  '/^CMAKE_PROJECT_VERSION:STATIC=/{print $2; exit}' \
  "$INSTALLER_DIR/build/macos/CMakeCache.txt")"
DMG_PATH="$INSTALLER_DIR/dist/ChromaDesk-$PACKAGE_VERSION-macOS-$ARCHITECTURE.dmg"
if [[ -f "$DMG_PATH" ]]; then
  echo "Reusable DMG installer created at: $DMG_PATH"
fi

/usr/bin/open "$TARGET_APP"
pause_before_close
