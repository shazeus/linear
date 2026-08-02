# Feature audit

Audit date: 2026-08-01
Product: Linear
Baseline: frozen creative IDs 1-41, UI topic IDs 42-59, append-only creative
IDs 60-128, and nested Label UI topic IDs 129-134

Status vocabulary is restricted to: **NOT PRESENT**, **UI ONLY**, **PARTIAL**,
**IMPLEMENTED**, **VERIFIED**, and **BLOCKED BY ENVIRONMENT**.

`VERIFIED` means an automated test or available-host check was actually run.
An algorithm existing in the shared core is not treated as a verified host
feature unless the Adobe parameter-to-visible-output chain was tested.

## Current host parameter chain

All 89 visible non-input creative parameters currently exposed by the Adobe
plug-in are read by `plugin/PluginEntry.cpp`, validated or clamped, passed to a
core algorithm or style snapshot, and affect output. Disk IDs 7, 8, and 68 are
hidden serialization-compatibility parameters and intentionally do not appear
in the UI or drive new renders. No disconnected visible host control was found.
Ten native collapsible Adobe groups organize these controls. Their start/end
rows use IDs 42-59 and 102/104; Labels adds nested topic rows 129-134. Topic
rows carry no render value and start collapsed except Detection.

| Feature | UI | Snapshot/read | Algorithm called | Visible output | Animation | CPU | GPU | AE tested | Premiere tested | Unit test | Golden | Docs | Status | Responsible files / remaining work |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Target color | Yes | Yes | Yes | Yes | Host value is keyframeable; timeline not exercised | 8/16 | No | No | Loader only | Yes | No | Yes | IMPLEMENTED | `plugin/PluginEntry.cpp`, `core/src/Detection.cpp`; host pixel test pending |
| Color threshold | Yes | Yes | Yes | Yes | Host timeline not exercised | 8/16 | No | No | Loader only | Yes | No | Yes | IMPLEMENTED | Same as target color |
| Minimum region area | Yes | Yes | Yes | Yes | Host timeline not exercised | 8/16 | No | No | Loader only | Yes | No | Yes | IMPLEMENTED | `core/src/Regions.cpp`; maximum/minimum dimension filters are not exposed |
| Bounding boxes | Yes | Yes | Yes | Yes | Toggle is keyframeable; timeline not exercised | 8/16 | No | No | Loader only | Overlay alpha test | No | Yes | IMPLEMENTED | Rectangle/corner-bracket/ellipse core exists; host exposes one current bracket style |
| Center markers | Yes | Yes | Yes | Yes | Toggle is keyframeable; timeline not exercised | 8/16 | No | No | Loader only | Overlay alpha test | No | Yes | IMPLEMENTED | Four marker primitives exist in core; host exposes current ring style |
| Output mode | Yes | Yes | Yes | Yes | Host timeline not exercised | 8/16 | No | No | Loader only | Mask/alpha tests | No | Yes | IMPLEMENTED | Source+overlay, overlay-only, detection mask |
| Legacy overlay color | Hidden | No | Compatibility storage only | No | Existing serialized ID retained | 8/16 | No | No | Loader only | N/A | No | Yes | IMPLEMENTED | ID 7 hidden; independent Colors controls replace it |
| Legacy stroke width | Hidden | No | Compatibility storage only | No | Existing serialized ID retained | 8/16 | No | No | Loader only | N/A | No | Yes | IMPLEMENTED | ID 8 hidden; section widths replace it |
| Overlay opacity | Yes | Yes | Yes | Yes | Host timeline not exercised | 8/16 | No | No | Loader only | Overlay alpha test | No | Yes | IMPLEMENTED | `core/src/Graphics.cpp` |
| Stabilization Mode | Yes | Yes | Yes | Yes when Basic Smooth is selected | Host timeline not exercised | 8/16 | No | No | Updated binary loader only | Updated binary loaded | Raw/smooth jitter and order tests | No | Yes | IMPLEMENTED | ID 10; default Legacy Raw; two-frame checkout in `plugin/PluginEntry.cpp` |
| Connection Mode / cap / radius | Yes | Yes | Sequential, Nearest, MST, Star, Full Mesh, Radius | Yes | Host timeline not exercised | 8/16 | No | No | No | Packaged binary; host visual check pending | Graph and pixel tests | No | Yes | IMPLEMENTED | IDs 11-12 and 31; default Off |
| Connection route | Yes | Yes | Straight / Rounded Arc / Smooth Bend / Step Bend / PCB Trace | Yes | Host timeline not exercised | 8/16 | No | No | No | Installed MediaCore binary | Route difference tests | No | Yes | IMPLEMENTED | IDs 60-61; bounded deterministic geometry |
| Per-section styles | Yes | Yes | Always-independent colors, opacity, widths, sizes, and panel/leader channels | Yes | Master opacity remains keyframeable | 8/16 | No | No | No | Packaged binary; host visual check pending | Six-color renderer isolation test | No | Yes | IMPLEMENTED | IDs 69-91 and 96/103; all colors collected under Colors; legacy selector hidden |
| Network decorations | Yes | Yes | Continuous dash/gap and bounded arrowheads | Yes | Host timeline not exercised | 8/16 | No | No | No | Installed MediaCore binary | Dash/arrow pixel test | No | Yes | IMPLEMENTED | IDs 92-98; default Off |
| Label layout | Yes | Yes | Content/Typography, Panel Appearance, Placement/Leaders; padding, offset, rounded fill, border, shadow, and collision placement | Yes | Host timeline not exercised | 8/16 | No | No | No | Packaged binary; updated host visual check pending | Label raster/style tests | No | Yes | IMPLEMENTED | IDs 99-101, 121-128, and UI topics 129-134 |
| Detection preprocessing | Yes | Yes | Color space, invert, alpha, blur, morphology | Yes | Host timeline not exercised | 8/16 | No | No | No | Packaged binary; host visual check pending | Color/mask tests | No | Yes | IMPLEMENTED | IDs 13-19; defaults preserve prior output |
| Box / marker shape | Yes | Yes | Three box and four marker renderers | Yes | Host timeline not exercised | 8/16 | No | No | No | Packaged binary; host visual check pending | Distinct-output pixel test | No | Yes | IMPLEMENTED | IDs 20-21 |
| Grid | Yes | Yes | Simple or Cartesian frame/axes/ticks/values plus node projections | Yes | Host timeline not exercised | 8/16 | No | No | No | Packaged binary; host visual check pending | Mode/projection pixel tests | No | Yes | IMPLEMENTED | IDs 22-24, 81-83, 103, 105-111; default Off |
| Technical paths / repeater | Yes | Yes | Orbit, contour, and four repeater glyphs | Yes | Host timeline not exercised | 8/16 | No | No | No | Packaged binary; host visual check pending | Geometry and raster tests | No | Yes | IMPLEMENTED | IDs 25-28; default Off; global 512-instance cap |
| Capture Persistence | Yes | Yes | Asymmetric temporal confidence filter | Yes when nonzero | Detection, regions, and derived graphics | Keyframeable | 8/16 | No | No | Packaged binary; host visual check pending | Dropout/recovery test | No | Yes | IMPLEMENTED | ID 32; default 0%; two-frame history |
| Analysis Resolution | Yes | Yes | Full/Half/Quarter direct sampled analysis | Yes | Full-resolution output | Keyframeable | 8/16 | No | No | Packaged Adobe binary | Resize/pipeline tests | No | Yes | IMPLEMENTED | ID 33; Half default for new instances; saved values remain stable |
| Labels | Yes | Yes | Technical bitmap or installed Windows font, direct UTF-8 Write + Apply editor, styled panels, leaders, collision placement | Yes when enabled | Yes | Styles keyframeable; text/font instance-persistent with v1 migration | 8/16 | No | No | Packaged binary; updated host visual check pending | Built-in/system-font and style pixel tests | No | Yes | IMPLEMENTED | IDs 34-38 and 120-134; default Off; 128 hard cap |
| Region-shape fills | Yes | Yes | Solid, Hatch, Crosshatch, Invert, X-Ray LUT, Duotone | Yes when enabled | ROI-only shape-confined union coverage; source-sampled transforms in both output modes | Keyframeable | 8/16 | No | No | Packaged binary; host visual check pending | All-mode distinction, exact RGB invert, zero-opacity/overlay-only invert, rectangle/ellipse containment, inset, and feather pixel tests | No | Yes | VERIFIED | IDs 39-41, 62-67, and 112; default Off |

