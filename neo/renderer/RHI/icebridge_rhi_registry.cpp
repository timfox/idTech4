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

idCVar r_icebridgeRHI( "r_icebridgeRHI", "d3d12", CVAR_RENDERER | CVAR_ARCHIVE,
	"Low-level API for the IceBridge GL compatibility layer: d3d12 (default, DXR path) or vulkan (experimental; falls back to d3d12 until implemented)" );

static QIceBridgeRHIKind s_requestedRHI = QICEBRIDGE_RHI_D3D12;
static QIceBridgeRHIKind s_activeRHI = QICEBRIDGE_RHI_D3D12;

extern "C" void IceBridge_RefreshRHIFromCvar( void ) {
	if ( !idStr::Icmp( r_icebridgeRHI.GetString(), "vulkan" ) ) {
		s_requestedRHI = QICEBRIDGE_RHI_Vulkan;
	} else {
		s_requestedRHI = QICEBRIDGE_RHI_D3D12;
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

extern "C" void IceBridge_Log( const char *msg ) {
	if ( common && msg ) {
		common->Printf( "%s", msg );
	} else {
		OutputDebugStringA( msg );
		OutputDebugStringA( "\n" );
	}
}
