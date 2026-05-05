# Bullet Physics — prebuilt static libs (Windows)

Source lives in the **`bullet3`** submodule: [`../bullet3`](../bullet3).

This folder holds **MSVC static libraries** (`/MT` or `/MTd`) that match the Doom 3 solution’s runtime (`MultiThreaded` / `MultiThreadedDebug`).

## Build (once per machine / after Bullet upgrade)

From the repository root, in **PowerShell**:

```powershell
git submodule update --init --recursive
pwsh -File third_party\build-bullet-msvc.ps1
pwsh -File third_party\build-bullet-msvc.ps1 -Configuration Debug
```

Libraries are copied to:

- `third_party/bullet/lib/Release/LinearMath.lib`, `BulletCollision.lib`, `BulletDynamics.lib`
- `third_party/bullet/lib/Debug/` — same names (debug CRT)

`neo/_Bullet.props` adds include path `third_party/bullet3/src` and links these libs for **DoomDLL** only.

## Verify in-game (Windows)

With a Release build that linked successfully, open the console and run:

```
bulletProbe
```

You should see a line reporting the simulated box height (sanity check that Bullet linked).

## Notes

- **Linux** builds of this fork do not link Bullet yet; `bulletProbe` is a no-op message unless you add a Linux CMake path for Bullet to the game project.
- The CMake build tree under `third_party/bullet/msbuild_*` is **ignored by git** (local only).
