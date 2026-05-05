/*
===========================================================================
OpenEXR load path (TinyEXR + miniz). Produces RGBA float (16 bytes/texel).
===========================================================================
*/

#include "precompiled.h"
#pragma hdrstop

#include "tr_local.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#define TINYEXR_IMPLEMENTATION
#include "third_party/tinyexr/tinyexr.h"

void R_LoadEXR( const char *name, byte **pic, int *width, int *height, ID_TIME_T *timestamp ) {
	if ( pic ) {
		*pic = NULL;
	}
	if ( width ) {
		*width = 0;
	}
	if ( height ) {
		*height = 0;
	}
	if ( timestamp ) {
		*timestamp = 0xFFFFFFFF;
	}

	byte *buf = nullptr;
	const int len = fileSystem->ReadFile( name, (void **)&buf, timestamp );
	if ( len <= 0 || !buf ) {
		return;
	}

	if ( !pic ) {
		fileSystem->FreeFile( buf );
		return;
	}

	float *rgba = nullptr;
	int w = 0, h = 0;
	const char *err = nullptr;
	const int ret = LoadEXRFromMemory( &rgba, &w, &h, buf, (size_t)len, &err );
	fileSystem->FreeFile( buf );
	buf = nullptr;

	if ( ret != TINYEXR_SUCCESS || !rgba || w < 1 || h < 1 ) {
		if ( err ) {
			common->Warning( "R_LoadEXR: %s — %s\n", name, err );
			FreeEXRErrorMessage( err );
		} else {
			common->Warning( "R_LoadEXR: failed to load %s (code %i)\n", name, ret );
		}
		if ( rgba ) {
			free( rgba );
		}
		return;
	}

	const size_t bytes = (size_t)w * (size_t)h * 4u * sizeof( float );
	byte *out = (byte *)R_StaticAlloc( (int)bytes );
	memcpy( out, rgba, bytes );
	free( rgba );

	*pic = out;
	if ( width ) {
		*width = w;
	}
	if ( height ) {
		*height = h;
	}
}
