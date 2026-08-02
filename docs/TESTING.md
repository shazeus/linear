# Testing

Run:

```powershell
ctest --preset windows-debug
```

Current automated coverage includes RGB/HSV/Lab distance, circular hue,
threshold softness, blur, erosion/dilation/open/close, alpha-aware color
detection, connected components, region limits, bounds/centroids/perimeter,
IoU, motion and scene cuts, combined masks, deterministic tracking, velocity,
MST/nearest connections, labels, row bytes, premultiplication, float conversion,
Premiere render-path selection, and overlay-only alpha.

Golden-image tests and manual host tests are not yet implemented. The host test
matrix must include 1080p, 4K, vertical, transparent, noisy, handheld, static,
fast/slow motion, scene cuts, reverse/time-remapped playback, proxy/downsample,
multiple instances, nested compositions, MFR on/off, save/reopen, and Media
Encoder.
