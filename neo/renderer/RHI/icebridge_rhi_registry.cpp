/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company.

===========================================================================
*/

#include "precompiled.h"
#pragma hdrstop

#include <cstdio>

#include "tr_local.h"
#include "../../opengl/icebridge_rhi.h"

static const char *r_icebridgeRHIArgs[] = { "d3d12", "vulkan", NULL };

idCVar r_icebridgeRHI( "r_icebridgeRHI", "d3d12", CVAR_RENDERER | CVAR_ARCHIVE,
	"Low-level API for the IceBridge GL compatibility layer: d3d12 (default, DXR path) or vulkan (experimental; falls back to d3d12 until implemented)",
	r_icebridgeRHIArgs, idCmdSystem::ArgCompletion_String<r_icebridgeRHIArgs> );

namespace {
QIceBridgeRHIKind s_requestedRHI = QICEBRIDGE_RHI_D3D12;
QIceBridgeRHIKind s_activeRHI = QICEBRIDGE_RHI_D3D12;
idStr s_lastInvalidIcebridgeRHI;
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
#if defined( _WIN32 )
		OutputDebugStringA( msg );
		OutputDebugStringA( "\n" );
#else
		if ( msg ) {
			fputs( msg, stderr );
			fputc( '\n', stderr );
		}
#endif
	}
}
