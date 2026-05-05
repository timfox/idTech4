# vulkan_caps

Small **Vulkan loader probe** for **raster / presentation** extension strings (swapchain, WSI, dynamic rendering, sync2, etc.). Intended for **Linux / Steam Deck** planning where **ray tracing is not** the target.

```bash
make -C tools/vulkan_caps
./tools/vulkan_caps/vulkan_caps
```

Requires `libvulkan.so.1` (Mesa or vendor ICD), or on macOS a MoltenVK-backed loader (`libvulkan.1.dylib` / `libvulkan.dylib`). When **`CI`** or **`GITHUB_ACTIONS`** is set and the loader or ICD is missing, the tool exits **0** so GitHub Actions can stay green without a GPU.
