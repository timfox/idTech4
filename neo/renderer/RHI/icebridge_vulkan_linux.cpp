/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company.

Linux + X11: Vulkan swapchain when r_icebridgeRHI=vulkan. Dynamically loads
libvulkan.so.1. Each frame: acquire → begin render pass (loadOp CLEAR) → end
→ submit → present. OpenGL still runs on GLX; this path presents on top.

===========================================================================
*/

#include "../../idlib/precompiled.h"
#pragma hdrstop

#if defined( __linux__ )

#include "../../sys/linux/local.h"
#include "../../renderer/tr_local.h"
#include "../../opengl/icebridge_rhi.h"
#include "icebridge_vulkan_linux.h"

#include <dlfcn.h>

#define VK_USE_PLATFORM_XLIB_KHR
#include "third_party/vulkan/vulkan_core.h"
#include "third_party/vulkan/vulkan_xlib.h"

namespace {

struct IceVk {
	bool active = false;

	Display *dpy = nullptr;
	Window xwin = 0;
	int width = 0;
	int height = 0;

	void *lib = nullptr;
	PFN_vkGetInstanceProcAddr pfnGipa = nullptr;
	PFN_vkGetDeviceProcAddr pfnGdpa = nullptr;

	VkInstance instance = VK_NULL_HANDLE;
	VkPhysicalDevice phys = VK_NULL_HANDLE;
	uint32_t gfxQueueFamily = 0;
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VkDevice device = VK_NULL_HANDLE;
	VkQueue queue = VK_NULL_HANDLE;

	VkSwapchainKHR swapchain = VK_NULL_HANDLE;
	VkFormat swapFormat = VK_FORMAT_UNDEFINED;
	VkExtent2D swapExtent{};
	uint32_t swapImageCount = 0;
	idList<VkImage> swapImages;
	idList<VkImageView> swapViews;
	idList<VkFramebuffer> swapFramebuffers;

	VkRenderPass renderPass = VK_NULL_HANDLE;
	VkCommandPool cmdPool = VK_NULL_HANDLE;
	idList<VkCommandBuffer> cmdBufs;

	VkSemaphore imageAvailable = VK_NULL_HANDLE;
	VkSemaphore renderFinished = VK_NULL_HANDLE;
	VkFence inFlight = VK_NULL_HANDLE;

