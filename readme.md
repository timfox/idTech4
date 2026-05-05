# idTech 4

## Overview

This is idTech 4 a fork of **Doom 3 IceBridge**, a Direct3D 12 renderer layer designed to bring modern GPU rendering features to classic idTech 4 technology while keeping the original game’s dark, industrial atmosphere intact.

This demo shows Doom 3 rendered with **Direct3D 12**, **path tracing**, **final gather**, and **global illumination**, adding richer lighting, more natural shadowing, improved reflections, and a deeper sense of atmosphere without rebuilding the game’s original art direction from scratch.

The goal is not to erase Doom 3’s classic look.

The goal is to enhance it with modern rendering techniques while preserving the mood, darkness, contrast, and oppressive atmosphere that made the original game so iconic.

No game assets are included in this fork.

## Building and running

This fork targets **64-bit Windows**. Open [`neo/Doom3.sln`](neo/Doom3.sln) in **Visual Studio** (the solution targets the **v145** MSVC platform toolset and **Windows 10.0** SDK as declared in the project files). Choose **Release \| x64** (or another non-dedicated configuration) and build the solution.

The main C++ targets (**DoomDLL**, **Game**, **idLib**, **MayaImport**, **opengl**) compile as **`/std:c++latest`** via [`neo/_CppLanguage.props`](neo/_CppLanguage.props) (CurlLib stays on the compiler default because it builds C sources).

Binaries are written next to the solution directory: **`Doom3.exe`** and **`gamex64.dll`** land in the repository root (one level above `neo/`), matching the `OutputFile` settings in the `.vcxproj` files.

To run the game you still need **legal Doom 3 game data** (for example `base/pak000.pk4` and the rest of the stock packs). Typical options:

- Copy **`Doom3.exe`** and **`gamex64.dll`** into an existing Doom 3 install folder next to the original assets, or  
- Point the engine at that install with **`+set fs_basepath "C:\Path\To\Doom3"`** (see `neo/framework/FileSystem.cpp` for how search paths are built).

The D3D12-backed GL path is wired through the **`opengl`** project and helpers such as [`neo/opengl/gl_d3d12wgl.cpp`](neo/opengl/gl_d3d12wgl.cpp); renderer initialization touches code like [`neo/renderer/RenderSystem_init.cpp`](neo/renderer/RenderSystem_init.cpp).

## Linux / Steam Deck and Vulkan (raster)

**Steam Deck** does not offer practical **hardware ray tracing** for this engine’s DXR-style path. Portable work should prioritize **Vulkan raster** (swapchain, passes, post), **asset and pak loading**, and **media** (see [docs/VULKAN_PLATFORM_STEAMDECK.md](docs/VULKAN_PLATFORM_STEAMDECK.md)).

Quick **Vulkan driver / extension check** (Linux, `libvulkan.so.1`):

```bash
make -C tools/vulkan_caps && ./tools/vulkan_caps/vulkan_caps
```

**Gamepad / Deck sticks (Linux joydev):** see [docs/STEAMDECK_CONTROLS.md](docs/STEAMDECK_CONTROLS.md) (`in_joydev`, axis mapping, `in_joyLookScale`). Starter binds: [base/steamdeck.cfg](base/steamdeck.cfg) (`exec steamdeck.cfg` or `in_joy_autoexec_profile 1`).

## What Is IceBridge?

**IceBridge** is a compatibility and modernization renderer layer for older engines.

It allows legacy rendering paths to be pushed onto modern graphics APIs like **Direct3D 12**, while opening the door for features such as:

- Ray tracing
- Path tracing
- Final gather
- Global illumination
- Denoising
- Improved reflections
- Modern GPU-driven rendering features
- Advanced upscaling support
- Legacy engine preservation

Instead of replacing the original renderer wholesale or rebuilding the game’s art direction, IceBridge works as a bridge between classic rendering code and modern GPU technology.

## Features Shown

- Direct3D 12 rendering
- Path traced lighting
- Final gather global illumination
- Improved shadows
- Improved reflections
- More natural light bounce
- Preserved Doom 3 atmosphere
- IceBridge renderer technology
- Legacy engine modernization

## Why This Matters

Doom 3’s original visual identity is built around darkness, harsh industrial lighting, strong contrast, and atmosphere. Modern rendering can easily destroy that look if applied carelessly.

This project is focused on preserving the original feeling of Doom 3 while enhancing the lighting model underneath it.

Older games do not need to be visually erased to be modernized. With the right renderer layer, they can be preserved, upgraded, and experienced in a new way while still respecting the original art direction.

## Work in Progress

This demo is still a work in progress.

Lighting, denoising, path tracing quality, performance, and compatibility are still being improved. The current goal is to prove that classic idTech 4 rendering can be modernized through IceBridge while keeping the original game recognizable and atmospheric.

## Technology

