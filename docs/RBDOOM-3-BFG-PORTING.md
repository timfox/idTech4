# Porting enhancements from RBDOOM-3-BFG

This document explains **why a wholesale “port RBDOOM-3-BFG into this tree” is not a single merge**, and how to **borrow real value** from [RobertBeckebans/RBDOOM-3-BFG](https://github.com/RobertBeckebans/RBDOOM-3-BFG) without discarding this fork’s **IceBridge + DXR** work.

Upstream reference: **RBDOOM-3-BFG** is a modernization of **DOOM 3 BFG** (GPL). This repository is **classic Doom 3–style id Tech 4** with a **GL compatibility layer implemented on D3D12** (`neo/opengl/gl_d3d12*.cpp`) and **DXR lighting** in `neo/opengl/gl_d3d12raylight.cpp`.

---

## 1. What is different at the core (merge reality)

| Area | This fork (IceBridge idTech4) | RBDOOM-3-BFG |
|------|--------------------------------|---------------|
| **Base game** | Classic Doom 3 code paths, `.pk4`, original assets | **BFG** codebase, `.resources`, BFG asset rules |
| **Renderer API** | Legacy **OpenGL-style** backend → **D3D12 shim** + **DXR** | **NVRHI** abstraction → **D3D12 and/or Vulkan**; renderer types and passes refactored around `nvrhi::` |
| **Build** | **MSVC solution** (`neo/Doom3.sln`, `.vcxproj`) | **CMake** first-class; optional VS generators |
| **Linux / Ubuntu** | Not the primary ship path today | **SDL2** OS layer, **GCC/Clang**, documented Linux builds |
| **“GI” story** | **DXR path tracing / lighting** stack tied to D3D12 | **Baked GI** (irradiance volumes, IBL), **PBR**, **soft shadows**, **MOC**, TAA/SMAA, etc. (different pipeline) |

Because RB rewired **vertex cache**, **frontend**, **material/shader** paths, and **frame submission** through **NVRHI**, you **cannot** drop their `neo/renderer` on top of this fork like a patch. Porting “the renderer” means **choosing an architecture**: keep IceBridge, or **replace** it with NVRHI (multi‑year project).

---

## 2. Enhancement map: what to take and where it lives in RB

Use RB’s own readme as the feature list. Rough mapping to RB tree areas (clone RB and search these):

- **CMake + Linux/macOS/Windows scripts** — `neo/CMakeLists.txt`, `neo/cmake-*.{sh,bat}`, `neo/cmake/`
- **SDL2 system layer** — `neo/sys/sdl/` (and related `sys/` glue)
- **NVRHI + Vulkan/D3D12 backends** — `neo/renderer/NVRHI/`, `neo/renderer/RenderBackend*.cpp`
- **PBR materials / new decl keywords** — shader pipeline under `neo/renderer`, material parsing, `neo/shaders/`
- **MOC (occlusion)** — `neo/libs/moc/`, integration in renderer culling
- **OpenAL Soft** — optional CMake path vs XAudio
- **FFmpeg / Binkdec** — `neo/libs/`, CMake options `FFMPEG`, `BINKDEC`
- **glTF / OBJ** — model loaders under `neo/renderer` / tools (search `gltf`, `GLTF`)
- **TrenchBroom / dmap** — tools under `neo/tools/` (RB-specific workflow)

---

## 3. Recommended integration strategies (pick one spine)

### Strategy A — **Infrastructure only** (fastest win for Ubuntu)

**Goal:** Build and run **this** game logic on Linux without adopting RB’s renderer.

1. Add a **CMake** build that mirrors RB’s layout only for: **idLib**, **framework**, **game**, **sys** (POSIX/SDL), **cm**, **renderer** *as far as it still compiles*.
2. Port **Win32-specific** calls in this fork behind `#ifdef _WIN32` / SDL2 equivalents (pattern exists in RB).
3. Keep **IceBridge** Windows-only initially: on Linux, either **no GL** (headless/tools) or a **real GLX/OpenGL** path if you revive classic Linux GLimp (large).

**Risk:** Classic Doom 3 + IceBridge assumes **HWND/WGL**; RB never had this shim. Linux display without rewriting GLimp is non-trivial.

### Strategy B — **Cherry-pick isolated fixes** (low risk, ongoing)

Use `git format-patch` / manual apply for **small, self-contained** commits:

- Warning fixes, undefined behavior fixes, **fileSystem** edge cases
- **OpenAL** audio path if you align APIs
- Specific **tool** improvements (radiant/dmap) *only if* file bases still match

Use `diff`/`patch` per file; **do not** merge whole `neo/renderer` from RB.

### Strategy C — **Adopt NVRHI (RB renderer spine)** (largest; replaces IceBridge)

**Goal:** Vulkan + D3D12 like RB, **and** optionally keep or reimplement DXR on top of NVRHI.

1. Vendor **NVRHI** and dependencies as RB does (`neo/libs/`, CMake).
2. Replace this fork’s **D3D12 GL shim** path with NVRHI backends **or** implement a minimal NVRHI “compat” layer that still exposes what `tr_backend` needs.
3. Re-port **DXR GI** as **NVRHI/D3D12 ray tracing** and/or **Vulkan KHR ray tracing** — essentially a **new project phase**, not a port of `gl_d3d12raylight.cpp` line-by-line.

**Outcome:** Closest to “RB enhancements,” but **this fork’s current IceBridge + GL layer is largely superseded**.

### Strategy D — **Keep IceBridge; cherry-pick algorithms** (middle ground)

Take **ideas and math**, not files:

- **ACES / filmic tonemapping** parameters (reimplement in your post chain / shader)
- **TAA / SMAA** as passes in the D3D12 shim if you add post-processing stages there
- **PBR** brdf approximations where your materials still use classic maps

Requires reading RB shaders and reproducing in your **HLSL/GLSL** path.

---

## 4. Ubuntu and Vulkan specifically

- **RB** gives you a **proven Vulkan path** via **NVRHI + CMake + SDL2**.
- **This fork** currently has **`r_icebridgeRHI`** / `vkInfo`-style groundwork only; **no Vulkan swapchain or draw** for the game yet.
- For **Ubuntu + ray tracing**, you need **Vulkan RT extensions** (and a full backend), not only instance creation.

Practical path for Linux users **today**: run **RBDOOM-3-BFG** for Vulkan/PBR/Linux; use **this fork** where you need **classic Doom 3 + IceBridge + DXR** on Windows until Strategy A or C is executed.

---

## 5. Suggested phased plan (if continuing in-repo)

1. **Document & CI** — CMake that builds **idLib + framework** only on Linux (sanity).
2. **SDL2 window + input** — parity with RB `sys` layer for one configuration.
3. **Renderer** — choose A (minimal GL/Linux) vs C (NVRHI); do not mix both without a clear boundary.
4. **Content** — if you ever target BFG packs, that is a **separate** asset and filesystem project (`fs_game`, `.resources`).

---

## 6. License

Both projects are **GPLv3**-oriented engine code; combining sources is fine **if** you comply with license terms for **all** bundled third-party libs you import from RB (each may have its own notice file).

---

## 7. References

- RBDOOM-3-BFG: https://github.com/RobertBeckebans/RBDOOM-3-BFG  
- This fork’s D3D12 / WGL bridge: `neo/opengl/gl_d3d12wgl.cpp`, `neo/opengl/gl_d3d12shim.cpp`  
- DXR lighting stack: `neo/opengl/gl_d3d12raylight.cpp`  
- IceBridge RHI switch: `r_icebridgeRHI`, `neo/renderer/RHI/`, `neo/opengl/icebridge_rhi.h`
