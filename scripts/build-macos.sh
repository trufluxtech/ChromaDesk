#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 /path/to/Qt/6.x.x/macos [arm64|x86_64|arm64;x86_64]"
  exit 2
fi

QT_ROOT="$1"
ARCHITECTURES="${2:-$(uname -m)}"
PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build/macos"
DIST_DIR="$PROJECT_ROOT/dist/macos"

clear_codesign_attributes() {
  local target="$1"

  # First clear recursively, then visit every entry and its symlink itself.
  # Homebrew frameworks contain symlinked version directories on which Finder
  # metadata can otherwise survive a single recursive xattr invocation.
  /usr/bin/xattr -cr "$target" 2>/dev/null || true
  while IFS= read -r -d '' item; do
    /usr/bin/xattr -c "$item" 2>/dev/null || true
    /usr/bin/xattr -cs "$item" 2>/dev/null || true
  done < <(/usr/bin/find "$target" -print0)

  local disallowed
  disallowed="$(/usr/bin/xattr -lr "$target" 2>/dev/null \
    | /usr/bin/grep -E 'com\.apple\.(FinderInfo|ResourceFork)' || true)"
  if [[ -n "$disallowed" ]]; then
    echo "Unable to remove metadata rejected by codesign:" >&2
    echo "$disallowed" >&2
    return 1
  fi
}