## Detection and region analysis

| Feature | UI | Snapshot/read | Algorithm | Called | Visible | Animation | CPU | GPU | AE | Premiere | Unit | Golden | Docs | Status | Remaining work |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| RGB color distance | Threshold UI | Yes | Yes | Yes | Yes | Parameter-driven | Yes | No | No | Loader only | Yes | No | Yes | IMPLEMENTED | Host timeline image verification |
| HSV circular-hue distance | Color Space popup | Yes | Yes | Yes when selected | Yes | Parameter-driven | Yes | No | No | Packaged binary; host visual check pending | Yes | No | Yes | IMPLEMENTED | Host timeline image verification |
| Lab distance | Color Space popup | Yes | Yes | Yes when selected | Yes | Parameter-driven | Yes | No | No | Packaged binary; host visual check pending | Yes | No | Yes | IMPLEMENTED | Host timeline image verification |
| Alpha-safe color detection | Yes | Yes | Yes | Yes | Yes | Parameter-driven | Yes | No | No | Packaged binary; host visual check pending | Yes | No | Yes | IMPLEMENTED | IDs 15-16 |
| Pre/post blur | Yes | Yes | Yes | Called conditionally | Yes when nonzero | Parameter-driven | Yes | No | No | Packaged binary; host visual check pending | Blur test | No | Yes | IMPLEMENTED | IDs 17-18 |
| Morphological open/close | Yes | Yes | Yes | Yes when nonzero | Yes | Parameter-driven | Yes | No | No | Packaged binary; host visual check pending | Yes | No | Yes | IMPLEMENTED | ID 19 |
| Connected components | Indirect | Yes | Yes | Yes | Yes | N/A | Yes | No | No | Loader only | Yes | No | Yes | IMPLEMENTED | Rotated geometry/contours missing |
| Region descriptors | No | N/A | Bounds, centroid, weighted centroid, area, fill, perimeter, mean color/luma/confidence | Yes | Boxes/markers only | N/A | Yes | No | No | Loader only | Yes | No | Partial | PARTIAL | Shape moments, hull, contour, rotation, ellipse, skeleton |
| Multi-key color | No | No | No | No | No | No | No | No | No | No | No | No | No | NOT PRESENT | Implement after single-key control completion |
| Motion frame difference | Yes | Yes | Motion, Color+Motion, Color-AND-Motion with cut guard/cleanup/translation compensation | Legacy host temporal checkout | Feeds regions and deterministic tracking | Keyframeable | Yes | No | No | Packaged binary; timeline pending | Core pipeline tests | No | Yes | IMPLEMENTED | IDs 113-119 |
| Scene-cut protection | No | No | Yes | Core API/tests only | No | N/A | Yes | No | No | No | Yes | No | Yes | PARTIAL | Host temporal integration |
| Camera translation compensation | No | No | Integer translation | Core API only | No | N/A | Yes | No | No | No | Indirect | No | Yes | PARTIAL | Host integration, subpixel/affine quality modes |
| Optical flow | No | No | No | No | No | No | No | No | No | No | No | No | No | NOT PRESENT | Optional advanced detector |
| Include/exclude masks | No | No | No | No | No | No | No | No | No | No | No | No | No | NOT PRESENT | Safe host mask/matte design required |