	PFN_vkDestroyInstance vkDestroyInstance = nullptr;
	PFN_vkDestroySurfaceKHR vkDestroySurfaceKHR = nullptr;
	PFN_vkDestroyDevice vkDestroyDevice = nullptr;
	PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR = nullptr;
	PFN_vkDestroyImageView vkDestroyImageView = nullptr;
	PFN_vkDestroyRenderPass vkDestroyRenderPass = nullptr;
	PFN_vkDestroyFramebuffer vkDestroyFramebuffer = nullptr;
	PFN_vkDestroyCommandPool vkDestroyCommandPool = nullptr;
	PFN_vkDestroySemaphore vkDestroySemaphore = nullptr;
	PFN_vkDestroyFence vkDestroyFence = nullptr;
	PFN_vkDeviceWaitIdle vkDeviceWaitIdle = nullptr;
	PFN_vkQueueSubmit vkQueueSubmit = nullptr;
	PFN_vkQueuePresentKHR vkQueuePresentKHR = nullptr;
	PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR = nullptr;
	PFN_vkWaitForFences vkWaitForFences = nullptr;
	PFN_vkResetFences vkResetFences = nullptr;
	PFN_vkResetCommandBuffer vkResetCommandBuffer = nullptr;
	PFN_vkBeginCommandBuffer vkBeginCommandBuffer = nullptr;
	PFN_vkEndCommandBuffer vkEndCommandBuffer = nullptr;
	PFN_vkCmdBeginRenderPass vkCmdBeginRenderPass = nullptr;
	PFN_vkCmdEndRenderPass vkCmdEndRenderPass = nullptr;
};

static IceVk g;

static void vk_shutdown_swapchain() {
	if ( !g.device ) {
		return;
	}
	if ( g.vkDeviceWaitIdle ) {
		g.vkDeviceWaitIdle( g.device );
	}
	for ( int i = 0; i < g.swapFramebuffers.Num(); ++i ) {
		if ( g.swapFramebuffers[i] && g.vkDestroyFramebuffer ) {
			g.vkDestroyFramebuffer( g.device, g.swapFramebuffers[i], nullptr );
		}
	}
	g.swapFramebuffers.Clear();
	for ( int i = 0; i < g.swapViews.Num(); ++i ) {
		if ( g.swapViews[i] && g.vkDestroyImageView ) {
			g.vkDestroyImageView( g.device, g.swapViews[i], nullptr );
		}
	}
	g.swapViews.Clear();
	g.swapImages.Clear();
	if ( g.swapchain && g.vkDestroySwapchainKHR ) {
		g.vkDestroySwapchainKHR( g.device, g.swapchain, nullptr );
		g.swapchain = VK_NULL_HANDLE;
	}
}

static void vk_shutdown_all() {
	vk_shutdown_swapchain();
	if ( g.cmdBufs.Num() && g.device && g.vkDestroyCommandPool ) {
		g.vkDestroyCommandPool( g.device, g.cmdPool, nullptr );
	}
	g.cmdBufs.Clear();
	g.cmdPool = VK_NULL_HANDLE;
	if ( g.renderPass && g.device && g.vkDestroyRenderPass ) {
		g.vkDestroyRenderPass( g.device, g.renderPass, nullptr );
		g.renderPass = VK_NULL_HANDLE;
	}
	if ( g.imageAvailable && g.device && g.vkDestroySemaphore ) {
		g.vkDestroySemaphore( g.device, g.imageAvailable, nullptr );
		g.imageAvailable = VK_NULL_HANDLE;
	}
	if ( g.renderFinished && g.device && g.vkDestroySemaphore ) {
		g.vkDestroySemaphore( g.device, g.renderFinished, nullptr );
		g.renderFinished = VK_NULL_HANDLE;
	}
	if ( g.inFlight && g.device && g.vkDestroyFence ) {
		g.vkDestroyFence( g.device, g.inFlight, nullptr );
		g.inFlight = VK_NULL_HANDLE;
	}
	if ( g.device && g.vkDestroyDevice ) {
		g.vkDestroyDevice( g.device, nullptr );
		g.device = VK_NULL_HANDLE;
		g.queue = VK_NULL_HANDLE;
	}
	g.pfnGdpa = nullptr;
	if ( g.surface && g.instance && g.vkDestroySurfaceKHR ) {
		g.vkDestroySurfaceKHR( g.instance, g.surface, nullptr );
		g.surface = VK_NULL_HANDLE;
	}
	if ( g.instance && g.vkDestroyInstance ) {
		g.vkDestroyInstance( g.instance, nullptr );
		g.instance = VK_NULL_HANDLE;
	}
	g.phys = VK_NULL_HANDLE;
	g.active = false;
	if ( g.lib ) {
		dlclose( g.lib );
		g.lib = nullptr;
		g.pfnGipa = nullptr;
	} else {
		g.pfnGipa = nullptr;
	}
}

static bool vk_load_lib() {
	if ( g.lib ) {
		return true;
	}
	g.lib = dlopen( "libvulkan.so.1", RTLD_NOW | RTLD_LOCAL );
	if ( !g.lib ) {
		common->Printf( "IceBridge Vulkan: dlopen(libvulkan.so.1) failed: %s\n", dlerror() ? dlerror() : "" );
		return false;
	}
	g.pfnGipa = (PFN_vkGetInstanceProcAddr)dlsym( g.lib, "vkGetInstanceProcAddr" );
	if ( !g.pfnGipa ) {
		common->Printf( "IceBridge Vulkan: vkGetInstanceProcAddr missing.\n" );
		dlclose( g.lib );
		g.lib = nullptr;
		return false;
	}
	return true;
}

static bool vk_create_swapchain_and_pass() {
	if ( !g.device || !g.pfnGdpa ) {
		return false;
	}
	PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR =
		(PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR)g.pfnGipa( g.instance, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR" );
	PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetPhysicalDeviceSurfaceFormatsKHR =
		(PFN_vkGetPhysicalDeviceSurfaceFormatsKHR)g.pfnGipa( g.instance, "vkGetPhysicalDeviceSurfaceFormatsKHR" );
	PFN_vkGetPhysicalDeviceSurfacePresentModesKHR vkGetPhysicalDeviceSurfacePresentModesKHR =
		(PFN_vkGetPhysicalDeviceSurfacePresentModesKHR)g.pfnGipa( g.instance, "vkGetPhysicalDeviceSurfacePresentModesKHR" );
	PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR =
		(PFN_vkCreateSwapchainKHR)g.pfnGdpa( g.device, "vkCreateSwapchainKHR" );
	PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR =
		(PFN_vkGetSwapchainImagesKHR)g.pfnGdpa( g.device, "vkGetSwapchainImagesKHR" );
	PFN_vkCreateImageView vkCreateImageView =
		(PFN_vkCreateImageView)g.pfnGdpa( g.device, "vkCreateImageView" );
	PFN_vkCreateRenderPass vkCreateRenderPass =
		(PFN_vkCreateRenderPass)g.pfnGdpa( g.device, "vkCreateRenderPass" );
	PFN_vkCreateFramebuffer vkCreateFramebuffer =
		(PFN_vkCreateFramebuffer)g.pfnGdpa( g.device, "vkCreateFramebuffer" );
	PFN_vkCreateCommandPool vkCreateCommandPool =
		(PFN_vkCreateCommandPool)g.pfnGdpa( g.device, "vkCreateCommandPool" );
	PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers =
		(PFN_vkAllocateCommandBuffers)g.pfnGdpa( g.device, "vkAllocateCommandBuffers" );
	PFN_vkBeginCommandBuffer vkBeginCommandBuffer =
		(PFN_vkBeginCommandBuffer)g.pfnGdpa( g.device, "vkBeginCommandBuffer" );
	PFN_vkEndCommandBuffer vkEndCommandBuffer =
		(PFN_vkEndCommandBuffer)g.pfnGdpa( g.device, "vkEndCommandBuffer" );
	PFN_vkCmdBeginRenderPass vkCmdBeginRenderPass =
		(PFN_vkCmdBeginRenderPass)g.pfnGdpa( g.device, "vkCmdBeginRenderPass" );
	PFN_vkCmdEndRenderPass vkCmdEndRenderPass =
		(PFN_vkCmdEndRenderPass)g.pfnGdpa( g.device, "vkCmdEndRenderPass" );

	g.vkDestroySwapchainKHR = (PFN_vkDestroySwapchainKHR)g.pfnGdpa( g.device, "vkDestroySwapchainKHR" );
	g.vkDestroyImageView = (PFN_vkDestroyImageView)g.pfnGdpa( g.device, "vkDestroyImageView" );
	g.vkDestroyFramebuffer = (PFN_vkDestroyFramebuffer)g.pfnGdpa( g.device, "vkDestroyFramebuffer" );
	g.vkDestroyRenderPass = (PFN_vkDestroyRenderPass)g.pfnGdpa( g.device, "vkDestroyRenderPass" );
	g.vkDestroyCommandPool = (PFN_vkDestroyCommandPool)g.pfnGdpa( g.device, "vkDestroyCommandPool" );
	g.vkDeviceWaitIdle = (PFN_vkDeviceWaitIdle)g.pfnGdpa( g.device, "vkDeviceWaitIdle" );
	g.vkQueueSubmit = (PFN_vkQueueSubmit)g.pfnGdpa( g.device, "vkQueueSubmit" );
	g.vkQueuePresentKHR = (PFN_vkQueuePresentKHR)g.pfnGdpa( g.device, "vkQueuePresentKHR" );
	g.vkAcquireNextImageKHR = (PFN_vkAcquireNextImageKHR)g.pfnGdpa( g.device, "vkAcquireNextImageKHR" );
	g.vkWaitForFences = (PFN_vkWaitForFences)g.pfnGdpa( g.device, "vkWaitForFences" );
	g.vkResetFences = (PFN_vkResetFences)g.pfnGdpa( g.device, "vkResetFences" );
	g.vkResetCommandBuffer = (PFN_vkResetCommandBuffer)g.pfnGdpa( g.device, "vkResetCommandBuffer" );

	if ( !vkGetPhysicalDeviceSurfaceCapabilitiesKHR || !vkGetPhysicalDeviceSurfaceFormatsKHR ||
		 !vkGetPhysicalDeviceSurfacePresentModesKHR || !vkCreateSwapchainKHR || !vkGetSwapchainImagesKHR ||
		 !vkCreateImageView || !vkCreateRenderPass || !vkCreateFramebuffer || !vkCreateCommandPool ||
		 !vkAllocateCommandBuffers || !vkBeginCommandBuffer || !vkEndCommandBuffer ||
		 !vkCmdBeginRenderPass || !vkCmdEndRenderPass ) {
		common->Printf( "IceBridge Vulkan: missing device/instance extension entry points.\n" );
		return false;
	}

	VkSurfaceCapabilitiesKHR caps{};
	if ( vkGetPhysicalDeviceSurfaceCapabilitiesKHR( g.phys, g.surface, &caps ) != VK_SUCCESS ) {
		return false;
	}

	VkExtent2D extent = caps.currentExtent;
	if ( extent.width == UINT32_MAX || extent.height == UINT32_MAX ) {
		extent.width = (uint32_t)idMath::ClampInt( g.width, (int)caps.minImageExtent.width, (int)caps.maxImageExtent.width );
		extent.height = (uint32_t)idMath::ClampInt( g.height, (int)caps.minImageExtent.height, (int)caps.maxImageExtent.height );
	}

	uint32_t fmtCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR( g.phys, g.surface, &fmtCount, nullptr );
	if ( fmtCount == 0 ) {
		return false;
	}
	idList<VkSurfaceFormatKHR> formats;
	formats.SetNum( fmtCount );
	vkGetPhysicalDeviceSurfaceFormatsKHR( g.phys, g.surface, &fmtCount, formats.Ptr() );
	VkSurfaceFormatKHR fmtPick = formats[0];
	for ( int i = 0; i < formats.Num(); ++i ) {
		if ( formats[i].format == VK_FORMAT_B8G8R8A8_UNORM || formats[i].format == VK_FORMAT_B8G8R8A8_SRGB ) {
			fmtPick = formats[i];
			break;
		}
	}

	VkPresentModeKHR presentModes[16];
	uint32_t pmCount = 16;
	vkGetPhysicalDeviceSurfacePresentModesKHR( g.phys, g.surface, &pmCount, presentModes );
	VkPresentModeKHR presentMode = presentModes[0];
	for ( uint32_t i = 0; i < pmCount; ++i ) {
		if ( presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR ) {
			presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
			break;
		}
	}

	uint32_t minImg = caps.minImageCount + 1;
	if ( caps.maxImageCount > 0 && minImg > caps.maxImageCount ) {
		minImg = caps.maxImageCount;
	}

	VkSwapchainCreateInfoKHR sci{};
	sci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	sci.surface = g.surface;
	sci.minImageCount = minImg;
	sci.imageFormat = fmtPick.format;
	sci.imageColorSpace = fmtPick.colorSpace;
	sci.imageExtent = extent;
	sci.imageArrayLayers = 1;
	sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	sci.preTransform = caps.currentTransform;
	sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	sci.presentMode = presentMode;
	sci.clipped = VK_TRUE;
	sci.oldSwapchain = VK_NULL_HANDLE;

	if ( vkCreateSwapchainKHR( g.device, &sci, nullptr, &g.swapchain ) != VK_SUCCESS ) {
		return false;
	}
	g.swapFormat = fmtPick.format;
	g.swapExtent = extent;

	uint32_t imgN = 0;
	vkGetSwapchainImagesKHR( g.device, g.swapchain, &imgN, nullptr );
	g.swapImages.SetNum( imgN );
	vkGetSwapchainImagesKHR( g.device, g.swapchain, &imgN, g.swapImages.Ptr() );
	g.swapImageCount = imgN;

	g.swapViews.SetNum( imgN );
	g.swapFramebuffers.SetNum( imgN );

	VkAttachmentDescription ad{};
	ad.format = g.swapFormat;
	ad.samples = VK_SAMPLE_COUNT_1_BIT;
	ad.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	ad.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	ad.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	ad.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	ad.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	ad.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference ar{};
	ar.attachment = 0;
	ar.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription sp{};
	sp.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	sp.colorAttachmentCount = 1;
	sp.pColorAttachments = &ar;

	VkSubpassDependency dep{};
	dep.srcSubpass = VK_SUBPASS_EXTERNAL;
	dep.dstSubpass = 0;
	dep.srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo rpci{};
	rpci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	rpci.attachmentCount = 1;
	rpci.pAttachments = &ad;
	rpci.subpassCount = 1;
	rpci.pSubpasses = &sp;
	rpci.dependencyCount = 1;
	rpci.pDependencies = &dep;

	if ( vkCreateRenderPass( g.device, &rpci, nullptr, &g.renderPass ) != VK_SUCCESS ) {
		return false;
	}

	VkImageViewCreateInfo ivTemplate{};
	ivTemplate.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	ivTemplate.viewType = VK_IMAGE_VIEW_TYPE_2D;
	ivTemplate.format = g.swapFormat;
	ivTemplate.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	ivTemplate.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	ivTemplate.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	ivTemplate.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	ivTemplate.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	ivTemplate.subresourceRange.levelCount = 1;
	ivTemplate.subresourceRange.layerCount = 1;

	for ( uint32_t i = 0; i < imgN; ++i ) {
		ivTemplate.image = g.swapImages[i];
		if ( vkCreateImageView( g.device, &ivTemplate, nullptr, &g.swapViews[i] ) != VK_SUCCESS ) {
			return false;
		}
		VkFramebufferCreateInfo fb{};
		fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fb.renderPass = g.renderPass;
		fb.attachmentCount = 1;
		fb.pAttachments = &g.swapViews[i];
		fb.width = extent.width;
		fb.height = extent.height;
		fb.layers = 1;
		if ( vkCreateFramebuffer( g.device, &fb, nullptr, &g.swapFramebuffers[i] ) != VK_SUCCESS ) {
			return false;
		}
	}

	VkCommandPoolCreateInfo cpci{};
	cpci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cpci.queueFamilyIndex = g.gfxQueueFamily;
	if ( vkCreateCommandPool( g.device, &cpci, nullptr, &g.cmdPool ) != VK_SUCCESS ) {
		return false;
	}

	VkCommandBufferAllocateInfo cbai{};
	cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cbai.commandPool = g.cmdPool;
	cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cbai.commandBufferCount = imgN;
	g.cmdBufs.SetNum( imgN );
	if ( vkAllocateCommandBuffers( g.device, &cbai, g.cmdBufs.Ptr() ) != VK_SUCCESS ) {
		return false;
	}

	VkClearValue clear{};
	clear.color.float32[0] = 0.12f;
	clear.color.float32[1] = 0.15f;
	clear.color.float32[2] = 0.25f;
	clear.color.float32[3] = 1.0f;

	for ( uint32_t i = 0; i < imgN; ++i ) {
		VkCommandBufferBeginInfo bi{};
		bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		bi.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
		if ( vkBeginCommandBuffer( g.cmdBufs[i], &bi ) != VK_SUCCESS ) {
			return false;
		}
		VkRenderPassBeginInfo rp{};
		rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		rp.renderPass = g.renderPass;
		rp.framebuffer = g.swapFramebuffers[i];
		rp.renderArea.offset = { 0, 0 };
		rp.renderArea.extent = extent;
		rp.clearValueCount = 1;
		rp.pClearValues = &clear;
		vkCmdBeginRenderPass( g.cmdBufs[i], &rp, VK_SUBPASS_CONTENTS_INLINE );
		vkCmdEndRenderPass( g.cmdBufs[i] );
		if ( vkEndCommandBuffer( g.cmdBufs[i] ) != VK_SUCCESS ) {
			return false;
		}
	}

	return true;
}

} // namespace

bool IceBridgeVulkanLinux_Init( Display *dpy, Window xwin, int width, int height ) {
	IceBridge_RefreshRHIFromCvar();
	if ( QIceBridge_GetRequestedRHI() != QICEBRIDGE_RHI_Vulkan ) {
		return false;
	}

	if ( g.active ) {
		IceBridgeVulkanLinux_Shutdown();
	}

	if ( !vk_load_lib() ) {
		QIceBridge_SetActiveRHI( QICEBRIDGE_RHI_D3D12 );
		return false;
	}

	g.dpy = dpy;
	g.xwin = xwin;
	g.width = width;
	g.height = height;

	PFN_vkCreateInstance pfnCreateInstance = (PFN_vkCreateInstance)g.pfnGipa( VK_NULL_HANDLE, "vkCreateInstance" );
	g.vkDestroyInstance = (PFN_vkDestroyInstance)g.pfnGipa( VK_NULL_HANDLE, "vkDestroyInstance" );
	PFN_vkEnumeratePhysicalDevices pfnEnumeratePhysicalDevices =
		(PFN_vkEnumeratePhysicalDevices)g.pfnGipa( VK_NULL_HANDLE, "vkEnumeratePhysicalDevices" );
	PFN_vkGetPhysicalDeviceQueueFamilyProperties pfnGetPhysicalDeviceQueueFamilyProperties =
		(PFN_vkGetPhysicalDeviceQueueFamilyProperties)g.pfnGipa( VK_NULL_HANDLE, "vkGetPhysicalDeviceQueueFamilyProperties" );
	PFN_vkCreateDevice pfnCreateDevice = (PFN_vkCreateDevice)g.pfnGipa( VK_NULL_HANDLE, "vkCreateDevice" );
	PFN_vkGetDeviceProcAddr pfnGetDeviceProcAddr = (PFN_vkGetDeviceProcAddr)g.pfnGipa( VK_NULL_HANDLE, "vkGetDeviceProcAddr" );

	if ( !pfnCreateInstance || !g.vkDestroyInstance || !pfnEnumeratePhysicalDevices ||
		 !pfnGetPhysicalDeviceQueueFamilyProperties || !pfnCreateDevice || !pfnGetDeviceProcAddr ) {
		common->Printf( "IceBridge Vulkan: missing core loader exports.\n" );
		vk_shutdown_all();
		QIceBridge_SetActiveRHI( QICEBRIDGE_RHI_D3D12 );
		return false;
	}

	const char *instExts[] = {
		VK_KHR_SURFACE_EXTENSION_NAME,
		VK_KHR_XLIB_SURFACE_EXTENSION_NAME
	};
	VkApplicationInfo app{};
	app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app.pApplicationName = "Doom 3";
	app.applicationVersion = VK_MAKE_VERSION( 1, 0, 0 );
	app.pEngineName = "idTech4";
	app.engineVersion = VK_MAKE_VERSION( 1, 0, 0 );
	app.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo ici{};
	ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	ici.pApplicationInfo = &app;
	ici.enabledExtensionCount = 2;
	ici.ppEnabledExtensionNames = instExts;

	if ( pfnCreateInstance( &ici, nullptr, &g.instance ) != VK_SUCCESS ) {
		common->Printf( "IceBridge Vulkan: vkCreateInstance failed.\n" );
		vk_shutdown_all();
		QIceBridge_SetActiveRHI( QICEBRIDGE_RHI_D3D12 );
		return false;
	}

	PFN_vkCreateXlibSurfaceKHR pfnCreateXlibSurfaceKHR =
		(PFN_vkCreateXlibSurfaceKHR)g.pfnGipa( g.instance, "vkCreateXlibSurfaceKHR" );
	g.vkDestroySurfaceKHR = (PFN_vkDestroySurfaceKHR)g.pfnGipa( g.instance, "vkDestroySurfaceKHR" );
	PFN_vkGetPhysicalDeviceSurfaceSupportKHR pfnGetPhysicalDeviceSurfaceSupportKHR =
		(PFN_vkGetPhysicalDeviceSurfaceSupportKHR)g.pfnGipa( g.instance, "vkGetPhysicalDeviceSurfaceSupportKHR" );

	if ( !pfnCreateXlibSurfaceKHR || !g.vkDestroySurfaceKHR || !pfnGetPhysicalDeviceSurfaceSupportKHR ) {
		common->Printf( "IceBridge Vulkan: KHR_surface / xlib_surface not available.\n" );
		vk_shutdown_all();
		QIceBridge_SetActiveRHI( QICEBRIDGE_RHI_D3D12 );
		return false;
	}

	VkXlibSurfaceCreateInfoKHR xsci{};
	xsci.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
	xsci.dpy = dpy;
	xsci.window = xwin;
	if ( pfnCreateXlibSurfaceKHR( g.instance, &xsci, nullptr, &g.surface ) != VK_SUCCESS ) {
		common->Printf( "IceBridge Vulkan: vkCreateXlibSurfaceKHR failed.\n" );
		vk_shutdown_all();
		QIceBridge_SetActiveRHI( QICEBRIDGE_RHI_D3D12 );
		return false;
	}

	uint32_t physCount = 0;
	pfnEnumeratePhysicalDevices( g.instance, &physCount, nullptr );
	if ( physCount == 0 ) {
		common->Printf( "IceBridge Vulkan: no physical devices.\n" );
		vk_shutdown_all();
		QIceBridge_SetActiveRHI( QICEBRIDGE_RHI_D3D12 );
		return false;
	}
	idList<VkPhysicalDevice> physList;
	physList.SetNum( physCount );
	pfnEnumeratePhysicalDevices( g.instance, &physCount, physList.Ptr() );

	g.phys = VK_NULL_HANDLE;
	g.gfxQueueFamily = 0;
	for ( uint32_t pi = 0; pi < physCount; ++pi ) {
		VkPhysicalDevice p = physList[pi];
		uint32_t qCount = 0;
		pfnGetPhysicalDeviceQueueFamilyProperties( p, &qCount, nullptr );
		idList<VkQueueFamilyProperties> qprops;
		qprops.SetNum( qCount );
		pfnGetPhysicalDeviceQueueFamilyProperties( p, &qCount, qprops.Ptr() );
		for ( uint32_t qi = 0; qi < qCount; ++qi ) {
			VkBool32 sup = VK_FALSE;
			pfnGetPhysicalDeviceSurfaceSupportKHR( p, qi, g.surface, &sup );
			if ( sup && ( qprops[qi].queueFlags & VK_QUEUE_GRAPHICS_BIT ) ) {
				g.phys = p;
				g.gfxQueueFamily = qi;
				break;
			}
		}
		if ( g.phys != VK_NULL_HANDLE ) {
			break;
		}
	}
	if ( g.phys == VK_NULL_HANDLE ) {
		common->Printf( "IceBridge Vulkan: no graphics+present queue family.\n" );
		vk_shutdown_all();
		QIceBridge_SetActiveRHI( QICEBRIDGE_RHI_D3D12 );
		return false;
	}

	float prio = 1.0f;
	VkDeviceQueueCreateInfo dq{};
	dq.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	dq.queueFamilyIndex = g.gfxQueueFamily;
	dq.queueCount = 1;
	dq.pQueuePriorities = &prio;

	const char *devExts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	VkDeviceCreateInfo dci{};
	dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	dci.queueCreateInfoCount = 1;
	dci.pQueueCreateInfos = &dq;
	dci.enabledExtensionCount = 1;
	dci.ppEnabledExtensionNames = devExts;

	if ( pfnCreateDevice( g.phys, &dci, nullptr, &g.device ) != VK_SUCCESS ) {
		common->Printf( "IceBridge Vulkan: vkCreateDevice failed.\n" );
		vk_shutdown_all();
		QIceBridge_SetActiveRHI( QICEBRIDGE_RHI_D3D12 );
		return false;
	}

	g.pfnGdpa = pfnGetDeviceProcAddr;
	g.vkDestroyDevice = (PFN_vkDestroyDevice)g.pfnGdpa( g.device, "vkDestroyDevice" );
	PFN_vkGetDeviceQueue pfnGetDeviceQueue = (PFN_vkGetDeviceQueue)g.pfnGdpa( g.device, "vkGetDeviceQueue" );
	PFN_vkCreateSemaphore pfnCreateSemaphore = (PFN_vkCreateSemaphore)g.pfnGdpa( g.device, "vkCreateSemaphore" );
	PFN_vkCreateFence pfnCreateFence = (PFN_vkCreateFence)g.pfnGdpa( g.device, "vkCreateFence" );
	g.vkDestroySemaphore = (PFN_vkDestroySemaphore)g.pfnGdpa( g.device, "vkDestroySemaphore" );
	g.vkDestroyFence = (PFN_vkDestroyFence)g.pfnGdpa( g.device, "vkDestroyFence" );

	if ( !g.vkDestroyDevice || !pfnGetDeviceQueue || !pfnCreateSemaphore || !pfnCreateFence ||
		 !g.vkDestroySemaphore || !g.vkDestroyFence ) {
		vk_shutdown_all();
		QIceBridge_SetActiveRHI( QICEBRIDGE_RHI_D3D12 );
		return false;
	}

	pfnGetDeviceQueue( g.device, g.gfxQueueFamily, 0, &g.queue );

	if ( !vk_create_swapchain_and_pass() ) {
		common->Printf( "IceBridge Vulkan: swapchain / render pass setup failed.\n" );
		vk_shutdown_all();
		QIceBridge_SetActiveRHI( QICEBRIDGE_RHI_D3D12 );
		return false;
	}

	VkSemaphoreCreateInfo sci{};
	sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	if ( pfnCreateSemaphore( g.device, &sci, nullptr, &g.imageAvailable ) != VK_SUCCESS ||
		 pfnCreateSemaphore( g.device, &sci, nullptr, &g.renderFinished ) != VK_SUCCESS ) {
		vk_shutdown_all();
		QIceBridge_SetActiveRHI( QICEBRIDGE_RHI_D3D12 );
		return false;
	}

	VkFenceCreateInfo fci{};
	fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	if ( pfnCreateFence( g.device, &fci, nullptr, &g.inFlight ) != VK_SUCCESS ) {
		vk_shutdown_all();
		QIceBridge_SetActiveRHI( QICEBRIDGE_RHI_D3D12 );
		return false;
	}

	QIceBridge_SetActiveRHI( QICEBRIDGE_RHI_Vulkan );
	g.active = true;
	common->Printf( "IceBridge Vulkan: presentation active (%ux%u).\n", g.swapExtent.width, g.swapExtent.height );
	return true;
}

bool IceBridgeVulkanLinux_Resize( int width, int height ) {
	if ( !g.active ) {
		return false;
	}
	g.width = width;
	g.height = height;
	if ( g.vkDeviceWaitIdle ) {
		g.vkDeviceWaitIdle( g.device );
	}
	vk_shutdown_swapchain();
	if ( g.renderPass && g.vkDestroyRenderPass ) {
		g.vkDestroyRenderPass( g.device, g.renderPass, nullptr );
		g.renderPass = VK_NULL_HANDLE;
	}
	if ( g.cmdPool && g.vkDestroyCommandPool ) {
		g.cmdBufs.Clear();
		g.vkDestroyCommandPool( g.device, g.cmdPool, nullptr );
		g.cmdPool = VK_NULL_HANDLE;
	}
	return vk_create_swapchain_and_pass();
}

bool IceBridgeVulkanLinux_Present() {
	if ( !g.active || !g.vkWaitForFences || !g.vkResetFences || !g.vkAcquireNextImageKHR ||
		 !g.vkQueueSubmit || !g.vkQueuePresentKHR ) {
		return false;
	}

	g.vkWaitForFences( g.device, 1, &g.inFlight, VK_TRUE, UINT64_MAX );
	g.vkResetFences( g.device, 1, &g.inFlight );

	uint32_t imageIndex = 0;
	VkResult acq = g.vkAcquireNextImageKHR( g.device, g.swapchain, UINT64_MAX, g.imageAvailable, VK_NULL_HANDLE, &imageIndex );
	if ( acq != VK_SUCCESS && acq != VK_SUBOPTIMAL_KHR ) {
		return false;
	}

	VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkSubmitInfo si{};
	si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	si.waitSemaphoreCount = 1;
	si.pWaitSemaphores = &g.imageAvailable;
	si.pWaitDstStageMask = &waitStage;
	si.commandBufferCount = 1;
	si.pCommandBuffers = &g.cmdBufs[imageIndex];
	si.signalSemaphoreCount = 1;
	si.pSignalSemaphores = &g.renderFinished;

	if ( g.vkQueueSubmit( g.queue, 1, &si, g.inFlight ) != VK_SUCCESS ) {
		return false;
	}

	VkPresentInfoKHR pi{};
	pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	pi.waitSemaphoreCount = 1;
	pi.pWaitSemaphores = &g.renderFinished;
	pi.swapchainCount = 1;
	pi.pSwapchains = &g.swapchain;
	pi.pImageIndices = &imageIndex;

	VkResult pr = g.vkQueuePresentKHR( g.queue, &pi );
	if ( pr != VK_SUCCESS && pr != VK_SUBOPTIMAL_KHR ) {
		return false;
	}
	return true;
}

void IceBridgeVulkanLinux_Shutdown() {
	vk_shutdown_all();
	QIceBridge_SetActiveRHI( QICEBRIDGE_RHI_D3D12 );
}

bool IceBridgeVulkanLinux_IsActive() {
	return g.active;
}

#endif // __linux__
