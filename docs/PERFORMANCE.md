# Performance

Build and run:

```powershell
cmake --build --preset windows-release --parallel
build\windows-debug\Release\linear_bench.exe
```

The benchmark reports color detection, motion detection, component extraction,
graphics, total time, and region count for generated 1080p and 4K frames. It is
a diagnostic baseline, not an Adobe host-performance claim.

## 2026-08-01 production CPU pass

Baseline measured immediately before this pass:

| Case | Color | Motion | Regions | Graphics | Combined diagnostic total |
|---|---:|---:|---:|---:|---:|
| 1080p | 113.58 ms | 82.23 ms | 17.52 ms | 19.80 ms | 233.13 ms |
| 4K | 445.16 ms | 364.88 ms | 68.67 ms | 70.98 ms | 949.69 ms |

Median of three Release/LTCG runs after optimization:

| Case | Color | Motion | Regions | Graphics | Combined total | Total speedup |
|---|---:|---:|---:|---:|---:|---:|
| 1080p | 28.37 ms | 41.09 ms | 6.45 ms | 7.01 ms | 82.81 ms | 2.82x |
| 4K | 111.92 ms | 213.10 ms | 42.21 ms | 28.12 ms | 396.14 ms | 2.40x |

## Broad pipeline pass

A second pass targeted work outside the detector: unused-feature gating,
overlapping temporal frames, component descriptors, contour resolution,
tracking, ellipse tessellation, network distance tests, and no-overlay host
copies. Median of three Release/LTCG runs after that pass:

| Case | Color | Motion | Regions | Graphics | Combined total | vs. original |
|---|---:|---:|---:|---:|---:|---:|
| 1080p | 28.31 ms | 39.78 ms | 5.57 ms | 7.31 ms | 81.03 ms | 2.88x |
| 4K | 111.94 ms | 215.55 ms | 21.80 ms | 27.39 ms | 378.10 ms | 2.51x |

The component stage now matches the plug-in's actual needs and skips unused
mean RGB/luminance accumulation. That halves the measured 4K region stage from
the first pass. The combined diagnostic still includes motion analysis even
though the current host effect does not run it.

Feature-heavy diagnostics added in this pass:

| Diagnostic | Release median |
|---|---:|
| 1080p ellipse + smooth network + grid + labels + crosshatch fill | 21.16 ms |
| 4K contour, Full analysis | 34.61 ms |
| 4K contour, Half analysis | 8.11 ms |
| 4K contour, Quarter analysis | 2.58 ms |

These figures measure owned core images, not an Adobe timeline. They exclude
host checkout, decode/encode, scheduling, and display costs.

The combined diagnostic deliberately runs both color and motion even though the
current Adobe host effect uses the color path. The optimized implementation
uses fused RGB thresholding, cache-friendly separable min/max morphology, a
visited-byte flood traversal instead of full-frame union-find, ROI-only fills,
a cached X-Ray LUT, incremental hatch phases, bilinear DDA strokes, direct
sampled history decode, move-based compositing, faster pixel quantization, and
link-time optimization.

The broad pass adds a bounded 16-entry/128 MiB content-addressed analysis cache.
The cache key includes sampled pixels and every analysis-affecting parameter;
values are immutable. Identical MFR requests use single-flight publication, so
one thread computes while peers share the result. Style-only changes and
overlapping temporal windows therefore avoid repeated detection/morphology/
component work without retaining host frame pointers. Unused region features
skip analysis and temporal checkout entirely. A no-overlay Source+Overlay render
uses a direct row copy, while an empty Overlay Only render clears the host world
without float decode/encode.

No internal frame-level thread pool is created. Adobe Multi-Frame Rendering can
therefore schedule frames without Linear oversubscribing the CPU.

## Analysis-resolution results

| Frame | Before Full | After Full | Before Half | After Half | Before Quarter | After Quarter |
|---|---:|---:|---:|---:|---:|---:|
| 1080p | 141.09 ms | 46.12 ms | 37.54 ms | 13.75 ms | 9.25 ms | 3.65 ms |
| 4K | 563.20 ms | 181.05 ms | 150.87 ms | 52.82 ms | 52.30 ms | 13.75 ms |

`Analysis Resolution` defaults to Half for newly applied effects. Existing
project instances keep their serialized numeric selection. Half is the
recommended production setting; Quarter is intended for dense 4K work or
interactive setup. Region geometry remains full-resolution. A reduced mask is
expanded only for Detection Mask output. Contours are extracted at the selected
analysis resolution and their points are scaled back to output coordinates.

Historical smoothing frames are decoded directly at the selected analysis
resolution instead of being decoded at full size and resized afterward. Masks
that are not needed for Capture Persistence are released before compositing,
which materially reduces MFR memory pressure.

The benchmark still excludes Adobe decode/encode, temporal checkout, and host
scheduling. In-host profiling, Smart Render/32-bpc, and a validated GPU path
remain future tiers.
