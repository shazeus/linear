# Parameter compatibility

## Frozen serialized identities

Parameter array index 0 is the Adobe input layer and has no plug-in disk ID.

| Legacy index | Enum | Disk ID | Serialized meaning | Default |
|---:|---|---:|---|---|
| 0 | `Input` | — | Source layer | Host supplied |
| 1 | `TargetColor` | 1 | Key color | RGB 255,32,32 |
| 2 | `Threshold` | 2 | Color threshold percentage | 18 |
| 3 | `MinimumArea` | 3 | Minimum connected-component pixels | 64 |
| 4 | `BoxesEnabled` | 4 | Bounding-box visibility | On |
| 5 | `MarkersEnabled` | 5 | Center-marker visibility | On |
| 6 | `OutputModeParam` | 6 | 1 source+overlay, 2 overlay-only, 3 mask | 1 |
| 7 | `StrokeColor` | 7 | Overlay stroke color | RGB 0,255,192 |
| 8 | `StrokeWidth` | 8 | Stroke width in pixels | 2 |
| 9 | `OverlayOpacity` | 9 | Global overlay opacity percentage | 100 |

The disk IDs, meanings, types, and defaults are immutable compatibility facts.
The legacy indices document the original flat 0.1.0 registration only. Runtime
array indices may move when non-value group rows are inserted; render code uses
the current enum layout, while Adobe project serialization uses the stable disk
IDs. Enum identifier spelling remains stable unless a controlled migration is
implemented.

## Append-only policy

- New creative controls receive the next unused disk ID.
- Topic start/end rows also receive unique append-only disk IDs and never reuse
  a creative control ID.
- No removed ID is reused.
- New creative modules default off.
- Stabilization defaults to `Legacy Raw` so old projects and new instances keep
  the 0.1.0 appearance.
- Invalid future enum values map to the compatibility-safe default and are
  surfaced in diagnostics when a diagnostic channel exists.
- Any arbitrary/sequence data receives an explicit version header and migration
  function before first use. The custom-label sequence block has magic/version
  fields and a fixed 64-byte payload.

## Implemented Stage 1 append

| Legacy index | Disk ID | Meaning | Compatibility default |
|---:|---:|---|---|
| 10 | 10 | Stabilization Mode: Raw / Basic / Adaptive / Cinematic / Predictive | Legacy Raw |

`Legacy Raw` is popup value 1 and `Basic Smooth` is popup value 2. Invalid
values resolve to `Legacy Raw`.

Position and size strength controls will be appended after ID 10 only when they
are wired into an actual host temporal snapshot. The core settings may expose
testable defaults before those host controls exist.

## Implemented host-connection append