## Tracking and stabilization

| Feature | UI | Snapshot/read | Algorithm | Called | Visible | Animation | CPU | GPU | AE | Premiere | Unit | Golden | Docs | Status | Remaining work |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Deterministic association | Indirect through stabilization | Yes | Explicit time-window greedy IoU/distance association | Multi-frame host path | Drives stabilized overlays | Parameter is keyframeable; timeline not exercised | Yes | No | No | Updated binary loaded | Yes | No | Yes | IMPLEMENTED | Content cache shares analysis across overlapping windows |
| Stable track IDs | Indirect through stabilization | Yes | Yes | Basic Smooth host path | Drives stabilized overlays | N/A | Yes | No | No | Updated binary loaded | Yes | No | Yes | IMPLEMENTED | Longer-window split/merge handling remains |
| Legacy Raw geometry | Default popup item | Yes | Detection geometry passes through unchanged | Yes | Yes | Parameter is keyframeable; timeline not exercised | Yes | No | No | Updated binary loaded | Exact-value jitter test | No | Yes | VERIFIED | Immutable compatibility default |
| Basic Smooth | Popup item | Yes | Deterministic per-track position and size filter | Yes | Boxes and markers use stabilized regions | Parameter is keyframeable; timeline not exercised | Yes | No | No | Updated binary loaded | Jitter, size, order, path endpoint tests | No | Yes | IMPLEMENTED | Host visual/timeline verification remains |
| Adaptive/Cinematic/Predictive smoothing | Yes | Yes | Motion/confidence gain, centered Gaussian window, bounded velocity prediction | Host render | Boxes, markers, networks, orbit anchors | Keyframeable | Yes | No | No | Packaged binary; host visual check pending | Distinct-mode sequence test | No | Yes | IMPLEMENTED | Host visual sequence verification remains |
| Position/size independent smoothing | Yes | Yes | Independent strength channels | Yes | Derived region geometry | Keyframeable | Yes | No | No | Packaged binary; host visual check pending | Yes | No | Yes | IMPLEMENTED | IDs 29-30 |
| Rotation/contour independent smoothing | No | No | Path point stabilization exists; rotation absent | Tests only | No | No | Yes | No | No | No | Path endpoint test | No | Yes | PARTIAL | Stage 2 |

