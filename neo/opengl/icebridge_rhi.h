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

/* D3D12-only: vertex shader pipeline vs mesh shader DispatchMesh path (requires SM6.6 mesh support). */
typedef enum QIceBridgeD3D12RasterKind {
	QICEBRIDGE_D3D12_RASTER_VERTEX = 0,
	QICEBRIDGE_D3D12_RASTER_MESH = 1
} QIceBridgeD3D12RasterKind;

/* Called from the renderer after CVars are registered / before GL context init. */
void IceBridge_RefreshRHIFromCvar(void);
void IceBridge_RefreshD3D12RasterFromCvar(void);

QIceBridgeRHIKind QIceBridge_GetRequestedRHI(void);
QIceBridgeRHIKind QIceBridge_GetActiveRHI(void);
void QIceBridge_SetActiveRHI(QIceBridgeRHIKind k);

QIceBridgeD3D12RasterKind IceBridge_GetD3D12RasterKind(void);
void IceBridge_SetD3D12RasterKind(QIceBridgeD3D12RasterKind k);

/*
 * Optional log hook implemented in the game DLL (uses idCommon).
 * Falls back to OutputDebugStringA only if not linked.
 */
void IceBridge_Log(const char *msg);

#ifdef __cplusplus
}
#endif
