# Troubleshooting

## CMake says `ADOBE_AE_SDK_ROOT` is required

The Adobe target was enabled without the official SDK. Download it through the
Adobe Developer Console, set the environment variable to the extracted SDK
root, and configure again. Do not point it at the Premiere application folder;
installed applications do not substitute for SDK headers.

## Core build cannot find a C++ compiler

Install the Visual Studio 2022 C++ workload and configure from a new PowerShell
session.

## Premiere does not show the effect

Confirm that `Linear.aex` exists under
`Adobe\Common\Plug-ins\7.0\MediaCore\Linear`, then restart Premiere. Search for
`Linear` in Video Effects. If it is still absent, inspect Premiere's plug-in
loading diagnostics; a successful compile does not prove that the installed
host accepted the binary.
