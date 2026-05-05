// Minimal Vulkan bootstrap for IceBridge (dynamic-loaded; no headers required)
#include "icebridge_vulkan.h"
#include "../opengl/icebridge_rhi.h"

#include <stdint.h>
#include <string.h>
#include <stdio.h>

#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  define VK_LIB_NAME L"vulkan-1.dll"
#else
#  include <dlfcn.h>
#  define VK_LIB_NAME "libvulkan.so.1"
#endif

// Vulkan opaque handles and structs (subset)
typedef uint64_t VkFlags;
typedef uint64_t VkDeviceSize;
typedef uint32_t VkBool32;
typedef uint64_t VkInstance;
typedef uint64_t VkPhysicalDevice;
typedef uint64_t VkDevice;
typedef uint64_t VkQueue;
typedef uint64_t VkSurfaceKHR;
typedef int32_t VkResult;

typedef struct VkApplicationInfo {
    const char*     sType; // ignored in our stub
    const void*     pNext;
    const char*     pApplicationName;
    uint32_t        applicationVersion;
    const char*     pEngineName;
    uint32_t        engineVersion;
    uint32_t        apiVersion;
} VkApplicationInfo;

typedef struct VkInstanceCreateInfo {
    const char*             sType; // ignored
    const void*             pNext;
    VkFlags                 flags;
    const VkApplicationInfo* pApplicationInfo;
    uint32_t                enabledLayerCount;
    const char* const*      ppEnabledLayerNames;
    uint32_t                enabledExtensionCount;
    const char* const*      ppEnabledExtensionNames;
} VkInstanceCreateInfo;

typedef struct VkDeviceQueueCreateInfo {
    const char* sType; // ignored
    const void* pNext;
    VkFlags     flags;
    uint32_t    queueFamilyIndex;
    uint32_t    queueCount;
    const float* pQueuePriorities;
} VkDeviceQueueCreateInfo;

typedef struct VkDeviceCreateInfo {
    const char*                 sType; // ignored
    const void*                 pNext;
    VkFlags                     flags;
    uint32_t                    queueCreateInfoCount;
    const VkDeviceQueueCreateInfo* pQueueCreateInfos;
    uint32_t                    enabledLayerCount;
    const char* const*          ppEnabledLayerNames;
    uint32_t                    enabledExtensionCount;
    const char* const*          ppEnabledExtensionNames;
    const void*                 pEnabledFeatures; // ignored
} VkDeviceCreateInfo;

// Function pointer types (subset)
typedef VkResult (*PFN_vkCreateInstance)(const VkInstanceCreateInfo*, const void*, VkInstance*);
typedef void     (*PFN_vkDestroyInstance)(VkInstance, const void*);
typedef VkResult (*PFN_vkEnumeratePhysicalDevices)(VkInstance, uint32_t*, VkPhysicalDevice*);
typedef void     (*PFN_vkGetPhysicalDeviceQueueFamilyProperties)(VkPhysicalDevice, uint32_t*, void*);
typedef VkResult (*PFN_vkCreateDevice)(VkPhysicalDevice, const VkDeviceCreateInfo*, const void*, VkDevice*);
typedef void     (*PFN_vkDestroyDevice)(VkDevice, const void*);
typedef void     (*PFN_vkGetDeviceQueue)(VkDevice, uint32_t, uint32_t, VkQueue*);
typedef void     (*PFN_vkGetPhysicalDeviceProperties)(VkPhysicalDevice, void*);

static struct VulkanDyn {
#if defined(_WIN32)
    HMODULE lib = nullptr;
#else
    void*   lib = nullptr;
#endif
    PFN_vkCreateInstance vkCreateInstance = nullptr;
    PFN_vkDestroyInstance vkDestroyInstance = nullptr;
    PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices = nullptr;
    PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties = nullptr;
    PFN_vkGetPhysicalDeviceProperties vkGetPhysicalDeviceProperties = nullptr;
    PFN_vkCreateDevice vkCreateDevice = nullptr;
    PFN_vkDestroyDevice vkDestroyDevice = nullptr;
    PFN_vkGetDeviceQueue vkGetDeviceQueue = nullptr;
} g_vk;

static VkInstance       g_instance = 0;
static VkPhysicalDevice g_phys = 0;
static VkDevice         g_device = 0;
static uint32_t         g_gfxQueueFamily = 0;
static VkQueue          g_gfxQueue = 0;

static void Log(const char* fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    IceBridge_Log(buf);
}

static void* LoadProc(const char* name) {
#if defined(_WIN32)
    return (void*)GetProcAddress(g_vk.lib, name);
#else
    return dlsym(g_vk.lib, name);
#endif
}

