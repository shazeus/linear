# Render pipeline map

## Current Adobe CPU path

```text
PF_Cmd_RENDER
  -> plugin::render
    -> decodeInput
      -> PF_Pixel8 or PF_Pixel16
      -> straight-alpha linear::Image
    -> immutable local ColorDetectionSettings
    -> detectColor
      -> alpha-safe unpremultiplication guard
      -> RGB confidence
      -> threshold softness
      -> morphology open + close
    -> immutable local RegionFilter
    -> connectedComponents
      -> union-find labels
      -> bounds / centroids / area / perimeter / averages / confidence
      -> area filter and deterministic sort
    -> when Stabilization Mode is Basic Smooth
      -> PF_CHECKOUT_PARAM for the preceding input time
      -> decode and analyze the preceding frame with the same snapshot
      -> trackFrameWindow(previous, current)
      -> stabilizeFrameWindow(BasicSmooth)
      -> PF_CHECKIN_PARAM before returning
    -> immutable local OverlayStyle
    -> renderOverlay
      -> output-mode initialization
      -> optional grid (host currently leaves disabled)
      -> optional connections (host currently leaves disabled)
      -> boxes
      -> markers
      -> alpha compositing
    -> encodeOutput
      -> premultiply
      -> PF_Pixel8 or PF_Pixel16 respecting rowbytes
```

Every object above is local to one invocation. No writable global render state
or global previous frame exists.

## Implemented temporal core path

```text
caller-supplied vector<FrameRegions>
  -> stable sort by timeKey
  -> deterministic first-frame IDs
  -> per-adjacent-frame IoU + centroid-distance candidates
  -> deterministic greedy association
  -> track ID / age / velocity
  -> vector<FrameRegions>
```

The Adobe adapter uses an explicit mode-specific window: one past frame for
Basic, two past for Adaptive, three past for Predictive, and two past plus two
future frames for centered Cinematic. Every successful checkout is checked in
before return; unavailable edge-of-clip samples are skipped safely.

When `Capture Persistence` is nonzero, at least two past detection masks are
also checked out. Confidence attacks immediately and releases by the selected
percentage before connected components are rebuilt, reducing single-frame
misses without mutable global history.

## Stage 1 stabilization

```text
explicit tracked FrameRegions window
  -> stabilizeFrameWindow(settings)
       LegacyRaw: bit-for-bit geometry pass-through
       BasicSmooth: deterministic per-track position/size filter
  -> stabilized region vector at requested timeKey
  -> node/label/path-anchor derivation
  -> graph/path generation
  -> rasterization
```

Smoothing operates on descriptors copied out of host frames. No host-owned
pixel pointer is retained. The requested frame result depends only on the
explicit window, settings, seed, and time key.

## Host-visible path geometry

```text
region bounds -> makeOrbitalPath -> ellipse or partial arc
detection mask -> extractContourPaths -> one bounded outer path per component
Polyline -> repeatAlongPath -> bounded arc-length/tangent instances
RepeaterInstance -> buildRepeaterGlyph -> ring/box/arrow/tick strokes
```

The Adobe snapshot now selects Off, Orbit, or Contour, rasterizes the selected
paths, and optionally expands them into bounded ring, box, arrow, or tick
repeaters. Creative defaults remain Off. A global 512-instance cap prevents
unbounded repeater work.

## Intentionally unexposed partial modules

- Motion detection needs a complete current/previous/earlier-frame checkout
  policy when combined with stabilization.
- Label templates have no font rasterizer or collision/layout system.
- Additional graph and route names remain unexposed because their distinct
  algorithms do not yet exist.

None of these partial modules has a host control that silently does nothing.

## Error path

```text
bad_alloc -> copy source -> PF_Err_OUT_OF_MEMORY
recoverable algorithm/render exception -> copy source -> PF_Err_NONE
invalid effect boundary state -> translated PF_Err
```

Overlay-only source-copy behavior during a recovered internal failure is still
an open policy decision; the current stability-first behavior predates the new
modules and is recorded rather than silently changed.
