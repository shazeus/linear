# Linear

[![Core CI](https://github.com/shazeus/linear/actions/workflows/ci.yml/badge.svg)](https://github.com/shazeus/linear/actions/workflows/ci.yml)

Linear is an open-source native C++ effect for Adobe After Effects and
Premiere Pro on Windows. It detects color- or motion-derived regions and turns
them into configurable technical overlays: tracked boxes, markers, labels,
networks, measurement grids, paths, and region-confined fills.

> **Project status:** v0.1.0 is an unsigned development release. The plug-in
> has been compiled with the official After Effects 25.6 SDK and recognized by
> Premiere Pro 2026. Full timeline validation in Premiere Pro and in-host
> validation in After Effects are still pending.

## Highlights

- RGB, HSV, and Lab color detection with alpha handling, blur, and morphology.
- Color, Motion, Color + Motion, and Color AND Motion detection sources.
- Deterministic tracking with Basic, Adaptive, Cinematic, and Predictive
  smoothing modes.
- Rectangle, corner-bracket, and ellipse regions with nine marker shapes.
- Six network topologies and five line-routing styles, including curved,
  stepped, and PCB-style connections.
- Simple and Cartesian measurement grids with node projections.
- Collision-aware labels with direct text editing, UTF-8 text, installed
  Windows fonts, rounded panels, borders, shadows, and leader lines.
- Shape-confined Solid, Hatch, Crosshatch, Invert, X-Ray, and Duotone fills.
- Full, half, and quarter-resolution analysis with full-resolution output.
- After Effects Multi-Frame Rendering-aware, render-order-independent core.
- 43 dependency-free core tests plus a CPU benchmark executable.

## Install the Windows release

1. Download `Linear-0.1.0-windows-x64.zip` from GitHub Releases.
2. Verify it against the accompanying `.sha256` file.
3. Close After Effects, Premiere Pro, and Adobe Media Encoder.
4. Extract `Linear.aex` to:

   ```text
   C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore\Linear\
   ```

5. Restart the Adobe host and search the effects list for **Linear**.

The binary is currently unsigned, so Windows or managed workstations may show
an additional trust warning. Source builds can use `scripts/install-dev.ps1`.

## Build the shared core

Requirements: CMake 3.24+ and a C++17 compiler.

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug --parallel
ctest --preset windows-debug --output-on-failure
```

The shared core does not require the Adobe SDK. GitHub Actions builds and tests
this SDK-free target on Windows and Linux.

## Build the Adobe plug-in

Download the official After Effects C++ SDK from Adobe, then point
`ADOBE_AE_SDK_ROOT` to the directory containing
`Examples/Headers/AE_Effect.h`:

```powershell
$env:ADOBE_AE_SDK_ROOT = 'D:\SDKs\AfterEffectsSDK'
cmake --preset windows-adobe
cmake --build --preset windows-adobe-release --parallel
```

Adobe SDK files are proprietary, user-supplied, excluded from source control,
and never included in Linear release archives. See [Building](docs/BUILDING.md)
and [Third-party notices](docs/THIRD_PARTY_NOTICES.md).

## Documentation

- [User guide](docs/USER_GUIDE.md)
- [Building](docs/BUILDING.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Performance](docs/PERFORMANCE.md)
- [Parameter compatibility](docs/PARAMETER_COMPATIBILITY.md)
- [Implementation status](docs/IMPLEMENTATION_STATUS.md)
- [Troubleshooting](docs/TROUBLESHOOTING.md)
- [Changelog](docs/CHANGELOG.md)

## Contributing and security

Bug reports and focused pull requests are welcome. Read
[CONTRIBUTING.md](CONTRIBUTING.md) before submitting changes. Please report
security issues privately as described in [SECURITY.md](SECURITY.md).

## License

Linear is released under the [MIT License](LICENSE). Adobe products and the
Adobe After Effects SDK are governed by Adobe's own terms and are not part of
this repository.
