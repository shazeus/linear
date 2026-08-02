# Building

## Requirements

- Windows 10/11 x64
- Visual Studio 2022 with Desktop development with C++
- CMake 3.24 or newer
- Official Adobe After Effects C++ SDK for the host target

The repository does not download dependencies or redistribute Adobe headers.
Do not copy the SDK into a tracked directory. The local
`third_party/adobe-sdk-local/` path is ignored as an additional safeguard.

## Core, tests, and benchmark

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug --parallel
ctest --preset windows-debug
```

Release:

```powershell
cmake --build --preset windows-release --parallel
```

## Adobe target

Set the SDK root to the directory containing `Examples/Headers/AE_Effect.h`:

```powershell
$env:ADOBE_AE_SDK_ROOT = 'D:\SDKs\AfterEffectsSDK'
cmake --preset windows-adobe
cmake --build --preset windows-adobe-release --parallel
```

The Adobe target has been compiled against the official After Effects 25.6
Windows SDK. Configuration still fails early when the SDK is missing, preventing
accidental compilation against invented or stale API declarations.

The Premiere C++ SDK is optional for a common After Effects-effect build but
will be read from `ADOBE_PREMIERE_SDK_ROOT` when Premiere-only suites are added.

Development installation:

```powershell
scripts\install-dev.ps1
```

This copies the release `.aex` into the shared Adobe MediaCore plug-in folder,
which is scanned by Premiere Pro and After Effects. Restart the host after
installation. Use `scripts\uninstall-dev.ps1` to remove that exact development
folder.

## Clean build

Build outputs are isolated under `build/`. Remove the exact configuration
directory you want to regenerate, then rerun its configure preset.

## macOS

The shared core has an Xcode preset, but no macOS build was performed in this
Windows environment. A future native bundle must be built with the SDK version
supported by the target After Effects/Premiere release and must not be described
as signed or notarized until those steps actually run.
