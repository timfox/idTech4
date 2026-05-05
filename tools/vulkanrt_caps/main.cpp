/*
 * Minimal Vulkan ray-tracing capability probe (no SDK required at compile time).
 * Linux: links with -ldl, loads libvulkan.so.1 at runtime.
 * Windows: loads vulkan-1.dll at runtime.
 *
 * Build (Linux): make -C tools/vulkanrt_caps
 * Run: ./tools/vulkanrt_caps/vulkanrt_caps
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
	VK_NULL_HANDLE_VALUE = 0,
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

static void *g_vulkanLib = nullptr;
static PFN_vkGetInstanceProcAddr g_vkGetInstanceProcAddr = nullptr;

#if !defined( _WIN32 )
static void *sym( const char *name ) {
	void *p = dlsym( g_vulkanLib, name );
	if ( !p ) {
		std::fprintf( stderr, "dlsym(%s): %s\n", name, dlerror() );
	}
	return p;
}
#endif

static bool load_vulkan_library() {
#if defined( _WIN32 )
	g_vulkanLib = LoadLibraryA( "vulkan-1.dll" );
	if ( !g_vulkanLib ) {
		std::fprintf( stderr, "Failed to load vulkan-1.dll\n" );
		return false;
	}
	g_vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)GetProcAddress( (HMODULE)g_vulkanLib, "vkGetInstanceProcAddr" );
#else
	g_vulkanLib = dlopen( "libvulkan.so.1", RTLD_NOW );
	if ( !g_vulkanLib ) {
		std::fprintf( stderr, "Failed to load libvulkan.so.1 (%s)\n", dlerror() );
		return false;
	}
	g_vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)dlsym( g_vulkanLib, "vkGetInstanceProcAddr" );
#endif
	if ( !g_vkGetInstanceProcAddr ) {
		std::fprintf( stderr, "vkGetInstanceProcAddr not found\n" );
		return false;
	}
	return true;
}

static void unload_vulkan_library() {
#if defined( _WIN32 )
	if ( g_vulkanLib ) {
		FreeLibrary( (HMODULE)g_vulkanLib );
	}
#else
	if ( g_vulkanLib ) {
		dlclose( g_vulkanLib );
	}
#endif
	g_vulkanLib = nullptr;
}

static bool ext_in_list( const VkExtensionProperties *props, uint32_t count, const char *name ) {
	for ( uint32_t i = 0; i < count; ++i ) {
		if ( std::strcmp( props[i].extensionName, name ) == 0 ) {
			return true;
		}
	}
	return false;
}

int main() {
	if ( !load_vulkan_library() ) {
		return 1;
	}

	PFN_vkCreateInstance pfnCreateInstance =
		(PFN_vkCreateInstance)g_vkGetInstanceProcAddr( nullptr, "vkCreateInstance" );
	PFN_vkDestroyInstance pfnDestroyInstance =
		(PFN_vkDestroyInstance)g_vkGetInstanceProcAddr( nullptr, "vkDestroyInstance" );
#if !defined( _WIN32 )
	if ( !pfnCreateInstance ) {
		pfnCreateInstance = (PFN_vkCreateInstance)sym( "vkCreateInstance" );
	}
	if ( !pfnDestroyInstance ) {
		pfnDestroyInstance = (PFN_vkDestroyInstance)sym( "vkDestroyInstance" );
	}
#endif

	if ( !pfnCreateInstance || !pfnDestroyInstance ) {
		std::fprintf( stderr, "Missing vkCreateInstance / vkDestroyInstance\n" );
		unload_vulkan_library();
		return 1;
	}

	VkApplicationInfo app = {};
	app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app.pApplicationName = "vulkanrt_caps";
	app.applicationVersion = 1;
	app.pEngineName = "idTech4";
	app.engineVersion = 1;
	app.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo ci = {};
	ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	ci.pApplicationInfo = &app;

	VkInstance instance = nullptr;
	VkResult r = pfnCreateInstance( &ci, nullptr, &instance );
	if ( r != VK_SUCCESS || !instance ) {
		std::fprintf( stderr, "vkCreateInstance failed VkResult=%d (0x%X). No ICD/driver (e.g. headless CI) is a common cause.\n",
			(int)r, (unsigned)r );
		unload_vulkan_library();
		return 1;
	}

	PFN_vkEnumeratePhysicalDevices pfnEnumPhys =
		(PFN_vkEnumeratePhysicalDevices)g_vkGetInstanceProcAddr( instance, "vkEnumeratePhysicalDevices" );
	PFN_vkEnumerateDeviceExtensionProperties pfnEnumDevExt =
		(PFN_vkEnumerateDeviceExtensionProperties)g_vkGetInstanceProcAddr( instance, "vkEnumerateDeviceExtensionProperties" );

	if ( !pfnEnumPhys || !pfnEnumDevExt ) {
		std::printf( "Missing vkEnumeratePhysicalDevices / vkEnumerateDeviceExtensionProperties\n" );
		pfnDestroyInstance( instance, nullptr );
		unload_vulkan_library();
		return 1;
	}

	uint32_t physCount = 0;
	pfnEnumPhys( instance, &physCount, nullptr );
	std::printf( "Vulkan: %u physical device(s)\n", physCount );
	if ( physCount == 0 ) {
		pfnDestroyInstance( instance, nullptr );
		unload_vulkan_library();
		return 0;
	}

	VkPhysicalDevice *phys = new VkPhysicalDevice[physCount];
	pfnEnumPhys( instance, &physCount, phys );

	static const char *kRtRelated[] = {
		"VK_KHR_acceleration_structure",
		"VK_KHR_ray_tracing_pipeline",
		"VK_KHR_ray_query",
		"VK_KHR_deferred_host_operations",
		"VK_KHR_buffer_device_address",
		"VK_KHR_spirv_1_4",
		"VK_KHR_pipeline_library",
		"VK_KHR_synchronization2",
	};

	for ( uint32_t pi = 0; pi < physCount; ++pi ) {
		uint32_t extCount = 0;
		pfnEnumDevExt( phys[pi], nullptr, &extCount, nullptr );
		VkExtensionProperties *props = new VkExtensionProperties[extCount];
		pfnEnumDevExt( phys[pi], nullptr, &extCount, props );

		std::printf( "\n--- GPU %u: %u device extensions ---\n", pi, extCount );
		for ( const char *name : kRtRelated ) {
			std::printf( "  %-42s %s\n", name, ext_in_list( props, extCount, name ) ? "yes" : "no" );
		}

		delete[] props;
	}

	delete[] phys;
	pfnDestroyInstance( instance, nullptr );
	unload_vulkan_library();
	std::printf( "\nNote: extension presence is necessary but not sufficient for a working RT pipeline.\n" );
	return 0;
}
