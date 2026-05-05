# Building and running on Linux with Vulkan (experimental)

This document covers **native Linux** builds of this **classic Doom 3 / id Tech 4** fork and how to use the **experimental Vulkan presentation path** (`r_icebridgeRHI vulkan`). It does **not** describe **Doom 3 BFG** (NVRHI/SDL); for BFG-style ports see [RBDOOM-3-BFG-PORTING.md](RBDOOM-3-BFG-PORTING.md).

---

## 1. What “Vulkan” means here today

| Layer | Role |
|--------|------|
| **OpenGL + GLX** | Full game rendering (meshes, materials, lights) as in stock Linux id Tech 4. |
| **Vulkan (optional)** | When `r_icebridgeRHI` is set to **`vulkan`**, the engine creates a **Vulkan swapchain** on the **X11 window**, clears each swap image, and **presents** every frame **in addition to** `glXSwapBuffers`. This validates **loader, surface, device, and present** on Ubuntu / Steam Deck while a full **GL-on-Vulkan** shim is developed. |

So you are **building one Linux binary**; “Vulkan mode” is a **runtime CVar**, not a second target.

Implementation pointers:

- Swapchain / present: `neo/renderer/RHI/icebridge_vulkan_linux.cpp`
- Hook after GLX window exists: `neo/sys/linux/glimp.cpp` (`GLX_Init`, `GLimp_SwapBuffers`, `GLimp_Shutdown`)
- Link line: `neo/sys/scons/SConscript.core` appends **`vulkan`** to `LIBS` for non-dedicated builds.

---

## 2. Prerequisites (Debian / Ubuntu example)

Install a **C++ toolchain**, **X11 dev libraries**, **ALSA** (for sound), **curl** (if enabled), and **Vulkan**:

```bash
sudo apt update
sudo apt install -y \
  build-essential \
  libsdl2-dev \
  libx11-dev libxext-dev libxxf86vm-dev \
  libasound2-dev \
  libcurl4-openssl-dev \
  libvulkan1 mesa-vulkan-drivers vulkan-tools
```

- **`libvulkan1`** provides `libvulkan.so.1` (the loader).
- **`mesa-vulkan-drivers`** (or your GPU vendor’s Vulkan driver package) provides the **ICD** so `vkCreateInstance` succeeds.
- **`vulkan-tools`** supplies `vulkaninfo` for a quick sanity check.

Other distros: install the equivalent **Vulkan loader + driver + X11 development** packages.

---

## 3. Build system (SCons)

The upstream **id Tech 4** Linux port used **SCons** with a top-level `SConstruct` under the game **`src/`** tree. This repository retains SCons scripts under **`neo/sys/scons/`** (for example `SConscript.core`). If your checkout does not include a root **`SConstruct`** that wires those scripts, you may need to **restore or add** the classic SCons entry point from the **Doom 3 GPL SDK** or your own wrapper so that `scons` can be run from the directory that SCons expects (historically the **`src/`** or **`neo/`** root, depending on layout).

Once SCons is configured for your tree:

```bash
# Example — run from the directory that contains SConstruct and neo/
scons -j"$(nproc)"
```

The produced client binary is typically named **`doom`** (see `SConscript.core`: `Program( target = 'doom', ... )`). Exact output path depends on your SConstruct.

**Dedicated server** builds omit GLX and the Vulkan present hook; use a normal (non-dedicated) client build to test Vulkan.

---

## 4. Verifying Vulkan before launching the game

1. **Loader and ICD**

   ```bash
   vulkaninfo --summary
   ```

   You should see your GPU and a non-zero API version. If this fails, fix drivers/ICD before debugging the engine.

2. **Optional: small probe built from this repo**

   ```bash
   make -C tools/vulkan_caps && ./tools/vulkan_caps/vulkan_caps
   ```

   On headless CI, the tool may exit successfully with a “no ICD” message; on a desktop with Mesa/NVIDIA/AMD, it should report instance/device creation.

---

## 5. Running with Vulkan presentation enabled

1. Install **legal Doom 3 base assets** (`base/pak000.pk4`, etc.) and point the engine at them with **`fs_basepath`** (same as Windows).

2. Start the Linux binary with the CVar:

   ```bash
   ./doom +set r_icebridgeRHI vulkan +set fs_basepath /path/to/your/doom3/install
   ```

3. **If initialization fails**, the engine prints a warning and continues **GLX-only** (no crash loop). Check:

   - `vulkaninfo` works.
   - You are on **X11** (this path uses **`VK_KHR_xlib_surface`**). Wayland-only sessions may need XWayland or a future Wayland surface extension.

4. **What you should see**: the window still shows the **OpenGL-rendered** frame; the Vulkan path currently **clears** the swapchain to a solid color and presents **before** `glXSwapBuffers`. Until the GL shim draws into Vulkan, the visible image is still dominated by GL compositing behavior on your driver—treat this as **infrastructure validation**, not final visuals.

---

## 6. Troubleshooting

| Symptom | Things to check |
|---------|------------------|
| `dlopen(libvulkan.so.1) failed` | Install `libvulkan1`; `ldconfig -p \| grep vulkan`. |
| `vkCreateInstance failed` | Run `vulkaninfo`; install Mesa or vendor Vulkan driver. |
| `vkCreateXlibSurfaceKHR failed` | Display must be X11 (`DISPLAY` set); window must exist (after `GLX_Init`). |
| `vkCreateDevice failed` / no queue | Driver does not expose `VK_KHR_swapchain` or present on the chosen queue family—update driver. |
| Build fails linking `-lvulkan` | Install `libvulkan-dev` (Debian) or the package that provides **`libvulkan.so`** for the linker. |

---

## 7. Steam Deck notes

- Deck uses **Gamescope** / **Wayland** in the default session; **XWayland** often provides the X11 `Display` the engine opens. If surface creation fails, try an explicit **X11 session** or confirm XWayland is active.
- Prefer **`+set r_icebridgeRHI vulkan`** only after `vulkaninfo` works on the device.
- Controller notes: [STEAMDECK_CONTROLS.md](STEAMDECK_CONTROLS.md).

---

## 8. Relation to “full Vulkan renderer”

Next engineering steps (not all implemented) would include:

- Wayland `VkSurfaceKHR` (e.g. `VK_KHR_wayland_surface`) where X11 is unavailable.
- Optional: skip `glXSwapBuffers` when Vulkan owns the window, or composite GL into a Vulkan texture.
- Port render passes and resource binding so **draw calls** hit Vulkan instead of only a clear pass.

Track high-level goals in [VULKAN_PLATFORM_STEAMDECK.md](VULKAN_PLATFORM_STEAMDECK.md).
