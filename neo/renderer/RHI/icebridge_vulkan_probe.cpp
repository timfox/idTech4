/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company.

Lightweight Vulkan loader probe (instance creation only). Independent of the
GL/D3D12 shim so it can be used to verify driver support on Windows builds.

===========================================================================
*/

#include "precompiled.h"
#pragma hdrstop

#include "tr_local.h"

#include <windows.h>

#ifndef VK_MAKE_API_VERSION
#define VK_MAKE_API_VERSION( variant, major, minor, patch ) \
	( ( (uint32_t)(variant) << 29U ) | ( (uint32_t)(major) << 22U ) | ( (uint32_t)(minor) << 12U ) | (uint32_t)(patch) )
#endif
#ifndef VK_API_VERSION_1_0
#define VK_API_VERSION_1_0 VK_MAKE_API_VERSION( 0, 1, 0, 0 )
#endif
#ifndef VK_VERSION_MAJOR
#define VK_VERSION_MAJOR( version ) ( (uint32_t)(version) >> 22U )
#endif
#ifndef VK_VERSION_MINOR
#define VK_VERSION_MINOR( version ) ( ( (uint32_t)(version) >> 12U ) & 0x3FFU )
#endif
#ifndef VK_VERSION_PATCH
#define VK_VERSION_PATCH( version ) ( (uint32_t)(version) & 0xFFFU )
#endif

enum VkStructureTypeProbe {
	VK_STRUCTURE_TYPE_APPLICATION_INFO_PROBE = 0,
	VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO_PROBE = 1
};

typedef uint32_t VkFlags;
typedef uint32_t VkResult;

#define VK_SUCCESS 0

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

typedef VkResult ( APIENTRY *PFN_vkCreateInstance )( const VkInstanceCreateInfo *pCreateInfo, void *pAllocator, void **pInstance );
typedef void ( APIENTRY *PFN_vkDestroyInstance )( void *instance, void *pAllocator );
typedef VkResult ( APIENTRY *PFN_vkEnumerateInstanceVersion )( uint32_t *pApiVersion );

static void *IceBridgeVulkanLoad( const char *name ) {
	static HMODULE mod = LoadLibraryA( "vulkan-1.dll" );
	if ( !mod ) {
		return nullptr;
	}
	return (void *)GetProcAddress( mod, name );
}

/*
================
R_VkInfo_f
================
*/
static void R_VkInfo_f( const idCmdArgs &args ) {
	(void)args;

	auto pfnEnumerateInstanceVersion = (PFN_vkEnumerateInstanceVersion)IceBridgeVulkanLoad( "vkEnumerateInstanceVersion" );
	auto pfnCreateInstance = (PFN_vkCreateInstance)IceBridgeVulkanLoad( "vkCreateInstance" );
	auto pfnDestroyInstance = (PFN_vkDestroyInstance)IceBridgeVulkanLoad( "vkDestroyInstance" );

	if ( !pfnCreateInstance || !pfnDestroyInstance ) {
		common->Printf( "Vulkan: vulkan-1.dll not found or exports missing.\n" );
		return;
	}

	uint32_t apiVersion = VK_API_VERSION_1_0;
	if ( pfnEnumerateInstanceVersion ) {
		if ( pfnEnumerateInstanceVersion( &apiVersion ) == VK_SUCCESS ) {
			common->Printf( "Vulkan: vkEnumerateInstanceVersion reports API version %u.%u.%u\n",
				VK_VERSION_MAJOR( apiVersion ), VK_VERSION_MINOR( apiVersion ), VK_VERSION_PATCH( apiVersion ) );
		}
	} else {
		apiVersion = VK_API_VERSION_1_0;
	}

	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO_PROBE;
	appInfo.pApplicationName = "idTech4 IceBridge probe";
	appInfo.applicationVersion = 1;
	appInfo.pEngineName = "IceBridge";
	appInfo.engineVersion = 1;
	appInfo.apiVersion = apiVersion;

	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO_PROBE;
	createInfo.pApplicationInfo = &appInfo;

	void *instance = nullptr;
	VkResult r = pfnCreateInstance( &createInfo, nullptr, &instance );
	if ( r != VK_SUCCESS || !instance ) {
		common->Printf( "Vulkan: vkCreateInstance failed (result %u).\n", (unsigned)r );
		return;
	}

	common->Printf( "Vulkan: vkCreateInstance succeeded (minimal probe).\n" );
	pfnDestroyInstance( instance, nullptr );
}

void R_InitVulkanProbeCommand( void ) {
	cmdSystem->AddCommand( "vkInfo", R_VkInfo_f, CMD_FL_RENDERER, "loads Vulkan-1.dll, prints instance API version, creates and destroys a VkInstance (diagnostic)" );
}
