# Adobe host notes

Linear is designed as an After Effects Effect SDK plug-in because Premiere Pro
supports the relevant subset of that effect API. The common CPU path is the
compatibility baseline.

## Premiere Pro

- Negotiate 8-bit BGRA and 32-bit BGRA where the installed SDK supports them.
- Convert through the checked pixel adapter; do not reinterpret AE-native ARGB
  memory as Premiere-native BGRA.
- Query every optional suite/capability before use.
- Keep a software path even if a future Premiere GPU extension is available.
- Treat unsupported custom UI and temporal checkout as declared limitations,
  never as silent no-ops.

## After Effects

- Support 8-bpc, 16-bpc, and 32-bpc float through explicit adapters.
- Add Smart Render only after checkout/checkin lifetimes are validated.
- Set Multi-Frame Rendering flags only after the host adapter and all caches pass
  a thread-safety audit.

## Media Encoder

Media Encoder compatibility is inherited only where the host loads the same
effect and provides the required render behavior. It remains unverified.

## Current environment

Premiere Pro 2026 is installed. The effect was compiled against the official
After Effects 25.6 Windows SDK, installed to MediaCore, recognized by Premiere's
effect loader, and observed in the running process module list. Applying it to
footage and validating pixels is still pending. After Effects is not installed.
