# Building ChromaDesk

## Supported targets

ChromaDesk's source is shared between Windows and macOS. The supported baseline
is Qt 6.5+ (Widgets, Concurrent, Network), CMake 3.24+, and a C++20 compiler.

| Target | Toolchain | Minimum deployment target |
| --- | --- | --- |
| Windows x64 | Visual Studio/MSVC 2022 | Windows 10 1809 |
| Windows ARM64 | MSVC 2022 | Windows 11 ARM64; script adjustment required |
| macOS Apple Silicon | Xcode 15+ / Apple Clang | macOS 13 |
| macOS Intel | Xcode 15+ / Apple Clang | macOS 13 |

These targets follow the current Qt 6 desktop support matrix. Always match the
Qt package architecture to the compiler target.

The app runs on macOS 13, while accurate native Subject/Background segmentation
activates on macOS 14 or later. macOS 13 and Windows use the portable local
fallback. Apple Vision is part of macOS and requires no separate package.

## Dependency model

Qt is installed separately because its distribution and licence are product
decisions. LibRaw is optional and is declared in `vcpkg.json`. Passing the
vcpkg CMake toolchain activates manifest mode and makes RAW support available to
the build. Without LibRaw, CMake prints a status message and builds a fully
working editor for standard image formats.

## Manual Windows build

Use a **Developer PowerShell for VS 2022**:

```powershell
$QtRoot = "C:\Qt\6.8.3\msvc2022_64"
$VcpkgRoot = "C:\src\vcpkg"

cmake -S . -B build/windows -G "Ninja Multi-Config" `
  "-DCMAKE_PREFIX_PATH=$QtRoot" `
  "-DCMAKE_TOOLCHAIN_FILE=$VcpkgRoot\scripts\buildsystems\vcpkg.cmake"
cmake --build build/windows --config Release --parallel
ctest --test-dir build/windows -C Release --output-on-failure
```

Deploy the runtime:

```powershell
New-Item -ItemType Directory -Force dist/windows
Copy-Item build/windows/Release/ChromaDesk.exe dist/windows/
& "$QtRoot\bin\windeployqt.exe" --release --compiler-runtime --no-translations dist/windows
```

The supplied `scripts/build-windows.ps1` performs all these steps and creates a
ZIP package.

## Manual macOS build

For an Apple Silicon Mac using Homebrew Qt, the shortest supported path is:

```bash
xcode-select -p
brew install qt cmake ninja
chmod +x scripts/build-macos.sh
./scripts/build-macos.sh /opt/homebrew/opt/qt arm64
```

If `xcode-select -p` prints a Command Line Tools directory, do not force it to
`/Applications/Xcode.app/Contents/Developer` unless the full Xcode application
is actually installed at that path. The supplied build script stages plugins,
audits bundled dependencies, removes Finder metadata, signs the local test app,
runs tests, installs the native `.icns` bundle icon, and creates the ZIP under
`dist/`. The ZIP also contains `create-macos-shortcut.command`; double-clicking
it installs the app to the current user's Applications folder when necessary
and creates a safe Desktop shortcut without replacing an existing item.

The source package also has a root-level `Install-ChromaDesk.command` for a
guided first installation. It checks the Apple toolchain and Homebrew packages,
runs the tested build, installs to the current user's Applications folder,
backs up an existing version, creates the Desktop shortcut, and launches the
app. The same build emits a drag-to-Applications DMG under `dist/`.

For a Qt Online Installer layout or a custom/manual build:

```bash
export QT_ROOT="$HOME/Qt/6.8.3/macos"
export VCPKG_ROOT="$HOME/src/vcpkg"   # optional

cmake -S . -B build/macos -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$QT_ROOT" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build/macos --parallel
ctest --test-dir build/macos --output-on-failure
"$QT_ROOT/bin/macdeployqt" build/macos/ChromaDesk.app
```

### Signing and notarization

The build script applies an ad-hoc signature for local testing. Public macOS
distribution requires an Apple Developer ID, hardened runtime signing,
notarization, and stapling. A typical release pipeline runs:

```bash
codesign --deep --force --options runtime --timestamp \
  --sign "Developer ID Application: YOUR COMPANY (TEAMID)" ChromaDesk.app
xcrun notarytool submit ChromaDesk.zip --keychain-profile chromadesk-notary --wait
xcrun stapler staple ChromaDesk.app
```

Do not place signing certificates, passwords, API keys, or notarization profiles
in the repository.

### Optional Generative Fill runtime setup

Generative Fill uses the OpenAI Image API and is the only feature that requires
network access. No API key is required to build, test, or use the local editing
tools. At runtime, ChromaDesk first checks `OPENAI_API_KEY`; if it is unset, the
app asks for a key in a password-style field. In both cases the key is kept only
for the process lifetime. Never add it to CMake files, shell scripts, the source
tree, or a packaged application.

## Common issues

### Qt package not found

Set `CMAKE_PREFIX_PATH` to the directory containing `lib/cmake/Qt6`, not to the
Qt installer root. On Windows this is normally `C:\Qt\VERSION\msvc2022_64`;
on macOS it is normally `$HOME/Qt/VERSION/macos`.

### RAW files report that LibRaw is unavailable

Pass the vcpkg toolchain during the first CMake configuration. CMake caches
dependency discovery, so remove only the platform-specific build folder and
configure again if the toolchain was added later.

### An image format is missing

Qt discovers many formats through runtime plugins. Run `windeployqt` or
`macdeployqt`; do not ship only the executable. The deployment tools copy the
needed image-format and platform plugins.

### macOS app is blocked

Local development builds can be opened from Finder with Control-click → Open.
Customer distribution should use signing and notarization instead.
