# Missing features

This is an execution backlog derived from `FEATURE_AUDIT.md`, not a product
redesign. Existing serialized values and default rendering are frozen.

## Stage 1 — stabilization compatibility (implemented; host visual check pending)

1. [x] Append `Stabilization Mode` after disk ID 9; default `Legacy Raw`.
2. [x] Preserve exact raw region position and size.
3. [x] Implement deterministic Basic Smooth over an explicit time-keyed window.
4. [x] Smooth position and size independently without mutable render history.
5. [x] Feed stabilized regions to derived boxes and markers; path endpoints use
   the same explicit-window filter in the core.
6. [x] Test a synthetic one-pixel jitter sequence: raw preserves it and Basic
   Smooth reduces both position and size jitter.
7. [x] Wire the mode through official temporal input checkout without retaining
   host-owned memory.
8. [x] Install the updated binary and verify that Premiere's loader recognizes
   and maps it into the running process.
9. [ ] Exercise visible smoothing in a Premiere timeline and save/reopen a
   project containing both popup values.

## Stage 2 — advanced deterministic smoothing

- [x] Adaptive Smooth with motion/confidence-sensitive gain.
- [x] Cinematic Smooth with centered temporal weighting.
- [x] Predictive Smooth with velocity extrapolation and overshoot protection.
- Independent rotation, contour, label, path, and endpoint smoothing.
- [x] Short lost-track reacquisition with deterministic time keys.
- [ ] Explicit split/merge lineage and long-occlusion appearance modeling.

## Stage 3 — node and geometry

- [x] Initial dedicated deterministic outer-contour extraction/simplification
  with hard contour/point limits.
- [x] Elliptical orbital and partial-arc geometry with rotation and hard
  tessellation limits.
- [x] Host/raster integration for orbital and outer-contour paths.
- [ ] Contour holes, subpixel fitting, and convex hull.
- Rotated rectangle, circle, ellipse, and shape moments.
- Node sources: centers, weighted centroids, bounds, contour samples, corners,
  skeleton points, optical-flow features, custom points.
- Uniform spatial index with hard node/query limits.

## Stage 4 — graph and route architecture

- Move graph construction out of the monolithic graphics file.
- [x] Implement None, configurable Sequential, Nearest, Star, Full Mesh, MST,
  and Radius.
- KNN, Horizontal, Vertical, Delaunay, Cluster, Track, and Hybrid remain.
- [x] Implement distinct Straight, Rounded Arc, Smooth Bend, Step Bend, and PCB
  Trace routes.
- Advanced obstacle-aware orthogonal and user-handle spline routes remain.
- Add obstacle routing, decorations, dash patterns, arrows, trims, and stable
  time-based animation.

## Stage 5–6 — technical paths

- [x] Core orbital and mask-contour path sources.
- [ ] Silhouette, motion trajectory, and flow paths.
- [x] Core arc-length repeaters made from real ring, box, arrow, and tick
  strokes with optional tangent alignment and hard instance limits.
- [x] Host controls and raster wiring for ring, box, arrow, and tick repeaters.
- [ ] Chain links, labels, and traveling elements.
- Curved dashes, tangent-correct arrows, path bundling, and strict iteration
  limits.

## Stage 7–8 — dense systems

- [x] Dependency-free technical bitmap font rasterization, panels, leader
  lines, collision avoidance, and density limits.
- [ ] System-font loading, Unicode shaping, and advanced dense-label routing.
- Micro boxes, clusters, nested regions, depth illusion, and mask occlusion.
- Stable/reactive glitch, technical textures, and style-density mappings.
- Presets only after every referenced control is implemented.

## Stage 9 — validation and release

- [x] Selectable Full/Half/Quarter detection downsampling with full-resolution
  output.
- [x] Bounded MFR-safe content-addressed analysis cache with single-flight
  publication for overlapping temporal windows.
- Scratch-buffer pooling, broader spatial indices, and host cancellation.
- Golden stills and short sequences across bit depths and preview scales.
- Premiere timeline apply/render/save/reopen test.
- After Effects and Media Encoder test when available.
- [x] Invocation-local render-state audit and MFR advertisement.
- Signed/notarized packaging only when credentials and platform exist.
