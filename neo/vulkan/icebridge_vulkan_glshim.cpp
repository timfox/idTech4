// IceBridge Vulkan GL-compatibility shim (skeleton)
#include "icebridge_vulkan_glshim.h"
#include "icebridge_vulkan.h"
#include "../opengl/icebridge_rhi.h"

#include <stdint.h>
#include <string.h>

struct QVulkanWindow {
    void*   hwnd = nullptr;
    int     width = 0;
    int     height = 0;
    bool    inited = false;
};

static void QVK_Log(const char* msg) {
    IceBridge_Log(msg);
}

struct QVulkanWindow* QVK_InitForWindow(void* hwnd, int width, int height) {
    (void)hwnd;
    if (!IceBridgeVulkan_Init()) {
        QVK_Log("QVK_InitForWindow: Vulkan bootstrap failed.\n");
        return nullptr;
    }
    QVulkanWindow* w = new QVulkanWindow();
    w->hwnd = hwnd;
    w->width = width;
    w->height = height;
    w->inited = true;
    QVK_Log("QVK_InitForWindow: initialized Vulkan shim skeleton.\n");
    return w;
}

void QVK_ShutdownForWindow(struct QVulkanWindow* w) {
    if (!w) return;
    IceBridgeVulkan_Shutdown();
    delete w;
}

bool QVK_Resize(struct QVulkanWindow* w, int width, int height) {
    if (!w || !w->inited) return false;
    w->width = width;
    w->height = height;
    // TODO: recreate swapchain when implemented
    return true;
}

bool QVK_BeginFrame(struct QVulkanWindow* w) {
    if (!w || !w->inited) return false;
    // TODO: acquire next image when implemented
    return true;
}

bool QVK_EndFrame(struct QVulkanWindow* w) {
    if (!w || !w->inited) return false;
    // TODO: record/submit command buffers when implemented
    return true;
}

bool QVK_Present(struct QVulkanWindow* w) {
    if (!w || !w->inited) return false;
    // TODO: present queue when implemented
    return true;
}