## Graphics, graph, and paths

| Feature | UI | Snapshot/read | Algorithm | Called | Visible | Animation | CPU | GPU | AE | Premiere | Unit | Golden | Docs | Status | Remaining work |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Sequential graph | Yes | Yes | Open chain | Host render | Yes | Parameter-driven | Yes | No | No | Packaged binary; host visual check pending | Pixel test | No | Yes | IMPLEMENTED | Region-order sorting options remain |
| Nearest-neighbor graph | Yes | Yes | Yes | Host render | Yes | Parameter-driven | Yes | No | No | Packaged binary; host visual check pending | Yes | No | Yes | IMPLEMENTED | Spatial-index optimization remains |
| Minimum spanning tree | Yes | Yes | Deterministic Prim | Host render | Yes | Parameter-driven | Yes | No | No | Packaged binary; host visual check pending | Graph/pixel tests | No | Yes | IMPLEMENTED | Distance limit remains |
| Star, Full Mesh, Radius | Yes | Yes | Confidence hub, bounded pair mesh, distance gate | Host render | Yes | Keyframeable | Yes | No | No | Packaged binary; host visual check pending | Yes | No | Yes | IMPLEMENTED | Hard edge cap applies |
| KNN, H/V, Delaunay, Cluster, Track, Hybrid | No | No | No | No | No | No | No | No | No | No | No | No | No | NOT PRESENT | Required dedicated graph architecture |
| Straight connection path | No | Core style only | Yes | Optional core render | Not from host | No | Yes | No | No | No | Indirect | No | Partial | Dedicated path object/tests |
| Spline, PCB, Smooth Bend, Step, Orthogonal, technical route, Arc | No | No | No | No | No | No | No | No | No | No | No | No | No | NOT PRESENT | Required routers and visibly distinct goldens |
| Endpoint/decorations/dashes/arrows/line animation | No | No | No | No | No | No | No | No | No | No | No | No | No | NOT PRESENT | Required renderer/animation modules |
| Grid | Yes | Yes | Simple divisions or Cartesian frame/axes/ticks/values/projections | Host render | Yes | Parameter-driven | Yes | No | No | Packaged binary; host visual check pending | Yes | No | Yes | IMPLEMENTED | Projection count capped at 128 regions |
| Label template/parser and rasterizer | Preset and custom text modes plus Text & Font editor | Yes | Parser plus technical bitmap and installed Windows font renderers | Host render | Yes | Parameter-driven + persistent UTF-8 text/font family | Yes | No | No | Packaged binary; updated host visual check pending | Built-in and system-font pixel tests | No | Yes | IMPLEMENTED | Font/editor sequence payload v2 migrates legacy v1 text |
| Fills | Yes | Yes | Solid, Hatch, Crosshatch, Invert, X-Ray LUT, Duotone | Host render | Yes | Parameter-driven | Yes | No | No | Packaged binary; host visual check pending | All modes and rectangle/ellipse confinement | No | Yes | VERIFIED | ROI-only selected-shape coverage; advanced source tiles remain future work |
| Orbital paths | Yes | Yes | Ellipse generation with padding and tessellation cap | Host render | Yes | Parameter-driven | Yes | No | No | Packaged binary; host visual check pending | Yes | No | Yes | IMPLEMENTED | Rotation/sweep host controls remain |
| Mask contour paths | Yes | Yes | Deterministic four-connected cell-edge outer contours with simplification/caps | Host render | Yes | Parameter-driven | Yes | No | No | Packaged binary; host visual check pending | Yes | No | Yes | IMPLEMENTED | Holes and subpixel contours remain |
| Silhouette/flow/trajectory paths | No | No | No | No | No | No | No | No | No | No | No | No | No | NOT PRESENT | Stages 4â€“6 |
| Path repeaters/glyph chains | Yes | Yes | Arc-length placement, tangent alignment, ring/box/arrow/tick stroke geometry | Host render | Yes | Parameter-driven | Yes | No | No | Packaged binary; host visual check pending | Yes | No | Yes | IMPLEMENTED | Animation and chain links remain |
| Micro/nested analysis, depth, glitch, texture, density | No | No | No | No | No | No | No | No | No | No | No | No | No | NOT PRESENT | Stages 7â€“8 |