- Doom 3 / idTech 4
- **SVG textures** — materials can reference **`.svg`** files; they are rasterized at load time (NanoSVG) to RGBA like other images. Tune with **`r_svgScale`** and **`r_svgMaxDimension`** (see `neo/renderer/R_SvgLoad.cpp`).
- **Extra audio codecs** — alongside **`.wav`** / **`.ogg`**, the sound cache will pick up a same-base-name **`.flac`** or **`.mp3`** if present (decoded with [dr_libs](https://github.com/mackron/dr_libs) in `neo/sound/`). Non-44.1/22.05/11.025 kHz sources are resampled to **44.1 kHz** for compatibility with existing sample validation.
- IceBridge renderer layer
- Direct3D 12
- Path tracing
- Final gather
- Global illumination
- Modern GPU rendering pipeline

### Vulkan (experimental)

The compatibility layer is still **D3D12-first** on Windows (including the DXR path-traced lighting stack in `neo/opengl/gl_d3d12raylight.cpp`). On **Linux**, **`r_icebridgeRHI vulkan`** enables an experimental **Vulkan swapchain** alongside the existing **GLX** path: each frame the engine still renders with OpenGL, then acquires a swapchain image, runs a minimal render pass (**clear only**), and **presents**—so you can verify `libvulkan.so.1`, surface, and presentation on Ubuntu / Steam Deck while the full Vulkan GL shim is built out. Use **`d3d12`** only on Windows builds that use IceBridge; on Linux it remains the documented default string but maps to the GLX renderer.

Use the in-engine command **`vkInfo`** (Windows) to verify that `vulkan-1.dll` loads and a minimal `VkInstance` can be created on your machine (diagnostic only).

A future **Vulkan ray tracing** path would port the **`glRaytracing*`** stack—see [docs/VULKAN_RAYTRACING.md](docs/VULKAN_RAYTRACING.md). On **Linux**, you can check whether the driver advertises RT-related extensions with:

```bash
make -C tools/vulkanrt_caps && ./tools/vulkanrt_caps/vulkanrt_caps
```

## About IceBridge

IceBridge is designed to bring modern rendering capabilities to legacy engines without requiring the entire game renderer to be rewritten from scratch.

It is part compatibility layer, part modernization layer, and part preservation tool.

The long-term goal is to allow older games and engines to take advantage of modern GPU APIs and rendering techniques while retaining their original identity.

## Related: RBDOOM-3-BFG

Merging [RBDOOM-3-BFG](https://github.com/RobertBeckebans/RBDOOM-3-BFG) is not a single patch: it targets **Doom 3 BFG** with **NVRHI** (Vulkan + D3D12), while this fork is **classic Doom 3** with **IceBridge (GL on D3D12) + DXR**. See [docs/RBDOOM-3-BFG-PORTING.md](docs/RBDOOM-3-BFG-PORTING.md) for a feature map, merge reality, and phased port strategies (CMake/SDL vs NVRHI vs cherry-picks).

## id Tech 5–style engineering (no MegaTextures)

Useful **id Tech 5–era directions** for this codebase—**multi-core–friendly work**, **streaming I/O**, **GPU-first RHI**, **memory and frame budgets**—are outlined without **virtual texturing / MegaTextures** in [docs/IDTECH5_ASPECTS_NO_MEGATEXTURES.md](docs/IDTECH5_ASPECTS_NO_MEGATEXTURES.md), mapped to existing files (`FileSystem`, renderer images, IceBridge/Vulkan).

## Map districts (`info_location`)

HUD **area names** and optional **district** grouping for `info_location` / `location_separator` are documented in [docs/DISTRICT_SYSTEM.md](docs/DISTRICT_SYSTEM.md) (priority overlap, CVars, console commands).

## AIML 3.0 (experimental Core interpreter)

The [AIML 3.0 draft spec](https://github.com/timfox/aiml-3.0-spec) is vendored as a git submodule at **`third_party/aiml-3.0-spec`**. Clone with submodules:

```bash
git clone --recurse-submodules https://github.com/timfox/idTech4.git
# or, after clone:  git submodule update --init --recursive
```

A **Core Dialog** reference tool (XML + JSON loaders, `*` / `_`, templates including `<srai>`, predicates, etc.) lives under **`tools/aiml3/`** — see [tools/aiml3/README.md](tools/aiml3/README.md).

## Bullet Physics (optional, Windows)

The **[bullet3](https://github.com/bulletphysics/bullet3)** sources are a submodule at **`third_party/bullet3`**. **DoomDLL** can link **static** Bullet libs (`LinearMath`, `BulletCollision`, `BulletDynamics`) built with **`/MT`** / **`/MTd`** to match this solution.

1. `git submodule update --init --recursive`
2. Run **`third_party/build-bullet-msvc.ps1`** (Release and optionally Debug) — see [third_party/bullet/README.md](third_party/bullet/README.md).
3. Rebuild **DoomDLL**; the **`bulletProbe`** console command runs a tiny simulation when Bullet is linked (`ID_HAVE_BULLET`).
