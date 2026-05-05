#ifndef VULKAN_XLIB_H_
#define VULKAN_XLIB_H_ 1

/*
 * Minimal vulkan_xlib.h for engine builds: only VkXlibSurfaceCreateInfoKHR and
 * PFN_vkCreateXlibSurfaceKHR. Full Khronos header pulls Display/Window from X;
 * we include Xlib here so C++ compilation order is simple.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <X11/Xlib.h>

#include "vulkan_core.h"

#define VK_KHR_xlib_surface 1
#define VK_KHR_XLIB_SURFACE_SPEC_VERSION 6
#define VK_KHR_XLIB_SURFACE_EXTENSION_NAME "VK_KHR_xlib_surface"

typedef VkFlags VkXlibSurfaceCreateFlagsKHR;

typedef struct VkXlibSurfaceCreateInfoKHR {
	VkStructureType sType;
	const void *pNext;
	VkXlibSurfaceCreateFlagsKHR flags;
	Display *dpy;
	Window window;
} VkXlibSurfaceCreateInfoKHR;

typedef VkResult ( VKAPI_PTR *PFN_vkCreateXlibSurfaceKHR )( VkInstance instance, const VkXlibSurfaceCreateInfoKHR *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkSurfaceKHR *pSurface );

#ifdef __cplusplus
}
#endif

#endif
