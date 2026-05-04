#pragma once

/*
 * Minimal C ABI between the static opengl/IceBridge layer (neo/opengl) and the
 * game executable (DoomDLL). Used to select the low-level RHI while keeping the
 * existing D3D12 + DXR path intact until Vulkan implements the full GL shim.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef enum QIceBridgeRHIKind {
	QICEBRIDGE_RHI_D3D12 = 0,
	QICEBRIDGE_RHI_Vulkan = 1
} QIceBridgeRHIKind;

/* Called from the renderer after CVars are registered / before GL context init. */
void IceBridge_RefreshRHIFromCvar(void);

QIceBridgeRHIKind QIceBridge_GetRequestedRHI(void);
QIceBridgeRHIKind QIceBridge_GetActiveRHI(void);
void QIceBridge_SetActiveRHI(QIceBridgeRHIKind k);

/*
 * Optional log hook implemented in the game DLL (uses idCommon).
 * Falls back to OutputDebugStringA only if not linked.
 */
void IceBridge_Log(const char *msg);

#ifdef __cplusplus
}
#endif
