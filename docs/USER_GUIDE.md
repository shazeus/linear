# User guide

The current Windows development effect is installable with:

```powershell
scripts\install-dev.ps1
```

Restart Premiere Pro, open a project, and search Video Effects for `Linear`.
The current host effect provides target color, threshold, minimum region area,
bounding boxes, center markers, source/overlay/mask output, independent visual
styles, and Stabilization Mode. `Legacy Raw` is the compatibility default.
`Basic Smooth` checks out the preceding input frame
and reduces tracked box/marker position and size jitter. These standard
controls are keyframeable where Premiere exposes animation for the
corresponding type.

Effect controls are arranged into ten native collapsible Adobe groups:
Detection, Tracking & Smoothing, Output, Network, Labels, Region Fill,
Measurement HUD, Technical Paths, Colors, and Performance. Detection opens
initially; the advanced groups start collapsed to keep the panel compact.
`05 LABELS & CALLOUTS` is further organized into `Content & Typography`,
`Panel Appearance`, and `Placement & Leaders` so related controls stay together.

Every visual color is always independent. Open `09 COLORS` to set:

- Box stroke and center marker.
- Network lines and network arrows.
- Grid lines and node projections.
- Label text, panel, and leader lines.
- Fill primary and secondary colors.
- Technical paths.

Opacity, thickness, size, route, and layout controls remain in their own
feature groups. No mode switch is needed for independent colors. The old
global color/style parameters remain hidden only so existing projects can be
opened safely.

The master `Overlay Opacity` still multiplies every section, which makes it
possible to fade the complete visualization with one keyframe.

Network lines can use a continuous `Dashed Lines` pattern across curved and
multi-segment routes. `Dash Length` and `Gap Length` set the cadence.
`Arrowheads` adds bounded end arrows with independent color, opacity, and size
at all times. Labels expose `Panel Padding`, `Offset X`, and `Offset Y` without
disabling collision avoidance or leader routing.

To draw the detected regions as a network:

1. Set `Connection Mode` to `Sequential`, `Nearest`, or
   `Minimum Spanning Tree`.
2. Use `Maximum Connections` to cap edge count and rendering cost.
3. Set its color in `09 COLORS`; thickness and opacity stay in `04 NETWORK`.

`Sequential` follows deterministic region order, `Nearest` connects each node
to its nearest neighbor without duplicate edges, and `Minimum Spanning Tree`
connects all available nodes with a deterministic acyclic tree.
`Star` builds a confidence-led hub, `Full Mesh` connects every pair up to the
hard edge cap, and `Radius (Proximity)` draws only local links within
`Connection Radius`. `Connection Style` provides Straight, Rounded Arc, Smooth
Bend, Step Bend, and PCB Trace. `Arc Curvature` controls bow for curved modes,
the corner position for Step Bend, and lead length for PCB Trace. All routes
use deterministic segment budgets; Straight remains the compatibility default.

Stabilization modes:

- `Basic Smooth`: predictable low-latency position and size damping.
- `Adaptive Smooth`: suppresses low-confidence jitter but follows genuine fast
  motion more aggressively.
- `Cinematic Smooth`: centered two-past/two-future temporal weighting for the
  cleanest floating-camera style motion.
- `Predictive Smooth`: velocity-aware causal filtering with bounded look-ahead
  and overshoot protection.

`Position Smoothing` and `Size Smoothing` control the two geometry channels
independently. Higher values produce stronger damping.

`Capture Persistence` bridges short color-confidence dropouts with asymmetric
attack/release filtering. New detections appear immediately while missing
confidence decays over two prior frames. Start around 35-60%; very high values
can intentionally leave longer trails. The default is 0% for compatibility.

`Detection Source` connects motion masks directly to the same region/tracking
pipeline. `Motion` tracks changing pixels, `Color + Motion` accepts either
signal, and `Color AND Motion` keeps only target-colored pixels that are also
moving. `Motion Threshold` and `Motion Softness` control sensitivity;
`Motion Cleanup` removes isolated noise. `Scene Cut Guard` suppresses a false
full-frame detection after a cut. Enable `Camera Compensation` for small camera
translations and bound its search with `Maximum Camera Shift`.

