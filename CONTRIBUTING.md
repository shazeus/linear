# Contributing to Linear

Thanks for helping improve Linear. Small, reviewable changes with clear tests
are the easiest to merge.

## Before opening a pull request

1. Create a focused branch from `main`.
2. Keep serialized Adobe parameter disk IDs append-only. Never reuse or reorder
   an existing ID; update `docs/PARAMETER_COMPATIBILITY.md` for additions.
3. Keep Adobe SDK files outside source control. Use `ADOBE_AE_SDK_ROOT` for a
   local SDK checkout.
4. Add or update tests for behavior changes.
5. Run:

   ```powershell
   cmake --preset windows-debug
   cmake --build --preset windows-debug --parallel
   ctest --preset windows-debug --output-on-failure
   ```

6. Update user-facing documentation and `docs/CHANGELOG.md` when appropriate.

## Code guidelines

- Target C++17 and preserve `/W4 /WX` cleanliness on MSVC.
- Keep core algorithms independent from Adobe host state.
- Preserve render-order independence and Multi-Frame Rendering safety.
- Bound work by image dimensions, region limits, or explicit budgets.
- Avoid hidden global mutable render state.
- Do not claim host support that has not been tested in that host.

## Pull requests

Describe what changed, why it changed, user impact, compatibility impact, and
the exact checks you ran. Screenshots or short clips are useful for visible
overlay changes, but should not contain copyrighted footage unless you have
permission to share it.
