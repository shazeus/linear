# Releasing

## Preconditions

- The working tree contains only intended source and documentation changes.
- `LICENSE`, `README.md`, `SECURITY.md`, and `THIRD_PARTY_NOTICES.md` are current.
- Adobe SDK files remain ignored and untracked.
- Adobe video hosts are closed before rebuilding or replacing `Linear.aex`.
- The version in `project(...)`, package naming, changelog, and tag agree.

## Validation

```powershell
cmake --build --preset windows-debug --parallel
ctest --preset windows-debug --output-on-failure
cmake --build --preset windows-adobe-release --parallel
ctest --test-dir build/windows-adobe -C Release --output-on-failure
scripts\package.ps1
```

Check that `EffectMain` and `PluginDataEntryFunction2` remain exported and that
the build artifact, packaged `Linear.aex`, and installed `Linear.aex` have the
same SHA-256 hash.

## GitHub release

1. Commit the release on `main` and create an annotated `vX.Y.Z` tag.
2. Push `main` and the tag.
3. Create a non-draft GitHub release from the tag.
4. Upload `Linear-X.Y.Z-windows-x64.zip` and its `.sha256` file.
5. State clearly that the binary is unsigned and list host validation that is
   still pending.

Never upload the Adobe SDK or its example source. Before distributing a
compiled plug-in binary, confirm that the current Adobe developer terms for
the SDK version in use permit that distribution.
