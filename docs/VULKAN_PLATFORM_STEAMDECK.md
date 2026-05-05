# Vulkan raster, Steam Deck, and “more” (no ray tracing)

**Steam Deck GPUs do not support hardware ray tracing** in a useful way for desktop-class DXR/VK RT pipelines. This fork should treat **Vulkan as the primary portable RHI for Linux / Deck**, and invest in **raster features**, **I/O**, and **media** instead of RT-on-Vulkan.

This document aligns goals with **what already exists in classic id Tech 4** in this tree and what typically comes from a **large port** (e.g. RBDOOM-3-BFG patterns).

---

## 1. Renderer: Vulkan without RT

**Today (this fork):** real-time GI / lighting experiments are tied to **D3D12 + DXR** in the IceBridge layer (`neo/opengl/gl_d3d12*.cpp`) on **Windows**. On **Linux**, an **experimental Vulkan swapchain + present** path exists when **`r_icebridgeRHI vulkan`** is set: it runs **alongside GLX** (OpenGL still renders the game; Vulkan clears and presents each frame for driver validation). See [LINUX_BUILD_VULKAN.md](LINUX_BUILD_VULKAN.md).

**Deck-appropriate direction:**

| Milestone | Outcome |
|-----------|---------|
| A | **Vulkan instance + `VkSurfaceKHR` + swapchain** — **partial on Linux/X11** (`icebridge_vulkan_linux.cpp` + `r_icebridgeRHI vulkan`); Wayland-native surface and Deck-specific tuning still open |
| B | **Depth/stencil + render pass** feeding the existing material backend *or* a staged rewrite |
| C | **MSAA / TAA** as post passes (Deck-friendly defaults: lower internal res + TAA) |
| D | **Async compute / graphics** overlap for upload and draw (where the driver wins) |

**Non-goals on Deck:** `VK_KHR_ray_tracing_pipeline`, `VK_KHR_ray_query` as shipping features.

Optional: keep **DX12 + DXR** on Windows for high-end PCs; use **Vulkan raster** on Linux/Deck from the same codebase (`#ifdef` / RHI factory).

---

## 2. Streaming and asset loading

**Already in id Tech 4 (see `neo/framework/FileSystem.cpp`):**

- **Virtual FS**: `fs_basepath`, `fs_savepath`, `fs_cdpath`, `fs_devpath`, **`fs_game`** mod directory layered over `base/`.
- **`.pk4` archives**: normal zip files, searched **high pak number first** so patches override.
- **Optional `fs_copyfiles`**: copy-on-read from CD path to save path (dev / partial install workflows).

**“Streaming” in the modern sense** (open world, texture streaming, GPU residency) is **not** a single switch in this engine. Practical steps:

1. **Smaller pak slices** and **`fs_game`** packs so Deck storage and seek patterns stay sane.  
2. **Image background load** — the renderer already has **background image load** concepts (`image_showBackgroundLoads`, deferred `ActuallyLoadImage` in `neo/renderer/Image_init.cpp`); tuning **preload**, **purge**, and **budget** is the lever.  
3. **Optional pack prefetch** — OS-level `madvise`/read-ahead or a small **async read thread** for next level’s manifest (new code).  
4. **GPU memory budget** — expose CVars for **texture LOD bias**, **max aniso**, **precompressed DDS** preference (`image_usePrecompressedTextures`).

---

## 3. Supported assets (formats in this tree)

Rough inventory from engine code paths (not an exhaustive spec):

| Kind | Formats / notes |
|------|------------------|
| **Packages** | `.pk4` (zip) |
| **Images** | Primarily **TGA** in materials; **DDS** precompressed path when enabled; cube maps via `R_LoadCubeImages` |
| **Audio** | **WAV**; **OGG** via `neo/sound/snd_wavefile.cpp` (`OpenOGG` / `ReadOGG`) |
| **Video / cinematics** | **ROQ** in `neo/renderer/Cinematic.cpp` (material-driven bink-like path is ROQ-era) |
| **Models** | ASE, LWO, MD5, procedural surfaces — **not** glTF in vanilla |

**RBDOOM-3-BFG-style additions** (png/exr/hdr, glTF, FFmpeg) are **separate feature ports** with new loaders and CMake deps—not enabled by default here.

---

## 4. Codecs and media pipeline

| Codec | Role in classic Doom 3 |
|-------|-------------------------|
| **ROQ** | In-engine cinematic playback |
| **Ogg Vorbis** | Compressed voice/SFX where shipped |
| **PNG/JPEG in materials** | **Not** stock; adding them needs image loader work |

For **Deck-friendly cinematics**, long-term options are:

- **FFmpeg** (optional build) like RB — license and binary size tradeoffs.  
- **Theora / WebM** — new decode path + texture upload.  
- Keep **ROQ** for vanilla compatibility; add modern formats behind **`fs_game`** only.

---

## 5. Steam Deck checklist (engineering)

- **Build**: Linux binary (CMake or SCons revival) with **Vulkan** + **SDL2** input/display (pattern from RB, not copy-paste).  
- **Display**: default to **fullscreen borderless** under Gamescope; respect **`SDL_VIDEODRIVER`** / Wayland.  
- **Power**: cap frame rate, reduce **sync interval**, prefer **precompressed textures**, avoid giant single **pk4** where possible.  
- **Storage**: default **`fs_savepath`** to user-writable path on Deck.  
- **Controls**: Steam Input already maps; engine-side **gamepad** polish is in `sys/` / game code.

---

## 6. Related tools in this repo

- **`tools/vulkan_caps`** — small probe: instance + optional device, lists **swapchain / sync / bindless** extensions useful for **raster** Vulkan (see README in that folder).

---

## 7. See also

- [RBDOOM-3-BFG](https://github.com/RobertBeckebans/RBDOOM-3-BFG) for **CMake + SDL2 + Vulkan (NVRHI)** reference implementation (different base: **BFG**, not a drop-in merge).
