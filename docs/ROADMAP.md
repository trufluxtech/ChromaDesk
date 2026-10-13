# Product Roadmap

## Release 0.2 — Core desktop editor and layers (implemented)

Library import, non-destructive adjustments, history, crop/rotate, presets,
comparison views, ratings, histogram, optional RAW decoding, and JPEG/PNG/TIFF
export on Windows and macOS. Adds ordered non-destructive adjustment layers with
independent values, names, visibility, opacity, duplication and reordering.

## Release 0.3 — Local masks and AI-assisted filters (implemented)

- Non-destructive Subject, Background, Sky, Color Range, Luminance Range,
  Rectangle, and Ellipse masks with editable feathering
- Content-aware Auto Enhance and editable Subject Pop, Background Blur, and
  Color Subject filter layers
- Editable Enhance Sky and masked colour-replacement layers
- Explicit save-edits, save-copy, export, and format-conversion workflows
- Nondestructive editable text layers and tabbed Develop controls
- A dedicated Filters tab with 50 categorized, editable creative looks and a
  File & Export tab that consolidates open, save, export, convert, and passport output
- Toolbar zoom controls, active-layer tint/boundary visualization, draggable
  canvas guides, configurable Photo Library views, and three persistent UI themes
- Icon-only library view controls with hide/restore, movable text/image
  watermarks with transparency in Tools, a persistent logo/version header, and
  configurable white-by-default image borders
- Editor-only rulers and composition grids, constrained crop-ratio presets,
  and 300 DPI 35 × 45 mm passport-photo grids
- Accurate Apple Vision Subject/Background segmentation on macOS 14+, with
  cached local processing, connected-sky foreground exclusion, and no photo upload
- Persistent mask recipes, layer opacity/order, undo/redo, preview and export

## Release 0.4 — Retouch and opt-in generative editing (implemented)

- Local Dust / Spot Heal with nearby texture and colour matching
- Local two-click Clone Stamp with editable brush size
- Canvas target overlays and one non-destructive layer per retouch operation
- Marked-rectangle Generative Fill through OpenAI `gpt-image-2`
- Explicit upload notice, session-only API key handling, and app-data result storage
- Qt Network integration plus heal/clone serialization and render tests

## Release 0.5 — Professional imaging foundation

- 16-bit integer and 32-bit floating-point tile pipeline
- Scene-linear working space with OpenColorIO or equivalent transform layer
- Display ICC soft proofing and wide-gamut/HDR awareness
- SQLite catalog, schema migrations, thumbnail/proxy cache, backups
- Lens distortion, chromatic aberration, vignetting, and perspective tools
- Better highlight reconstruction and camera profiles
- GPU compute abstraction over Direct3D 12 and Metal
- Background export queue and batch synchronization

## Release 0.6 — Advanced local adjustments and model inference

- Brush, linear gradient, radial gradient, and mask add/subtract/intersect
- Optional ONNX semantic segmentation for sky, face, hair, clothing and objects
- AI denoise, super-resolution, auto tone, and distraction removal
- All inference available locally; cloud processing is opt-in
- Mask provenance, confidence view, and before/after evaluation fixtures

## Release 0.7 — Asset workflow

- Smart collections, keywords, people, places, duplicate detection, and search
- Sidecar XMP interoperability and metadata conflict handling
- Versions, virtual copies, stacks, color labels, and compare/survey modes
- Watched folders, removable-volume relinking, and catalog integrity checks
- Plugin SDK with a stable C ABI boundary

## Release 1.0 — Commercial readiness

- Crash recovery and edit journal
- Performance budgets for 100k+ catalogs and 100 MP source files
- Accessibility audit and full keyboard workflow
- Signed Windows installer, signed/notarized macOS package, differential updates
- Privacy model, dependency SBOM, security review, telemetry consent, and support diagnostics
- Golden-image render tests across CPU/GPU and platform combinations

## Suggested team

| Capability | Initial staffing |
| --- | --- |
| C++ imaging/platform engineers | 3–4 |
| GPU/color-science engineer | 1–2 |
| Desktop product/UI engineer | 1–2 |
| Imaging ML engineer | 1–2 from Release 0.3 |
| Product designer | 1 |
| QA/performance automation | 1–2 |
| Product manager / photography domain lead | 1 |

A serious Lightroom-class product is a multi-year imaging platform, not only a
desktop UI. The MVP deliberately proves the workflow and component boundaries
before investing in the high-cost color, GPU, catalog, AI, and distribution layers.
