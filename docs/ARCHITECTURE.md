# Architecture

The core has no Adobe dependency. Host adapters decode a checked-out host frame
into `linear::Image`, take an immutable parameter snapshot, invoke one detection
pass, and feed its mask/regions to every enabled graphics module.

```text
Adobe host adapter (AE / Premiere)
  -> pixel adapter (row bytes, format, alpha)
  -> immutable render settings
  -> content + analysis-settings cache key
  -> color and/or temporal motion confidence
  -> threshold, blur, morphology
  -> connected components and descriptors
  -> explicit-window deterministic tracking
  -> connections and CPU overlay rasterizer
  -> host-format encoder
```

`HostCapabilities` keeps feature choice explicit. Premiere prefers BGRA or
32-bit float RGB, while After Effects can use its native supported format. The
host adapter—not the algorithms—owns suite acquisition, frame checkout,
cancellation, and error translation.

Detection is performed once per unique sampled content/settings pair. Graphics
modules consume the same region vector and never rescan the source. Mutable
render containers are invocation-local. A bounded process-wide cache shares only
immutable masks/regions, uses single-flight synchronization, and never stores a
host pointer or order-dependent frame history.

Product identity is generated from `config/ProductConfig.cmake`.