| Legacy index | Disk ID | Meaning | Compatibility default |
|---:|---:|---|---|
| 11 | 11 | Connection Mode: Off / Sequential / Nearest / MST | Off |
| 12 | 12 | Maximum Connections | 128 |
| 13 | 13 | Color Space: RGB / HSV / Lab | RGB |
| 14 | 14 | Invert Detection | Off |
| 15 | 15 | Ignore Transparent Pixels | On |
| 16 | 16 | Alpha Threshold percentage | 1 |
| 17 | 17 | Pre Blur Radius | 0 |
| 18 | 18 | Post Blur Radius | 0 |
| 19 | 19 | Morphology Radius | 1 |
| 20 | 20 | Box Shape: Corner Brackets / Rectangle / Ellipse | Corner Brackets |
| 21 | 21 | Marker Shape: Ring / Dot / Plus / Cross / Triangle / Star / Diamond / Target / Chevron | Ring |
| 22 | 22 | Grid | Off |
| 23 | 23 | Grid Columns | 5 |
| 24 | 24 | Grid Rows | 5 |
| 25 | 25 | Technical Paths: Off / Orbit / Contour | Off |
| 26 | 26 | Orbit Padding | 4 |
| 27 | 27 | Path Repeater: Off / Ring / Box / Arrow / Tick | Off |
| 28 | 28 | Repeater Spacing | 16 |
| 29 | 29 | Position Smoothing percentage | 65 |
| 30 | 30 | Size Smoothing percentage | 65 |
| 31 | 31 | Connection Radius | 160 |
| 32 | 32 | Capture Persistence percentage | 0 |
| 33 | 33 | Analysis Resolution: Full / Half / Quarter | Half for new instances |
| 34 | 34 | Label Content: Off / Node ID / Area / Dimensions / Track ID / Confidence / Custom Text | Off |
| 35 | 35 | Label Scale | 2 |
| 36 | 36 | Maximum Labels | 32 |
| 37 | 37 | Label Panel Opacity percentage | 72 |
| 38 | 38 | Label Leader Lines | On |
| 39 | 39 | Fill Mode: Off / Solid / Hatch / X-Ray / Duotone / Crosshatch / Invert | Off |
| 40 | 40 | Fill Opacity percentage | 35 |
| 41 | 41 | Hatch Spacing | 12 |
| Categorized layout | 60 | Connection Style: Straight / Rounded Arc | Straight |
| Categorized layout | 61 | Arc Curvature percentage | 25 |
| Categorized layout | 62 | Fill Primary Color | RGB 0,255,192 |
| Categorized layout | 63 | Fill Secondary Color | RGB 24,4,48 |
| Categorized layout | 64 | Fill Box Inset | 0 |
| Categorized layout | 65 | Fill Edge Feather | 0 |
| Categorized layout | 66 | Hatch Width | 3 |
| Categorized layout | 67 | Hatch Angle | 45 degrees |
| Categorized layout | 68 | Legacy Style Source (hidden; colors are always independent) | Global |
| Categorized layout | 69 | Box Stroke Color | RGB 0,255,192 |
| Categorized layout | 70 | Box Opacity | 100 |
| Categorized layout | 71 | Box Stroke Width | 2 |
| Categorized layout | 72 | Box Padding | 4 |
| Categorized layout | 73 | Box Corner Length percentage | 25 |
| Categorized layout | 74 | Marker Color | RGB 0,255,192 |
| Categorized layout | 75 | Marker Opacity | 100 |
| Categorized layout | 76 | Marker Stroke Width | 2 |
| Categorized layout | 77 | Marker Size | 8 |
| Categorized layout | 78 | Network Color | RGB 0,255,192 |
| Categorized layout | 79 | Network Opacity | 100 |
| Categorized layout | 80 | Network Thickness | 2 |
| Categorized layout | 81 | Grid Color | RGB 255,255,255 |
| Categorized layout | 82 | Grid Opacity | 35 |
| Categorized layout | 83 | Grid Thickness | 1 |
| Categorized layout | 84 | Label Text Color | RGB 0,255,192 |
| Categorized layout | 85 | Label Text Opacity | 100 |
| Categorized layout | 86 | Label Panel Color | RGB 5,20,18 |
| Categorized layout | 87 | Label Leader Color | RGB 0,255,192 |
| Categorized layout | 88 | Label Leader Opacity | 100 |
| Categorized layout | 89 | Technical Path Color | RGB 0,255,192 |
| Categorized layout | 90 | Technical Path Opacity | 100 |
| Categorized layout | 91 | Technical Path Thickness | 2 |
| Categorized layout | 92 | Network Dashed Lines | Off |
| Categorized layout | 93 | Network Dash Length | 10 |
| Categorized layout | 94 | Network Gap Length | 5 |
| Categorized layout | 95 | Network Arrowheads | Off |
| Categorized layout | 96 | Network Arrow Color | RGB 210,236,255 |
| Categorized layout | 97 | Network Arrow Opacity | 100 |
| Categorized layout | 98 | Network Arrow Size | 12 |
| Categorized layout | 99 | Label Panel Padding | 3 |
| Categorized layout | 100 | Label Offset X | 0 |
| Categorized layout | 101 | Label Offset Y | 0 |
| Colors + Cartesian HUD | 102 | Colors group start row | N/A |
| Colors + Cartesian HUD | 103 | Grid Projection Color | RGB 140,255,38 |
| Colors + Cartesian HUD | 104 | Colors group end row | N/A |
| Colors + Cartesian HUD | 105 | Grid View Mode: Simple / Cartesian | Cartesian |
| Colors + Cartesian HUD | 106 | Grid Axis Labels | On |
| Colors + Cartesian HUD | 107 | Grid Tick Size | 6 |
| Colors + Cartesian HUD | 108 | Grid Node Projections | On |
| Colors + Cartesian HUD | 109 | Grid Projection Opacity | 85 |
| Colors + Cartesian HUD | 110 | Grid Projection Dash Length | 8 |
| Colors + Cartesian HUD | 111 | Grid Projection Gap Length | 6 |
| Performance + Fill | 112 | Fill Invert Strength | 100 |
| Motion + Labels | 113 | Detection Source: Color / Motion / Color + Motion / Color AND Motion | Color |
| Motion + Labels | 114 | Motion Threshold percentage | 8 |
| Motion + Labels | 115 | Motion Softness percentage | 4 |
| Motion + Labels | 116 | Scene Cut Guard percentage | 65 |
| Motion + Labels | 117 | Motion Cleanup radius | 1 |
| Motion + Labels | 118 | Camera Translation Compensation | Off |
| Motion + Labels | 119 | Maximum Camera Shift | 8 |
| Motion + Labels | 120 | Text & Font editor button | N/A |
| Motion + Labels | 121 | Label Panel Corner Radius | 4 |
| Motion + Labels | 122 | Label Border Opacity | 0 |
| Motion + Labels | 123 | Label Border Thickness | 1 |
| Motion + Labels | 124 | Label Shadow Opacity | 0 |
| Motion + Labels | 125 | Label Shadow Offset X | 3 |
| Motion + Labels | 126 | Label Shadow Offset Y | 3 |
| Motion + Labels | 127 | Label Border Color | RGB 0,255,192 |
| Motion + Labels | 128 | Label Shadow Color | RGB 0,0,0 |
| Label UI topics | 129 | Content & Typography group start | N/A |
| Label UI topics | 130 | Content & Typography group end | N/A |
| Label UI topics | 131 | Panel Appearance group start | N/A |
| Label UI topics | 132 | Panel Appearance group end | N/A |
| Label UI topics | 133 | Placement & Leaders group start | N/A |
| Label UI topics | 134 | Placement & Leaders group end | N/A |