The host also exposes previously core-only controls for RGB/HSV/Lab detection,
invert and transparent-pixel handling, pre/post blur, morphology, box and
marker shapes, grid divisions, orbit/contour paths, and path repeaters.
Marker Shape now includes Triangle, Star, Diamond, Target, and Chevron in
addition to Ring, Dot, Plus, and Cross.
`Path Mode` inside `08 TECHNICAL PATHS` must be set to `Orbit` or `Contour`
before a path repeater can be visible.

For the reference-style measurement view, enable `Grid` in
`07 MEASUREMENT HUD` and choose `Cartesian`. This draws the outer frame,
center axes, ticks, and -100..100 values. `Node Projections` adds independently
colored dashed horizontal/vertical guides from each detected center to the
axes. `Simple` keeps the lighter division-only grid.

## Labels and reference-inspired fills

Open `05 LABELS & CALLOUTS` and use `Label Content` to enable panels for node
ID, detected area, dimensions, track ID, confidence, or custom text. Labels use bounded
four-candidate placement, skip collisions, and can draw leader lines back to
the detected region. `Label Limit` is both a visual-density and performance
budget.

For free custom label text, choose `Label Content > Custom Text`, click
`Text & Font > Edit Label...`, type directly into `Label text`, select any
installed Windows font (or keep `Linear Technical (Built-in)`), then press
`Apply`. No clipboard step is used. UTF-8 text and the selected font are stored
with the effect instance and survive project save/reopen. The built-in bitmap
font keeps its uppercase technical style; installed fonts preserve their own
letterforms and case. Panel fill/border/shadow settings are under
`Panel Appearance`; offsets and leader controls are under
`Placement & Leaders`. Their colors remain independently adjustable in
`09 COLORS` as Label Text, Label Panel Fill, Label Border, Label Shadow, and
Label Leaders.

The `Mode` control inside `06 REGION FILL` provides Solid, Hatch, Crosshatch,
classic Invert, a cool luminance-driven X-Ray LUT, and Duotone treatments. Fill
is strictly clipped to each selected `Box Shape`: Rectangle and Corner Brackets
use the region bounds, while Ellipse uses the actual curved ellipse interior.
It does not spread over the raw color-detection mask and overlapping regions do
not accumulate opacity.

`Fill Primary` in `09 COLORS` controls Solid/Hatch and the bright Duotone
endpoint; `Fill Secondary` controls the dark Duotone endpoint. `Box Inset` shrinks the
fill inward, while `Edge Feather` softens inward from the selected shape's
boundary, including the curved Ellipse edge. Hatch
and Crosshatch expose independent spacing, width, and angle controls. `Invert
Strength` blends from the original pixels to a photographic negative. At 100%,
Invert is deliberately fully visible inside the regions even when the general
Fill Opacity is zero or remains at its 35% default; lowering Invert Strength
restores a softer mix. Invert, X-Ray, and Duotone sample the actual source frame
in both Source + Overlay and Overlay Only output modes.
Fill and labels default Off.

## After Effects performance

Open `10 PERFORMANCE` and use `Analysis Resolution`:

- `Full`: maximum detection and contour fidelity at the highest CPU cost.
- `Half`: production default for new instances and recommended for 1080p/4K
  preview and most exports.
- `Quarter`: dense 4K analysis, Cinematic smoothing, or interactive setup.

Only detection, morphology, and component analysis are reduced. Final source
compositing, labels, lines, and output encoding remain at full resolution.
Contour extraction follows the analysis resolution and scales its geometry back
to full-resolution coordinates, avoiding a second full-frame mask traversal.
Multi-Frame Rendering is advertised because shared analysis results are
immutable, bounded, and synchronized; host-owned frame pointers are never
cached. Existing projects retain their serialized resolution choice; switch old
`Full` instances to `Half` manually when maximum contour fidelity is not
required. Restart After Effects after installing a new build.

The core library is also developer-facing:

1. Decode a checked frame through `decodePixels`.
2. Run `detectColor`, `detectMotion`, or `combineMasks`.
3. Clean the confidence mask and call `connectedComponents`.
4. Track an explicit time-keyed frame window with `trackFrameWindow`.
5. Stabilize tracked regions or fixed-topology paths through
   `stabilizeFrameWindow` or `stabilizePathWindow`.
6. Generate optional orbital paths, mask contours, and repeaters through
   `PathGeometry`.
7. Render the detection mask or overlay with `renderOverlay`.
8. Encode to the negotiated host buffer through `encodePixels`.

Source-crop data tiles, presets,
and complete workflow instructions remain future host features. Automatic
technical labels, fills, grids, paths, and connection networks are available
now.
