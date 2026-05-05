/*
===========================================================================

SVG → RGBA8 via NanoSVG (rasterized to a texture-friendly buffer).

===========================================================================
*/

#include "precompiled.h"
#pragma hdrstop

#include "tr_local.h"
#include "R_SvgLoad.h"

idCVar r_svgScale( "r_svgScale", "1", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_FLOAT,
	"Scale factor applied to SVG document pixel size before rasterization (icons)", 0.01f, 64.0f );
idCVar r_svgMaxDimension( "r_svgMaxDimension", "4096", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_INTEGER,
	"Clamp raster width/height so huge SVGs cannot exhaust GPU memory", 64, 8192 );

void R_LoadSVG( const char *name, byte **pic, int *width, int *height, ID_TIME_T *timestamp ) {
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

	byte *buffer = nullptr;
	const int fileSize = fileSystem->ReadFile( name, (void **)&buffer, timestamp );
	if ( fileSize <= 0 || !buffer ) {
		return;
	}

	// nsvgParse mutates the buffer; ReadFile guarantees a trailing NUL at fileSize.
	char *svgText = reinterpret_cast<char *>( buffer );
	NSVGimage *image = nsvgParse( svgText, "px", 96.0f );
	fileSystem->FreeFile( buffer );
	buffer = nullptr;

	if ( !image || image->width <= 0.0f || image->height <= 0.0f ) {
		if ( image ) {
			nsvgDelete( image );
		}
		common->Warning( "R_LoadSVG: failed to parse %s\n", name );
		return;
	}

	const float scale = r_svgScale.GetFloat();
	const int maxDim = r_svgMaxDimension.GetInteger();
	int rw = ( int )idMath::Ceil( image->width * scale );
	int rh = ( int )idMath::Ceil( image->height * scale );
	if ( rw < 1 ) {
		rw = 1;
	}
	if ( rh < 1 ) {
		rh = 1;
	}
	if ( rw > maxDim || rh > maxDim ) {
		const float sx = ( float )maxDim / ( float )rw;
		const float sy = ( float )maxDim / ( float )rh;
		const float s = idMath::Min( sx, sy );
		rw = idMath::Max( 1, ( int )idMath::Floor( rw * s ) );
		rh = idMath::Max( 1, ( int )idMath::Floor( rh * s ) );
	}

	const int stride = rw * 4;
	byte *rgba = (byte *)R_StaticAlloc( stride * rh );
	memset( rgba, 0, stride * rh );

	NSVGrasterizer *rast = nsvgCreateRasterizer();
	if ( !rast ) {
		nsvgDelete( image );
		common->Warning( "R_LoadSVG: nsvgCreateRasterizer failed for %s\n", name );
		return;
	}

	const float rasterScale = ( float )rw / image->width;
	nsvgRasterize( rast, image, 0.0f, 0.0f, rasterScale, rgba, rw, rh, stride );
	nsvgDeleteRasterizer( rast );
	nsvgDelete( image );

	if ( pic ) {
		*pic = rgba;
	} else {
		R_StaticFree( rgba );
	}
	if ( width ) {
		*width = rw;
	}
	if ( height ) {
		*height = rh;
	}
}
