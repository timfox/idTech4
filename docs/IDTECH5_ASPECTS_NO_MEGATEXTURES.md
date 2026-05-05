# id Tech 5–style directions in this fork (excluding MegaTextures / virtual texturing)

This document names **aspects of id Tech 5’s engineering direction** that are still relevant on **classic id Tech 4 + IceBridge**, and maps them to **concrete areas in this repository**. It deliberately **does not** cover **MegaTextures**, **virtual texturing**, or **megatexture-style clip maps**—those imply a different asset pipeline, streaming residency model, and tooling stack than Doom 3’s materials and images.

---

## 1. What we mean by “id Tech 5 aspects” (without VT)

| Theme | id Tech 5–era idea | Fit for this fork |
|--------|-------------------|-------------------|
| **Multi-core** | Keep simulation and rendering fed from worker threads; minimize giant critical sections. | Extend **existing threading** (`Sys_CreateThread`, file background thread) with **clear job boundaries** around file I/O, decode, and renderer prep—without rewriting the whole game loop overnight. |
| **Streaming I/O** | Predictable latency for loads; overlap I/O with gameplay. | Lean on **`BackgroundDownload`** / background reads in `neo/framework/FileSystem.*` and **background image load** paths in the renderer (`image_showBackgroundLoads`, deferred image completion in `neo/renderer/Image_init.cpp`). |
| **GPU-first API usage** | Command recording, heaps, barriers, async queues where it wins. | **IceBridge on D3D12** already sits here; **Vulkan RHI** work (`r_icebridgeRHI`, `vkInfo`, `tools/vulkan_caps`) is the portable counterpart. |
| **Frame and memory budgets** | Bounded frame cost; reclaim or LOD before you hitch. | Tune **image purge**, **preload**, **texture quality** CVars and renderer budgets; document “Deck-safe” defaults (see [VULKAN_PLATFORM_STEAMDECK.md](VULKAN_PLATFORM_STEAMDECK.md)). |
| **Deterministic failure modes** | If a resource is missing, degrade clearly. | Preserve **idFileSystem** search order and **pak override** semantics; extend logging and optional **async read** error surfaces where new code is added. |

---

## 2. Where it lands in this tree (starting points)

**Threading and async I/O**

- `neo/sys/sys_public.h` / platform `Sys_*` — thread creation and sync primitives used across the engine.
- `neo/framework/FileSystem.cpp` — **`BackgroundDownload`**, background download thread, virtual path layering (`fs_basepath`, `fs_game`, `.pk4` ordering).

**Renderer: overlap and “next frame” work**

- `neo/renderer/tr_local.h` — comments and structures around **front/back** separation and parallel-friendly data duplication.
- `neo/renderer/Image_init.cpp` — **background image load** completion and related CVars (tuning is the main “id5-like” lever without a VT system).

**Modern GPU path (not VT)**

- `neo/opengl/gl_d3d12*.cpp` — D3D12 / DXR path used today on Windows.
- `neo/renderer/RHI/icebridge_rhi_registry.cpp`, `neo/opengl/icebridge_rhi.h` — **RHI selection** (D3D12 vs experimental Vulkan).
- `docs/VULKAN_PLATFORM_STEAMDECK.md` — raster Vulkan milestones aligned with “GPU-first” without megatextures.

**Input / platform**

- `neo/sys/sys_joystick.cpp`, `docs/STEAMDECK_CONTROLS.md` — console-first tuning and diagnostics in the spirit of shipping on varied hardware.

---

## 3. Suggested phases (practical order)

1. **Measure** — frame time breakdown (CPU game vs render vs I/O); which pak or image loads spike on Deck or HDD.
2. **I/O overlap** — ensure level transitions and large reads use **background** paths where safe; avoid synchronous giant reads on the game thread during combat.
3. **Image policy** — document and ship **conservative preload / purge** defaults for low-memory GPUs; optional “high” profile for desktop.
4. **RHI** — complete **Vulkan raster** milestones in `VULKAN_PLATFORM_STEAMDECK.md` so Linux/Deck share one modern API path; keep **DX12 + DXR** on Windows for RT-heavy configs.
5. **Optional job queue** — only after (1)–(4): a small **job system** (work stealing or fixed worker pool) scoped to **isolated tasks** (decompression, mesh preprocess, bake steps)—not a full entity rewrite.

---

## 4. Explicit non-goals (this document)

- **Virtual texturing / clip maps / megatexture authoring** and the associated **single giant page table** asset pipeline.
- Replacing Doom 3’s **per-material image** model with VT without a multi-year content and tool migration.

For borrowing features from a **different** codebase lineage, see [RBDOOM-3-BFG-PORTING.md](RBDOOM-3-BFG-PORTING.md) for merge reality and cherry-pick strategies.
