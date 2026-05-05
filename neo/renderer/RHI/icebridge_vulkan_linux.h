/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company.

Linux: optional Vulkan swapchain presentation when r_icebridgeRHI=vulkan.
GLX remains the OpenGL context; each frame we also acquire/present a Vulkan
swapchain image (cleared to a solid color) so the window shows Vulkan output.

===========================================================================
*/

#ifndef ICEBRIDGE_VULKAN_LINUX_H
#define ICEBRIDGE_VULKAN_LINUX_H

#ifdef __linux__

struct Display;
typedef unsigned long Window;

bool IceBridgeVulkanLinux_Init( Display *dpy, Window xwin, int width, int height );
bool IceBridgeVulkanLinux_Resize( int width, int height );
bool IceBridgeVulkanLinux_Present();
void IceBridgeVulkanLinux_Shutdown();
bool IceBridgeVulkanLinux_IsActive();

#endif

#endif
