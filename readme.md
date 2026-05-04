# idTech 4

## Overview

This is idTech 4 a fork of **Doom 3 IceBridge**, a Direct3D 12 renderer layer designed to bring modern GPU rendering features to classic idTech 4 technology while keeping the original game’s dark, industrial atmosphere intact.

This demo shows Doom 3 rendered with **Direct3D 12**, **path tracing**, **final gather**, and **global illumination**, adding richer lighting, more natural shadowing, improved reflections, and a deeper sense of atmosphere without rebuilding the game’s original art direction from scratch.

The goal is not to erase Doom 3’s classic look.

The goal is to enhance it with modern rendering techniques while preserving the mood, darkness, contrast, and oppressive atmosphere that made the original game so iconic.

No game assets are included in this fork.

## Building and running

This fork targets **64-bit Windows**. Open [`neo/Doom3.sln`](neo/Doom3.sln) in **Visual Studio** (the solution targets the **v145** MSVC platform toolset and **Windows 10.0** SDK as declared in the project files). Choose **Release \| x64** (or another non-dedicated configuration) and build the solution.

Binaries are written next to the solution directory: **`Doom3.exe`** and **`gamex64.dll`** land in the repository root (one level above `neo/`), matching the `OutputFile` settings in the `.vcxproj` files.

To run the game you still need **legal Doom 3 game data** (for example `base/pak000.pk4` and the rest of the stock packs). Typical options:

- Copy **`Doom3.exe`** and **`gamex64.dll`** into an existing Doom 3 install folder next to the original assets, or  
- Point the engine at that install with **`+set fs_basepath "C:\Path\To\Doom3"`** (see `neo/framework/FileSystem.cpp` for how search paths are built).

The D3D12-backed GL path is wired through the **`opengl`** project and helpers such as [`neo/opengl/gl_d3d12wgl.cpp`](neo/opengl/gl_d3d12wgl.cpp); renderer initialization touches code like [`neo/renderer/RenderSystem_init.cpp`](neo/renderer/RenderSystem_init.cpp).

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
- IceBridge renderer layer
- Direct3D 12
- Path tracing
- Final gather
- Global illumination
- Modern GPU rendering pipeline

## About IceBridge

IceBridge is designed to bring modern rendering capabilities to legacy engines without requiring the entire game renderer to be rewritten from scratch.

It is part compatibility layer, part modernization layer, and part preservation tool.

The long-term goal is to allow older games and engines to take advantage of modern GPU APIs and rendering techniques while retaining their original identity.
