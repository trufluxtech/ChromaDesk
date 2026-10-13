#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
SOURCE_APP="${1:-}"

if [[ -z "$SOURCE_APP" ]]; then
  for candidate in \
    "/Applications/ChromaDesk.app" \
    "$HOME/Applications/ChromaDesk.app" \
    "$SCRIPT_DIR/ChromaDesk.app" \
    "$PROJECT_ROOT/dist/macos/ChromaDesk.app" \
    "$PROJECT_ROOT/build/macos/ChromaDesk.app"; do
    if [[ -d "$candidate" ]]; then
      SOURCE_APP="$candidate"
      break
    fi
  done
fi

if [[ -z "$SOURCE_APP" || ! -d "$SOURCE_APP" ]]; then
  echo "ChromaDesk.app was not found. Build ChromaDesk first, then run this file again."
  echo "You may also pass the full path to ChromaDesk.app as the first argument."
  if [[ -t 0 ]]; then read -r -p "Press Return to close…"; fi
  exit 2
fi

INSTALLED_APP="$SOURCE_APP"
case "$SOURCE_APP" in
  /Applications/*|"$HOME/Applications/"*)
    ;;
  *)
    USER_APPLICATIONS="$HOME/Applications"
    INSTALLED_APP="$USER_APPLICATIONS/ChromaDesk.app"
    /bin/mkdir -p "$USER_APPLICATIONS"
    if [[ ! -e "$INSTALLED_APP" ]]; then
      COPYFILE_DISABLE=1 /usr/bin/ditto --norsrc --noextattr --noqtn \
        "$SOURCE_APP" "$INSTALLED_APP"
    fi
    ;;
esac

DESKTOP_DIR="$HOME/Desktop"
SHORTCUT_PATH="$DESKTOP_DIR/ChromaDesk.app"
if [[ -e "$SHORTCUT_PATH" || -L "$SHORTCUT_PATH" ]]; then
  echo "A Desktop item already exists at: $SHORTCUT_PATH"
  echo "Nothing was overwritten."
  if [[ -t 0 ]]; then read -r -p "Press Return to close…"; fi
  exit 0
fi

/bin/ln -s "$INSTALLED_APP" "$SHORTCUT_PATH"
echo "ChromaDesk is installed at: $INSTALLED_APP"
echo "Desktop shortcut created at: $SHORTCUT_PATH"
/usr/bin/open -R "$SHORTCUT_PATH" 2>/dev/null || true
if [[ -t 0 ]]; then read -r -p "Press Return to close…"; fi

