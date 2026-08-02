# Implementation status

Updated: 2026-08-01

## Completed

- Repository and Windows toolchain audit.
- Central product configuration; product name is `Linear`.
- CMake presets for Windows core, Windows Adobe, and macOS core.
- Dependency-free C++17 image, mask, color, region, motion, tracking, graphics,
  label-template, pixel-adapter, and host-capability modules.
- CPU raster output for source-plus-overlay, overlay-only alpha, and detection
  mask.
- Automated core test executable and CTest registration.
- Dependency-free CPU benchmark executable.
- SDK validation and clear configuration failure for a missing/invalid SDK.
- Native Effect SDK adapter, PiPL resource, and keyframeable standard controls
  for target color, threshold, minimum area, boxes, markers, output mode,
  overlay color, stroke width, opacity, and append-only Stabilization Mode.
- Deterministic Legacy Raw and Basic Smooth stabilization for region position,
  size, and fixed-topology paths, with official previous-frame host checkout.
- Deterministic orbital/arc paths, outer mask contours, and arc-length path
  repeaters with ring, box, arrow, and tick stroke primitives.
- Host-visible Sequential, Nearest, and Minimum Spanning Tree connection
  networks with an explicit edge cap.
- Host-visible RGB/HSV/Lab, alpha, blur, morphology, box/marker shape, grid,
  orbit/contour, and repeater controls using append-only disk IDs 11-28.
- Predictive multi-cue tracking with deterministic short-dropout reacquisition.
- Adaptive, centered Cinematic, and bounded Predictive smoothing with
  mode-specific multi-frame host windows and independent position/size strength.
- Star, Full Mesh, and Radius network topologies with hard edge/distance limits.
- Deterministic bounded network routes with host-controlled curvature.
- Smooth Bend, Step Bend, and PCB Trace routes plus independent Box, Marker,
  Network, Grid, Label text/panel/leader, and Technical Path style channels.
- Continuous network dash/gap patterns, independently styled arrowheads, and
  label panel padding/X/Y offset controls.
- Optional asymmetric temporal Capture Persistence for short confidence
  dropouts, with zero as the compatibility default.
- Technical bitmap and installed Windows font rasterization, panels, leader
  lines, collision-aware placement, UTF-8 text, and bounded label density.
- Persistent direct Write + Apply custom label text/font selection plus rounded panels with
  independent border/shadow color, opacity, thickness, radius, and offset.
- Host-visible Motion, Color+Motion, and Color-AND-Motion analysis feeding the
  same deterministic region tracking pipeline, with scene-cut protection,
  cleanup, and optional camera-translation compensation.
- Triangle, Star, Diamond, Target, and Chevron marker primitives appended
  without changing the four legacy popup values.
- Box-confined Solid, Hatch, Crosshatch, Invert, X-Ray LUT, and Duotone fills
  with ROI-only unioned overlap coverage, two colors, invert strength, inset,
  feather, angle, spacing, and width.
- Full/Half/Quarter analysis resolution with full-resolution output and Adobe
  Multi-Frame Rendering advertisement.
- Ten native collapsible Adobe parameter groups with a dedicated Colors
  section, three focused nested Label groups, advanced sections initially
  collapsed, and stable creative-control disk IDs.
- Always-independent section colors and a Cartesian measurement HUD with
  frame, center axes, values, ticks, and dashed node projections.
- Fused RGB threshold classification, cache-friendly separable morphology,
  low-memory flood component extraction, sampled historical-frame decode,
  no-op mask-upscale elimination, move-based source compositing, optimized
  pixel quantization, bilinear line rasterization, adaptive ellipse
  tessellation, squared-distance graph/tracking gates, and Release LTCG/IPO.
- Bounded 16-entry/128 MiB content-addressed analysis cache with immutable
  results and single-flight MFR publication for style edits and overlapping
  temporal windows.
- Feature-level analysis/checkout gating, direct host copy/clear for empty
  overlays, descriptor-free component extraction, and reduced-resolution
  contour extraction with full-resolution geometry scaling.
- Development installation and packaging scripts.

## In progress

- Smart Render/32-bpc host adapter and longer temporal windows.
- Full creative modules and standard-host parameter UI.

## Blocked

- After Effects is not installed on the audited machine.
- A timeline render/application test in Premiere Pro 2026 remains pending.

## Verified

- Visual Studio Build Tools 2022 17.14.34.
- MSVC 19.44.35228.
- Windows SDK 10.0.26100.0.
- CMake 4.3.3.
- Premiere Pro 2026 installation detected.
- Debug core build completes under `/W4 /WX`.
- All 43 core test cases pass through CTest in Debug and Release, including
  every Region Fill mode, rectangle/ellipse shape confinement, Cartesian grid modes, and
  independent projection color, installed-font UTF-8 rasterization, plus
  optimized/reference morphology and RGB detector equivalence.
- Official After Effects 25.6 Windows SDK obtained through Adobe Developer
  Console and kept outside source control.
- `Linear.aex` Release target compiles and links.
- Installed at
  `C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore\Linear\Linear.aex`.
- Premiere Pro 2026 `Plugin Loading.log` reports the binary was recognized by
  the effect loader.
- The current packaged release has SHA-256
  `ACAB2BD3C60CFC12E81574F5C8660C218895F8FE30C6B19047163A8A7DEA61CE`.
- The always-independent Colors section, five-route decorated Network,
  Cartesian HUD, label layout, and fully tested shape-confined Fill build is
  installed in MediaCore and byte-identical to the package.
- A clean Premiere launch lists the updated installed `Linear.aex` as a loaded
  process module; the validation session was then closed without opening a
  project.
- Missing-SDK configuration failure was exercised and contains the expected
  `ADOBE_AE_SDK_ROOT` guidance.
- Release benchmark executed; measured results are recorded in `PERFORMANCE.md`.

## Not yet verified

- Effect menu search, application to footage, frame rendering, parameter
  keyframing, project persistence, runtime temporal checkout, and Smart Render.
- After Effects, Media Encoder, macOS, signing, and notarization.
- Golden images, production footage, 16-bpc host convention, HDR host behavior,
  performance targets, and in-host MFR stress testing.
- Complete UI, arbitrary Unicode/system-font labels, source-crop data tiles,
  full preset catalog, and GPU acceleration.

## Build commands used

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug --parallel
cmake --preset windows-adobe
cmake --build --preset windows-adobe-release --parallel
```

## Test commands used

```powershell
ctest --preset windows-debug
build\windows-debug\Debug\linear_tests.exe
ctest --test-dir build\windows-adobe -C Release --output-on-failure
```

## Known issues

- CPU morphology is separable and cache-friendly; reusable scratch-buffer
  pooling remains deferred pending in-host memory-pressure profiling.
- Translation compensation currently estimates integer translation only.
- Tracking and smoothing consume explicit ordered host frame windows. Analysis
  within overlapping windows is content-cached, but host checkout itself still
  occurs for every frame required by the selected smoothing mode.
- Labels use a dependency-free technical bitmap font. Arbitrary Unicode and
  system-font selection are not yet supported.
- The host effect currently renders 8-bpc and 16-bpc legacy worlds. The shared
  core supports float, but the host must add and validate Smart Render before
  advertising 32-bpc.