Creative IDs 11-41 and 60-128 plus UI topic IDs 129-134 are strictly
append-only. Every creative
feature defaults off, while active baseline values match the rendering behavior
that predated these controls. Invalid popup values resolve to their
compatibility default.

## Categorized host UI append

The flat list is presented through native `PF_Param_GROUP_START` and
`PF_Param_GROUP_END` rows. These rows contain no creative value:

| Group | Start disk ID | End disk ID | Initial state |
|---|---:|---:|---|
| Detection | 42 | 43 | Expanded |
| Tracking & Smoothing | 44 | 45 | Collapsed |
| Output | 46 | 47 | Collapsed |
| Network | 48 | 49 | Collapsed |
| Labels | 50 | 51 | Collapsed |
| Region Fill | 52 | 53 | Collapsed |
| Measurement HUD | 54 | 55 | Collapsed |
| Technical Paths | 56 | 57 | Collapsed |
| Performance | 58 | 59 | Collapsed |
| Colors | 102 | 104 | Collapsed |
| Labels / Content & Typography | 129 | 130 | Collapsed |
| Labels / Panel Appearance | 131 | 132 | Collapsed |
| Labels / Placement & Leaders | 133 | 134 | Collapsed |

Topic rows were registered before the later rounded-connection controls, which
is why the next creative disk IDs are 60 and 61. This does not affect the
serialized identity of IDs 1-41.
