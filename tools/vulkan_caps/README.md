# vulkan_caps

Small **Vulkan loader probe** for **raster / presentation** extension strings (swapchain, WSI, dynamic rendering, sync2, etc.). Intended for **Linux / Steam Deck** planning where **ray tracing is not** the target.

```bash
make -C tools/vulkan_caps
./tools/vulkan_caps/vulkan_caps
```

Requires `libvulkan.so.1` (Mesa or vendor ICD). In headless CI, `vkCreateInstance` may fail with no ICD — that is expected.
