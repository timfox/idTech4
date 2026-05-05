# Vulkan + ray tracing on this fork (roadmap)

This repository’s **real-time ray traced lighting / GI** path today is implemented as **Direct3D 12 + DXR** inside the IceBridge layer (`neo/opengl/gl_d3d12raylight.cpp`), driven by the **`glRaytracing*`** API declared in `neo/opengl/opengl.h` and called from the renderer (`draw_dx.cpp`, `Model.cpp`, etc.).

**There is no Vulkan swapchain, no Vulkan command submission, and no Vulkan acceleration-structure build** wired to `glRaytracing*` yet. Running “Doom 3 on Vulkan with RT” from this tree means **porting that entire stack** (or replacing it with something like NVRHI as in RBDOOM-3-BFG).

---

## 1. What must be built for Vulkan RT

Rough dependency order:

1. **Vulkan instance + surface + swapchain** (platform: **X11/Wayland** on Linux, Win32 on Windows).
2. **Vulkan 1.2+** features as required by your chosen RT extension set (commonly **buffer device address**, **descriptor indexing**, **timeline semaphores** / **synchronization2** — match what your shaders and tooling assume).
3. **Acceleration structures** (`VK_KHR_acceleration_structure`) — map from `glRaytracingCreateMesh` / instances / TLAS.
4. **Ray tracing pipeline or ray query** — map from `glRaytracingLighting*` dispatch (today: DXR state objects + shader tables).
5. **Interop with the existing GL shim** — either:
   - implement **Vulkan** inside the same “fake GL” layer as an alternative to `QD3D12_*`, or  
   - **replace** the GL shim with a native backend (multi-year).

---

## 2. Capability probe (Linux-friendly)

Before writing thousands of lines of device code, verify the **GPU + driver** exposes the extensions you need.

### Standalone tool

From the repo root on **Linux** (needs `libvulkan.so.1` — Mesa/NVIDIA/AMD proprietary all ship a loader):

```bash
make -C tools/vulkanrt_caps
./tools/vulkanrt_caps/vulkanrt_caps
```

The tool prints, for each physical device, whether common RT-related **device extensions** are reported (e.g. `VK_KHR_acceleration_structure`, `VK_KHR_ray_tracing_pipeline`). **“yes”** means the extension string is present; you still need feature bits, queues, and a working pipeline.

---

## 3. Relation to RBDOOM-3-BFG

RBDOOM-3-BFG achieves Vulkan + modern raster via **NVRHI** and a **BFG-based** renderer. Porting that here is a **separate architecture decision** (see [RBDOOM-3-BFG-PORTING.md](RBDOOM-3-BFG-PORTING.md) if present in your branch).

For **Vulkan ray tracing specifically**, RB is still largely a **raster PBR** engine unless you add RT passes yourself—the value is **cross-platform RHI**, not a drop-in “Doom 3 RT on Vulkan” from one file.

---

## 4. Suggested next code milestones (in this fork)

1. **`vkInfo` / caps** integrated or extended in-engine on Linux builds (once CMake + POSIX GLimp exist).
2. **Vulkan instance only** in a new `neo/opengl/` module, selected by `r_icebridgeRHI` (no draw).
3. **Swapchain + clear color** (prove frame pacing).
4. **BLAS from `glRaytracingMeshDesc_t`** (geometry already uploaded for DXR).
5. **TLAS + `vkCmdTraceRaysKHR`** parity with current lighting pass.

Each step should remain buildable on Windows (DX12 path) until Vulkan reaches parity.
