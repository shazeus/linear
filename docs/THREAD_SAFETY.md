# Thread safety

- Core functions receive all inputs explicitly and return owned values.
- There is no global previous frame or order-dependent tracking state.
- Tracking sorts and evaluates a caller-supplied time-keyed frame window.
- Repeated evaluation of identical inputs and seed produces identical IDs.
- Images and temporary raster buffers belong to one invocation.
- Host suite calls must remain outside core locks.

The Adobe adapter owns a process-wide, content-addressed analysis cache. Its key
contains the decoded sampled pixels, dimensions, downsample divisor, and every
detection/component parameter that affects the result. It does not trust render
order, time alone, or a host-world pointer. Published masks and regions are
immutable. The cache is protected by mutexes, uses single-flight publication
for identical concurrent requests, and is bounded to 16 entries / 128 MiB.
Failures wake waiters and remove the failed entry. Host frame handles and
pointers never survive checkout scope.

The current adapter advertises Multi-Frame Rendering. Every render invocation
owns its decoded frames, mutable region copies, label layout, paths, and raster
output. Only immutable analysis values are shared. Premiere and After Effects
host checkouts are paired within the command scope.

Custom label text and font family are stored in a fixed-size, versioned
sequence-data block. The modal Write + Apply editor runs only on the host UI
selector; render obtains a const view through `PF_EffectSequenceDataSuite1`.
Flatten/get-flattened/resetup callbacks migrate the legacy v1 text payload and
copy the v2 POD block, so no window handle, mutable string allocation, or host
pointer crosses render threads or project-save boundaries. Each render creates
and owns its GDI font raster surfaces locally.
