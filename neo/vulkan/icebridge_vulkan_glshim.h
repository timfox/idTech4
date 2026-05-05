// IceBridge Vulkan GL-compatibility shim (skeleton)
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct QVulkanWindow;

// Create/init Vulkan for a native window; returns an opaque window object or NULL on failure.
struct QVulkanWindow* QVK_InitForWindow(void* hwnd, int width, int height);

// Shutdown and free resources for a window.
void QVK_ShutdownForWindow(struct QVulkanWindow* w);

// Resize swapchain/resources; returns false on fatal failures.
bool QVK_Resize(struct QVulkanWindow* w, int width, int height);

// Begin a frame; returns false on failure.
bool QVK_BeginFrame(struct QVulkanWindow* w);

// End the frame and submit work; returns false on failure.
bool QVK_EndFrame(struct QVulkanWindow* w);

// Present the last submitted frame; returns false on failure.
bool QVK_Present(struct QVulkanWindow* w);

#ifdef __cplusplus
}
#endif

