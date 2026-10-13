# ChromaDesk 0.4.2

[![Cross-platform build](https://github.com/trufluxtech/ChromaDesk/actions/workflows/build.yml/badge.svg)](https://github.com/trufluxtech/ChromaDesk/actions/workflows/build.yml)

ChromaDesk is a modern, non-destructive desktop photo editor written in C++20.
It provides a Lightroom-inspired editing workflow while remaining an independent
implementation with its own interface, processing engine, catalog format, and
visual identity.

![ChromaDesk logo](resources/logo.svg)

## What works in this MVP

- Cross-platform Qt desktop interface for Windows and macOS
- Add individual photos, import folders recursively, or drag folders onto the app
- Persistent local photo catalog with non-destructive edit state
- Non-destructive adjustment layers with per-layer values, visibility, opacity,
  rename, duplicate, delete, and reorder controls
- Editable text layers with multiline content, font, size, colour, outline,
  nine-position placement, visibility, opacity, ordering, undo/redo and export
- Text or image watermark layers with adjustable transparency, size, styling,
  ordering, and direct drag-to-position interaction on the photo canvas
- Adjustable image borders from 0–20% in any colour, defaulting to white
- Local Dust / Spot Heal with automatic nearby-texture selection and a local
  two-click Clone Stamp; each repair is a removable, reorderable layer with
  editable brush size and opacity
- Mark a rectangular area and use prompt-based Generative Fill through OpenAI
  `gpt-image-2`; upload consent is explicit, the API key is session-only, and
  the result is a feathered non-destructive layer
- Accurate local Subject and Background segmentation through Apple Vision on
  macOS 14+, plus a connected Sky selector and deterministic Color Range,
  Luminance Range, Rectangle, and Ellipse masks
- One-click AI Auto Enhance, Subject Pop, Background Blur, Color Subject, and
  Enhance Sky filters; every result remains an editable layer
- Select a source colour and replace it nondestructively while preserving image
  shading; source tolerance, replacement colour, strength, and opacity stay editable
- Background thumbnail and preview rendering
- Exposure, contrast, highlights, shadows, whites, and blacks
- Temperature, tint, vibrance, saturation, clarity, and masked blur
- Interactive crop, 90-degree rotation, fit view, and 100% view
- Crop presets for original ratio, 1:1, 4:5, 35:45 passport, 3:2, 16:9,
  and A4 portrait, with constrained interactive selection
- Optional percentage rulers and rule-of-thirds, 4 × 4, and golden-ratio grids;
  guides remain editor-only and never appear in exports
- Draggable custom guides: pull horizontal guides from the top ruler and
  vertical guides from the left ruler, with one-click clearing
- Fit, zoom out, 100%, and zoom in controls in the main toolbar
- Active-layer visualization with a mild cyan mask tint and thin broken boundary;
  this editor-only overlay is never rendered into saved files
- Switch the Photo Library using icon-only Icon/List controls, resize thumbnails
  with a persistent slider, and hide or restore the library from the eye button,
  View menu, or main toolbar
- Persistent Gray, High Contrast, and Colorful interface themes under Preferences
- One-click 35 × 45 mm passport-photo sheets at 300 DPI: six photos on
  4 × 6-inch paper or 25 photos on A4
- Develop controls grouped into Layers, AI Masks, Adjust, Tools, Filters, and
  File & Export tabs; text/image Watermark controls are grouped inside Tools
- Persistent ChromaDesk logo and exact version number above the Develop panel,
  with the same version also shown in the application title bar
- A high-resolution transparent PNG logo and native multi-resolution macOS
  application icon, plus a packaged Desktop-shortcut installer
- 50 nondestructive creative filters across Cinematic, Film, Portrait,
  Landscape, and Black & White collections; every filter creates a separate
  editable layer
- Undo/redo with merged slider history
- Edited, original, split, and side-by-side comparison views
- Live RGB histogram
- Pick flags and 0–5 star ratings
- Five quick presets plus 50 creative filter looks
- Save edits explicitly, save an edited copy, export, or convert to full-resolution
  JPEG, PNG, and TIFF with optional resizing; all file operations are also
  available together in the File & Export tab
- Branded two-second startup splash and shared code-drawn application icon
- Optional camera RAW decoding through LibRaw
- Native Windows and macOS deployment scripts
- Unit tests for edit/mask/text/watermark/border/retouch serialization, complementary
  Subject/Background masks, masked color/luminance/shape composition, rendering,
  local dust healing, clone stamping, passport print dimensions, cropping,
  rotation, and source immutability

The source photo is never written to. Catalog edits are saved as JSON in the
operating system's per-user application-data folder.

## Technology

| Area | Choice |
| --- | --- |
| Language | C++20 |
| Desktop UI | Qt Widgets and Qt Network 6.5+ |
| Build | CMake 3.24+ and Ninja |
| Concurrency | Qt Concurrent |
| Smart masking | Apple Vision foreground segmentation on macOS 14+; local fallback and sky classifier; no photo upload |
| Generative Fill | Opt-in OpenAI Image API edit using `gpt-image-2` |
| Standard image I/O | Qt image plugins |
| Camera RAW | Optional LibRaw |
| Color handling | Embedded ICC input converted to sRGB through Qt |
| Catalog | Atomic versioned JSON with layer-stack migration; no modification of originals |
| Tests | Qt Test |

Qt 6.11 is the current documented desktop line, but ChromaDesk intentionally
keeps a Qt 6.5 minimum to make deployment practical across enterprise machines.

## Quick start: Windows 10/11

Install:

1. Visual Studio 2022 with **Desktop development with C++**.
2. CMake and Ninja (both can be selected in the Visual Studio installer).
3. Qt 6.5 or later, **MSVC 2022 64-bit**, from the Qt installer.
4. Optional: vcpkg with `libraw` for camera RAW files.

In PowerShell:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\scripts\build-windows.ps1 `
  -QtRoot "C:\Qt\6.8.3\msvc2022_64" `
  -VcpkgRoot "C:\src\vcpkg"
```

Without vcpkg, omit `-VcpkgRoot`. The editor will still support JPEG, PNG,
TIFF, WebP, and the other formats exposed by the installed Qt image plugins.
The packaged application is written to `dist/ChromaDesk-Windows-x64.zip`.

## Quick start: macOS 13+

Install Xcode command-line tools, CMake, Ninja, and the desktop Qt package. Then:

```bash
chmod +x scripts/build-macos.sh
./scripts/build-macos.sh /opt/homebrew/opt/qt arm64
```

This command matches Apple Silicon Homebrew installations. A Qt Online
Installer path such as `$HOME/Qt/6.8.3/macos` also works. Set `VCPKG_ROOT`
before the command only when optional LibRaw camera-RAW support is required.

The macOS application ZIP contains `create-macos-shortcut.command`. After
unzipping, double-click it to install ChromaDesk under your user Applications
folder when needed and create a branded `ChromaDesk.app` shortcut on the
Desktop. It refuses to overwrite an existing Desktop item. You can also run:

```bash
./scripts/create-macos-shortcut.command
```

For the simplest first installation, unzip the source package and double-click
`Install-ChromaDesk.command` at its top level. It verifies Apple Command Line
Tools, offers to install missing Homebrew `qt`, `cmake`, and `ninja` packages,
builds and verifies the application, installs it to `~/Applications`, creates a
Desktop shortcut, and opens ChromaDesk. An existing installation is moved to a
timestamped backup before the new version is installed.

The build also creates a conventional
`dist/ChromaDesk-0.4.2-macOS-<architecture>.dmg`. Open that DMG and drag
ChromaDesk onto the Applications link. The development DMG is ad-hoc signed;
public distribution still requires Apple Developer ID signing and notarization.

For an Intel build, pass `x86_64`. A universal build requires a universal Qt
installation and can be requested with `'arm64;x86_64'`. The output is written
under `dist/` as a zipped `.app` bundle.

Detailed setup and troubleshooting are in [docs/BUILDING.md](docs/BUILDING.md).

### Generative Fill setup

Dust Heal and Clone Stamp need no account and run locally. Generative Fill is
optional and requires network access plus an OpenAI API key. The safest setup is
to enter the key in ChromaDesk's password-style prompt; it stays in memory only
until the app closes. A terminal launch can instead inherit a temporary key:

```bash
read -s OPENAI_API_KEY
export OPENAI_API_KEY
./build/macos/ChromaDesk.app/Contents/MacOS/ChromaDesk
```

Do not place API keys in this repository or the catalog.

## Project structure

```text
src/core/       Non-destructive state, catalog, image loading and processing
src/ui/         Qt desktop interface and export workflow
resources/      Theme and application identity
tests/          Unit tests
scripts/        Windows and macOS build/deployment automation
docs/           Architecture, build guide and roadmap
```

## Commercial use

The ChromaDesk application source in this package is provided as a development
baseline. Before distributing a commercial binary, select an appropriate Qt
licence and review the obligations of every linked dependency. See
[THIRD_PARTY.md](THIRD_PARTY.md). This is engineering guidance, not legal advice.

The ChromaDesk source is currently published for review as an all-rights-reserved
development baseline. See [NOTICE.txt](NOTICE.txt) before copying, modifying, or
redistributing it.

## AI features and privacy

All built-in selectors run locally and never upload a photograph. On macOS 14
or later, Subject and Background use Apple's native foreground-instance mask
request at the rendered image resolution; ChromaDesk caches the result and
applies editable feathering in the layer engine. Sky combines upper-edge region
analysis with the Vision foreground result to avoid selecting foreground people
and objects. On older macOS releases and Windows, Subject and Background retain
the portable saliency fallback, which is less precise than the native macOS path.
Color, luminance, and shape masks remain deterministic range masks.

Dust Heal and Clone Stamp also run locally. Generative Fill is the sole cloud
editing feature: only after the user accepts its notice, ChromaDesk sends a
maximum-1536-pixel copy of the current edited image, a same-size transparent
rectangle mask, and the user's prompt to OpenAI's Image API. The key is read
from `OPENAI_API_KEY` or requested in a password field and is never persisted by
ChromaDesk. The returned PNG is stored in the per-user application-data folder
and referenced by the catalog as a non-destructive layer. API usage may incur
charges under the user's OpenAI account.

## Important MVP boundaries

The current engine uses an 8-bit sRGB working preview and CPU processing. This
is suitable for demonstrating the complete product workflow. A production
Lightroom-class product should move to a scene-linear 16/32-bit processing graph,
GPU kernels, tile caches, lens profiles, brush/gradient masks, calibrated display
proofing, and a transactional catalog. The planned progression is documented in
[docs/ROADMAP.md](docs/ROADMAP.md).
