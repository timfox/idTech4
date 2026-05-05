/*
 * Vulkan capability probe — raster / presentation focused (Steam Deck friendly).
 * No ray tracing extension checks by default.
 *
 * Linux:  make -C tools/vulkan_caps && ./tools/vulkan_caps/vulkan_caps
 */

#include <cstdint>
#include <cstdio>
#include <cstring>

#if defined( _WIN32 )
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#ifndef VK_MAKE_API_VERSION
#define VK_MAKE_API_VERSION( variant, major, minor, patch ) \
	( ( (uint32_t)(variant) << 29U ) | ( (uint32_t)(major) << 22U ) | ( (uint32_t)(minor) << 12U ) | (uint32_t)(patch) )
#endif
#ifndef VK_API_VERSION_1_0
#define VK_API_VERSION_1_0 VK_MAKE_API_VERSION( 0, 1, 0, 0 )
#endif

enum {
	VK_MAX_EXTENSION_NAME_SIZE = 256,
	VK_STRUCTURE_TYPE_APPLICATION_INFO = 0,
	VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO = 1,
	VK_SUCCESS = 0
};

typedef uint32_t VkFlags;
typedef uint32_t VkResult;
typedef void *VkInstance;
typedef void *VkPhysicalDevice;

typedef struct VkApplicationInfo {
	uint32_t sType;
	const void *pNext;
	const char *pApplicationName;
	uint32_t applicationVersion;
	const char *pEngineName;
	uint32_t engineVersion;
	uint32_t apiVersion;
} VkApplicationInfo;

typedef struct VkInstanceCreateInfo {
	uint32_t sType;
	const void *pNext;
	VkFlags flags;
	const VkApplicationInfo *pApplicationInfo;
	uint32_t enabledLayerCount;
	const char *const *ppEnabledLayerNames;
	uint32_t enabledExtensionCount;
	const char *const *ppEnabledExtensionNames;
} VkInstanceCreateInfo;

typedef struct VkExtensionProperties {
	char extensionName[VK_MAX_EXTENSION_NAME_SIZE];
	uint32_t specVersion;
} VkExtensionProperties;

typedef VkResult ( *PFN_vkCreateInstance )( const VkInstanceCreateInfo *, const void *, VkInstance * );
typedef void ( *PFN_vkDestroyInstance )( VkInstance, const void * );
typedef VkResult ( *PFN_vkEnumeratePhysicalDevices )( VkInstance, uint32_t *, VkPhysicalDevice * );
typedef VkResult ( *PFN_vkEnumerateDeviceExtensionProperties )(
	VkPhysicalDevice, const char *, uint32_t *, VkExtensionProperties * );
typedef void *( *PFN_vkGetInstanceProcAddr )( VkInstance, const char * );

static void *g_lib = nullptr;
static PFN_vkGetInstanceProcAddr g_gipa = nullptr;

#if !defined( _WIN32 )
static void *sym( const char *n ) {
	return dlsym( g_lib, n );
}
#endif

static bool load_loader() {
#if defined( _WIN32 )
	g_lib = LoadLibraryA( "vulkan-1.dll" );
	if ( !g_lib ) {
		std::fprintf( stderr, "LoadLibrary(vulkan-1.dll) failed\n" );
		return false;
	}
	g_gipa = (PFN_vkGetInstanceProcAddr)GetProcAddress( (HMODULE)g_lib, "vkGetInstanceProcAddr" );
#else
	g_lib = dlopen( "libvulkan.so.1", RTLD_NOW );
	if ( !g_lib ) {
		std::fprintf( stderr, "dlopen(libvulkan.so.1): %s\n", dlerror() );
		return false;
	}
	g_gipa = (PFN_vkGetInstanceProcAddr)dlsym( g_lib, "vkGetInstanceProcAddr" );
#endif
	return g_gipa != nullptr;
}

static void unload() {
#if defined( _WIN32 )
	if ( g_lib ) {
		FreeLibrary( (HMODULE)g_lib );
	}
#else
	if ( g_lib ) {
		dlclose( g_lib );
	}
#endif
	g_lib = nullptr;
	g_gipa = nullptr;
}

