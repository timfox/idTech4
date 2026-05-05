/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company.

===========================================================================
*/

#include "precompiled.h"
#pragma hdrstop

#include "tr_local.h"
#include "../../opengl/icebridge_rhi.h"

static const char *r_icebridgeRHIArgs[] = { "d3d12", "vulkan", NULL };
static const char *r_icebridgeD3D12RasterArgs[] = { "vertex", "mesh", NULL };

idCVar r_icebridgeRHI( "r_icebridgeRHI", "d3d12", CVAR_RENDERER | CVAR_ARCHIVE,
	"Low-level API for the IceBridge GL compatibility layer: d3d12 (default, DXR path) or vulkan (experimental; falls back to d3d12 until implemented)",
	r_icebridgeRHIArgs, idCmdSystem::ArgCompletion_String<r_icebridgeRHIArgs> );

idCVar r_icebridgeD3D12Raster( "r_icebridgeD3D12Raster", "vertex", CVAR_RENDERER | CVAR_ARCHIVE,
	"D3D12 rasterization path for the GL shim: vertex (default) or mesh (DispatchMesh + mesh shaders when hardware supports SM6.6 mesh tier)",
	r_icebridgeD3D12RasterArgs, idCmdSystem::ArgCompletion_String<r_icebridgeD3D12RasterArgs> );

namespace {
QIceBridgeRHIKind s_requestedRHI = QICEBRIDGE_RHI_D3D12;
QIceBridgeRHIKind s_activeRHI = QICEBRIDGE_RHI_D3D12;
idStr s_lastInvalidIcebridgeRHI;
QIceBridgeD3D12RasterKind s_d3d12Raster = QICEBRIDGE_D3D12_RASTER_VERTEX;
idStr s_lastInvalidD3D12Raster;
}

extern "C" void IceBridge_RefreshRHIFromCvar( void ) {
	const char *v = r_icebridgeRHI.GetString();
	if ( !idStr::Icmp( v, "vulkan" ) ) {
		s_requestedRHI = QICEBRIDGE_RHI_Vulkan;
		s_lastInvalidIcebridgeRHI.Clear();
	} else if ( !idStr::Icmp( v, "d3d12" ) ) {
		s_requestedRHI = QICEBRIDGE_RHI_D3D12;
		s_lastInvalidIcebridgeRHI.Clear();
	} else {
		s_requestedRHI = QICEBRIDGE_RHI_D3D12;
		if ( common && v && v[0] && s_lastInvalidIcebridgeRHI.Icmp( v ) != 0 ) {
			common->Warning( "r_icebridgeRHI: unknown value '%s'; using d3d12. Valid: d3d12, vulkan.", v );
			s_lastInvalidIcebridgeRHI = v;
		}
	}
}

extern "C" void IceBridge_RefreshD3D12RasterFromCvar( void ) {
	const char *v = r_icebridgeD3D12Raster.GetString();
	if ( !idStr::Icmp( v, "mesh" ) ) {
		s_d3d12Raster = QICEBRIDGE_D3D12_RASTER_MESH;
		s_lastInvalidD3D12Raster.Clear();
	} else if ( !idStr::Icmp( v, "vertex" ) ) {
		s_d3d12Raster = QICEBRIDGE_D3D12_RASTER_VERTEX;
		s_lastInvalidD3D12Raster.Clear();
	} else {
		s_d3d12Raster = QICEBRIDGE_D3D12_RASTER_VERTEX;
		if ( common && v && v[0] && s_lastInvalidD3D12Raster.Icmp( v ) != 0 ) {
			common->Warning( "r_icebridgeD3D12Raster: unknown value '%s'; using vertex. Valid: vertex, mesh.", v );
			s_lastInvalidD3D12Raster = v;
		}
	}
}

extern "C" QIceBridgeRHIKind QIceBridge_GetRequestedRHI( void ) {
	return s_requestedRHI;
}

extern "C" QIceBridgeRHIKind QIceBridge_GetActiveRHI( void ) {
	return s_activeRHI;
}

extern "C" void QIceBridge_SetActiveRHI( QIceBridgeRHIKind k ) {
	s_activeRHI = k;
}

extern "C" QIceBridgeD3D12RasterKind IceBridge_GetD3D12RasterKind( void ) {
	return s_d3d12Raster;
}

extern "C" void IceBridge_SetD3D12RasterKind( QIceBridgeD3D12RasterKind k ) {
	s_d3d12Raster = k;
}

extern "C" void IceBridge_Log( const char *msg ) {
	if ( common && msg ) {
		common->Printf( "%s", msg );
	} else {
		OutputDebugStringA( msg );
		OutputDebugStringA( "\n" );
	}
}