audit_bundle_dependencies() {
  local app_bundle="$1"
  local frameworks="$app_bundle/Contents/Frameworks"
  local failed=0

  while IFS= read -r -d '' candidate; do
    if ! /usr/bin/file -b "$candidate" 2>/dev/null | /usr/bin/grep -q 'Mach-O'; then
      continue
    fi

    # otool -L lists a framework/dylib's own LC_ID_DYLIB before its real
    # dependencies. Homebrew paths are valid as an object's identity; only a
    # different object referring to that path would escape the bundle.
    local install_names
    install_names="$(/usr/bin/otool -D "$candidate" 2>/dev/null \
      | /usr/bin/awk 'NR > 1 { print $1 }' || true)"

    while IFS= read -r dependency; do
      if /usr/bin/grep -Fqx "$dependency" <<<"$install_names"; then
        continue
      fi

      case "$dependency" in
        @rpath/*)
          if [[ ! -e "$frameworks/${dependency#@rpath/}" ]]; then
            echo "Unresolved dependency in $candidate: $dependency" >&2
            failed=1
          fi
          ;;
        /opt/homebrew/*|/usr/local/opt/*|/usr/local/Cellar/*)
          echo "Homebrew dependency escaped the app bundle in $candidate: $dependency" >&2
          failed=1
          ;;
      esac
    done < <(/usr/bin/otool -L "$candidate" 2>/dev/null \
      | /usr/bin/awk 'NR > 1 { print $1 }')
  done < <(/usr/bin/find "$app_bundle/Contents" -type f -print0)

  return "$failed"
}

if [[ ! -x "$QT_ROOT/bin/macdeployqt" ]]; then
  echo "Qt path is invalid: $QT_ROOT/bin/macdeployqt was not found"
  exit 2
fi

EXTRA_ARGS=(-DCHROMADESK_BUILD_TESTS=ON)
if [[ -n "${VCPKG_ROOT:-}" && -f "$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" ]]; then
  EXTRA_ARGS+=("-DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake")
else
  EXTRA_ARGS+=(-DCHROMADESK_ENABLE_RAW=OFF)
  echo "Note: LibRaw disabled. Set VCPKG_ROOT to include camera RAW support."
fi

cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$QT_ROOT" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
  -DCMAKE_OSX_ARCHITECTURES="$ARCHITECTURES" \
  "${EXTRA_ARGS[@]}"
cmake --build "$BUILD_DIR" --parallel
ctest --test-dir "$BUILD_DIR" --output-on-failure

rm -rf "$DIST_DIR"
mkdir -p "$DIST_DIR"

# Deploy and sign outside Desktop. Finder can attach com.apple.FinderInfo while
# a bundle on Desktop is being assembled, which makes codesign reject it.
STAGE_DIR="$(mktemp -d "${TMPDIR:-/tmp}/chromadesk-deploy.XXXXXX")"
trap 'rm -rf "$STAGE_DIR"' EXIT
APP_BUNDLE="$STAGE_DIR/ChromaDesk.app"
COPYFILE_DISABLE=1 /usr/bin/ditto --norsrc --noextattr --noqtn \
  "$BUILD_DIR/ChromaDesk.app" "$APP_BUNDLE"

clear_codesign_attributes "$APP_BUNDLE"

DEPLOY_ARGS=(-always-overwrite -no-codesign "-libpath=$QT_ROOT/lib")

# Homebrew splits Qt and its transitive dependencies across many formula
# prefixes. macdeployqt does not discover all of those rpaths automatically,
# so provide every installed Qt dependency's lib directory explicitly.
if command -v brew >/dev/null 2>&1; then
  HOMEBREW_PREFIX="$(brew --prefix)"
  case "$QT_ROOT" in
    "$HOMEBREW_PREFIX"/*)
      while IFS= read -r dependency; do
        [[ -z "$dependency" ]] && continue
        dependency_prefix="$(brew --prefix "$dependency" 2>/dev/null || true)"
        if [[ -n "$dependency_prefix" && -d "$dependency_prefix/lib" ]]; then
          DEPLOY_ARGS+=("-libpath=$dependency_prefix/lib")
        fi
      done < <(brew deps --installed --formula qt 2>/dev/null || true)

      # Homebrew's Qt meta-formula uses compatibility symlinks for split Qt
      # modules. Some versions omit those module prefixes from `brew deps`, so
      # add the modules that macdeployqt commonly discovers through plugins.
      for dependency in qtbase qtsvg qtvirtualkeyboard qtpdf; do
        dependency_prefix="$(brew --prefix "$dependency" 2>/dev/null || true)"
        if [[ -n "$dependency_prefix" && -d "$dependency_prefix/lib" ]]; then
          DEPLOY_ARGS+=("-libpath=$dependency_prefix/lib")
        fi
      done
      ;;
  esac
fi

# Homebrew exposes a shared plugin tree containing PDF, SVG-icon and virtual
# keyboard plugins even though ChromaDesk does not use those modules. Deploy a
# curated set so macdeployqt never scans their unavailable split frameworks.
QT_PLUGIN_DIR=""
if [[ -x "$QT_ROOT/bin/qtpaths" ]]; then
  QT_PLUGIN_DIR="$("$QT_ROOT/bin/qtpaths" --query QT_INSTALL_PLUGINS 2>/dev/null || true)"
elif [[ -x "$QT_ROOT/bin/qmake" ]]; then
  QT_PLUGIN_DIR="$("$QT_ROOT/bin/qmake" -query QT_INSTALL_PLUGINS 2>/dev/null || true)"
fi

if [[ ! -d "$QT_PLUGIN_DIR" ]]; then
  for candidate in "$QT_ROOT/share/qt/plugins" "$QT_ROOT/plugins"; do
    if [[ -d "$candidate" ]]; then
      QT_PLUGIN_DIR="$candidate"
      break
    fi
  done
fi

if [[ ! -d "$QT_PLUGIN_DIR" ]]; then
  echo "Unable to locate the Qt plugin directory under $QT_ROOT" >&2
  exit 2
fi

PLUGIN_EXECUTABLE_ARGS=()
copy_qt_plugin() {
  local relative_path="$1"
  local requirement="${2:-optional}"
  local source_path="$QT_PLUGIN_DIR/$relative_path"
  local destination_path="$APP_BUNDLE/Contents/PlugIns/$relative_path"

  if [[ ! -f "$source_path" ]]; then
    if [[ "$requirement" == "required" ]]; then
      echo "Required Qt plugin was not found: $source_path" >&2
      exit 2
    fi
    return
  fi

  /bin/mkdir -p "$(/usr/bin/dirname "$destination_path")"
  COPYFILE_DISABLE=1 /usr/bin/ditto --norsrc --noextattr --noqtn \
    "$source_path" "$destination_path"
  PLUGIN_EXECUTABLE_ARGS+=("-executable=$destination_path")
}

copy_qt_plugin "platforms/libqcocoa.dylib" required
copy_qt_plugin "styles/libqmacstyle.dylib"
for image_plugin in \
  libqgif.dylib libqicns.dylib libqico.dylib libqjpeg.dylib \
  libqmacheif.dylib libqwbmp.dylib libqjp2.dylib libqmng.dylib \
  libqtga.dylib libqtiff.dylib libqwebp.dylib; do
  copy_qt_plugin "imageformats/$image_plugin"
done

# Generative Fill uses HTTPS through Qt Network. Because deployment is otherwise
# deliberately restricted with -no-plugins, include at least one real TLS
# backend explicitly (the certificate-only backend cannot establish TLS).
TLS_BACKEND_FOUND=0
for tls_plugin in libqsecuretransportbackend.dylib libqopensslbackend.dylib; do
  if [[ -f "$QT_PLUGIN_DIR/tls/$tls_plugin" ]]; then
    copy_qt_plugin "tls/$tls_plugin" required
    TLS_BACKEND_FOUND=1
  fi
done
copy_qt_plugin "tls/libqcertonlybackend.dylib"
if [[ "$TLS_BACKEND_FOUND" -ne 1 ]]; then
  echo "No usable Qt TLS backend was found under $QT_PLUGIN_DIR/tls" >&2
  exit 2
fi

"$QT_ROOT/bin/macdeployqt" "$APP_BUNDLE" "${DEPLOY_ARGS[@]}" \
  -no-plugins "${PLUGIN_EXECUTABLE_ARGS[@]}"

clear_codesign_attributes "$APP_BUNDLE"
audit_bundle_dependencies "$APP_BUNDLE"

# Local development package: apply an ad-hoc signature after all frameworks
# and plugins have been copied. Commercial distribution should replace this
# with Developer ID signing and notarization.
/usr/bin/codesign --force --deep --sign - "$APP_BUNDLE"
/usr/bin/codesign --verify --deep --strict --verbose=2 "$APP_BUNDLE"

# Copy only the completed signed bundle back to Desktop, discard any metadata
# added during that copy, and verify the exact bundle that will be zipped.
DIST_APP="$DIST_DIR/ChromaDesk.app"
COPYFILE_DISABLE=1 /usr/bin/ditto --norsrc --noextattr --noqtn \
  "$APP_BUNDLE" "$DIST_APP"
clear_codesign_attributes "$DIST_APP"
/usr/bin/codesign --verify --deep --strict --verbose=2 "$DIST_APP"

SHORTCUT_INSTALLER="$DIST_DIR/create-macos-shortcut.command"
COPYFILE_DISABLE=1 /usr/bin/ditto --norsrc --noextattr --noqtn \
  "$PROJECT_ROOT/scripts/create-macos-shortcut.command" "$SHORTCUT_INSTALLER"
/bin/chmod +x "$SHORTCUT_INSTALLER"

(cd "$DIST_DIR" && COPYFILE_DISABLE=1 zip -qry \
  "$PROJECT_ROOT/dist/ChromaDesk-macOS-$ARCHITECTURES.zip" \
  ChromaDesk.app create-macos-shortcut.command)
echo "Package: $PROJECT_ROOT/dist/ChromaDesk-macOS-$ARCHITECTURES.zip"
echo "After unzipping, double-click create-macos-shortcut.command to install the app and create a Desktop shortcut."

PACKAGE_VERSION="$(/usr/bin/awk -F= \
  '/^CMAKE_PROJECT_VERSION:STATIC=/{print $2; exit}' "$BUILD_DIR/CMakeCache.txt")"
PACKAGE_ARCHITECTURES="${ARCHITECTURES//;/-}"
DMG_ROOT="$STAGE_DIR/dmg-root"
/bin/mkdir -p "$DMG_ROOT"
COPYFILE_DISABLE=1 /usr/bin/ditto --norsrc --noextattr --noqtn \
  "$DIST_APP" "$DMG_ROOT/ChromaDesk.app"
/bin/ln -s /Applications "$DMG_ROOT/Applications"
DMG_PATH="$PROJECT_ROOT/dist/ChromaDesk-$PACKAGE_VERSION-macOS-$PACKAGE_ARCHITECTURES.dmg"
/usr/bin/hdiutil create -volname "ChromaDesk $PACKAGE_VERSION" \
  -srcfolder "$DMG_ROOT" -ov -format UDZO "$DMG_PATH"
echo "DMG installer: $DMG_PATH"