static bool has_ext( const VkExtensionProperties *p, uint32_t n, const char *name ) {
	for ( uint32_t i = 0; i < n; ++i ) {
		if ( std::strcmp( p[i].extensionName, name ) == 0 ) {
			return true;
		}
	}
	return false;
}

int main() {
	if ( !load_loader() ) {
		return 1;
	}

	PFN_vkCreateInstance pfnCreate = (PFN_vkCreateInstance)g_gipa( nullptr, "vkCreateInstance" );
	PFN_vkDestroyInstance pfnDestroy = (PFN_vkDestroyInstance)g_gipa( nullptr, "vkDestroyInstance" );
#if !defined( _WIN32 )
	if ( !pfnCreate ) {
		pfnCreate = (PFN_vkCreateInstance)sym( "vkCreateInstance" );
	}
	if ( !pfnDestroy ) {
		pfnDestroy = (PFN_vkDestroyInstance)sym( "vkDestroyInstance" );
	}
#endif
	if ( !pfnCreate || !pfnDestroy ) {
		std::fprintf( stderr, "vkCreateInstance / vkDestroyInstance missing\n" );
		unload();
		return 1;
	}

	VkApplicationInfo app = {};
	app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app.pApplicationName = "vulkan_caps";
	app.applicationVersion = 1;
	app.pEngineName = "idTech4";
	app.engineVersion = 1;
	app.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo ci = {};
	ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	ci.pApplicationInfo = &app;

	VkInstance inst = nullptr;
	VkResult r = pfnCreate( &ci, nullptr, &inst );
	if ( r != VK_SUCCESS || !inst ) {
		std::fprintf( stderr, "vkCreateInstance failed (%d) — no ICD/driver?\n", (int)r );
		unload();
		return 1;
	}

	PFN_vkEnumeratePhysicalDevices pfnPhys =
		(PFN_vkEnumeratePhysicalDevices)g_gipa( inst, "vkEnumeratePhysicalDevices" );
	PFN_vkEnumerateDeviceExtensionProperties pfnExt =
		(PFN_vkEnumerateDeviceExtensionProperties)g_gipa( inst, "vkEnumerateDeviceExtensionProperties" );
	if ( !pfnPhys || !pfnExt ) {
		std::fprintf( stderr, "Missing enumerate entry points\n" );
		pfnDestroy( inst, nullptr );
		unload();
		return 1;
	}

	static const char *kRaster[] = {
		"VK_KHR_swapchain",
		"VK_KHR_surface",
		"VK_KHR_xcb_surface",
		"VK_KHR_xlib_surface",
		"VK_KHR_wayland_surface",
		"VK_EXT_swapchain_colorspace",
		"VK_KHR_get_surface_capabilities2",
		"VK_KHR_synchronization2",
		"VK_KHR_dynamic_rendering",
		"VK_KHR_bind_memory2",
		"VK_KHR_buffer_device_address",
		"VK_EXT_descriptor_indexing",
		"VK_KHR_maintenance3",
		"VK_KHR_timeline_semaphore",
	};

	uint32_t nPhys = 0;
	pfnPhys( inst, &nPhys, nullptr );
	std::printf( "Physical devices: %u\n", nPhys );
	if ( nPhys == 0 ) {
		pfnDestroy( inst, nullptr );
		unload();
		return 0;
	}

	auto *phys = new VkPhysicalDevice[nPhys];
	pfnPhys( inst, &nPhys, phys );

	for ( uint32_t i = 0; i < nPhys; ++i ) {
		uint32_t ne = 0;
		pfnExt( phys[i], nullptr, &ne, nullptr );
		auto *e = new VkExtensionProperties[ne];
		pfnExt( phys[i], nullptr, &ne, e );
		std::printf( "\n--- GPU %u : %u device extensions ---\n", i, ne );
		for ( const char *name : kRaster ) {
			std::printf( "  %-40s %s\n", name, has_ext( e, ne, name ) ? "yes" : "no" );
		}
		delete[] e;
	}

	delete[] phys;
	pfnDestroy( inst, nullptr );
	unload();
	std::printf( "\nUse this to plan WSI (Wayland/X11) and modern-vulkan features on Deck/desktop.\n" );
	return 0;
}