## Host, formats, build, and delivery

| Feature | UI | Snapshot/read | Algorithm | Called | Visible | Animation | CPU | GPU | AE | Premiere | Unit | Golden | Docs | Status | Remaining work |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Native PiPL/Effect entry | N/A | N/A | Yes | Yes | Effect registration | N/A | Yes | No | Blocked | Loaded | N/A | N/A | Yes | VERIFIED | After Effects unavailable |
| Premiere loader compatibility | N/A | N/A | Yes | Yes | Module load | N/A | Yes | No | N/A | Process module and loader log | N/A | N/A | Yes | VERIFIED | Timeline render still not verified |
| Host 8-bpc | N/A | N/A | Yes | Render path | Not timeline-tested | N/A | Yes | No | No | Loader only | Core conversion | No | Yes | IMPLEMENTED | Host image test |
| Host 16-bpc | N/A | N/A | Yes, Adobe 32768 convention | Render path | Not host-tested | N/A | Yes | No | No | No | Core 16-bit adapter uses 65535 generic convention | No | Yes | PARTIAL | Host golden test |
| Host 32-bpc float | N/A | N/A | Shared adapter only | Not host | No | N/A | Core only | No | No | No | Core conversion | No | Yes | PARTIAL | Smart Render pixel-format query and float iterate |
| Multi-Frame Rendering | N/A | N/A | Core has no mutable render globals | Threaded-render flag enabled | Host scheduling | N/A | Yes | No | No | Packaged binary; host visual check pending | Determinism tests | No | Yes | IMPLEMENTED | Host stress test remains |
| GPU backend | No | No | No | No | No | No | CPU fallback | No | No | No | No | No | Yes | NOT PRESENT | Remains disabled |
| Presets | No | No | No | No | No | No | N/A | N/A | No | No | No | No | Honest status doc | NOT PRESENT | Add only after controls exist |
| Packaging/install | Script | N/A | Yes | Updated build packaged and installed | Installed updated binary | N/A | N/A | N/A | No | Updated module loaded | SHA-256 equality | N/A | Yes | VERIFIED | Unsigned development package only |

## Audit findings

- No `TODO`, `FIXME`, mock-output, fake-control, or placeholder marker exists in
  tracked source.
- No existing host control is unread or disconnected.
- The current output popup has an explicit default mapping to Source + Overlay;
  invalid serialized values do not select an unimplemented mode.
- Marker and color-space switches enumerate every declared enum value. There is
  no declared mode silently falling back to another visual mode.
- Recoverable core render exceptions copy the source; this is an intentional
  stability fallback documented in the implementation status.
- Host float output is not advertised.
- The label parser remains core-only because no font rasterizer exists; it is
  intentionally not exposed as a disconnected host control.
