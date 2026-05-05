// Minimal Vulkan bootstrap for IceBridge (dynamic-loaded; no headers required)
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Initialize a Vulkan instance and logical device for diagnostics and future use.
// Returns true on success. Safe to call multiple times; subsequent calls are no-ops.
bool IceBridgeVulkan_Init(void);

// Print a concise capabilities summary to the engine log.
void IceBridgeVulkan_LogCaps(void);

// Destroy Vulkan device/instance if initialized.
void IceBridgeVulkan_Shutdown(void);

#ifdef __cplusplus
}
#endif