bool IceBridgeVulkan_Init(void) {
    if (g_instance) return true;
#if defined(_WIN32)
    g_vk.lib = LoadLibraryW(VK_LIB_NAME);
#else
    g_vk.lib = dlopen(VK_LIB_NAME, RTLD_NOW | RTLD_LOCAL);
#endif
    if (!g_vk.lib) {
        Log("Vulkan loader not found (%s).", VK_LIB_NAME ? VK_LIB_NAME : "vulkan");
        return false;
    }

    g_vk.vkCreateInstance = (PFN_vkCreateInstance)LoadProc("vkCreateInstance");
    g_vk.vkDestroyInstance = (PFN_vkDestroyInstance)LoadProc("vkDestroyInstance");
    g_vk.vkEnumeratePhysicalDevices = (PFN_vkEnumeratePhysicalDevices)LoadProc("vkEnumeratePhysicalDevices");
    g_vk.vkGetPhysicalDeviceQueueFamilyProperties = (PFN_vkGetPhysicalDeviceQueueFamilyProperties)LoadProc("vkGetPhysicalDeviceQueueFamilyProperties");
    g_vk.vkGetPhysicalDeviceProperties = (PFN_vkGetPhysicalDeviceProperties)LoadProc("vkGetPhysicalDeviceProperties");
    g_vk.vkCreateDevice = (PFN_vkCreateDevice)LoadProc("vkCreateDevice");
    g_vk.vkDestroyDevice = (PFN_vkDestroyDevice)LoadProc("vkDestroyDevice");
    g_vk.vkGetDeviceQueue = (PFN_vkGetDeviceQueue)LoadProc("vkGetDeviceQueue");

    if (!g_vk.vkCreateInstance || !g_vk.vkEnumeratePhysicalDevices || !g_vk.vkCreateDevice) {
        Log("Vulkan functions missing; disabling Vulkan bootstrap.");
        return false;
    }

    VkApplicationInfo app{};
    app.pApplicationName = "IceBridge";
    app.applicationVersion = (1u << 22) | (0u << 12) | 0u; // 1.0.0
    app.pEngineName = "idTech4-IceBridge";
    app.engineVersion = (1u << 22);
    app.apiVersion = (1u << 22); // Vulkan 1.0

    const char* layers[] = {};
    const char* exts[] = {};
    VkInstanceCreateInfo ci{};
    ci.pApplicationInfo = &app;
    ci.enabledLayerCount = 0;
    ci.ppEnabledLayerNames = layers;
    ci.enabledExtensionCount = 0;
    ci.ppEnabledExtensionNames = exts;

    if (g_vk.vkCreateInstance(&ci, nullptr, &g_instance) != 0 || !g_instance) {
        Log("vkCreateInstance failed.");
        return false;
    }

    uint32_t physCount = 0;
    if (g_vk.vkEnumeratePhysicalDevices(g_instance, &physCount, nullptr) != 0 || physCount == 0) {
        Log("No Vulkan physical devices.");
        return false;
    }
    VkPhysicalDevice phys = 0;
    g_vk.vkEnumeratePhysicalDevices(g_instance, &physCount, &phys);
    g_phys = phys;

    // Find a queue family (index 0 is fine for bootstrap; we won't submit work).
    uint32_t qCount = 0;
    g_vk.vkGetPhysicalDeviceQueueFamilyProperties(g_phys, &qCount, nullptr);
    g_gfxQueueFamily = 0;

    float prio = 1.0f;
    VkDeviceQueueCreateInfo qci{};
    qci.queueFamilyIndex = g_gfxQueueFamily;
    qci.queueCount = 1;
    qci.pQueuePriorities = &prio;

    VkDeviceCreateInfo dci{};
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    if (g_vk.vkCreateDevice(g_phys, &dci, nullptr, &g_device) != 0 || !g_device) {
        Log("vkCreateDevice failed.");
        return false;
    }
    g_vk.vkGetDeviceQueue(g_device, g_gfxQueueFamily, 0, &g_gfxQueue);

    Log("Vulkan bootstrap initialized (instance, device).");
    return true;
}

void IceBridgeVulkan_LogCaps(void) {
    if (!g_instance || !g_phys) {
        Log("Vulkan not initialized.");
        return;
    }
    // We don't include headers; print minimal confirmation.
    Log("Vulkan: instance=%p device=%p queueFamily=%u", (void*)g_instance, (void*)g_device, (unsigned)g_gfxQueueFamily);
}

void IceBridgeVulkan_Shutdown(void) {
    if (g_device && g_vk.vkDestroyDevice) {
        g_vk.vkDestroyDevice(g_device, nullptr);
        g_device = 0;
    }
    if (g_instance && g_vk.vkDestroyInstance) {
        g_vk.vkDestroyInstance(g_instance, nullptr);
        g_instance = 0;
    }
}

