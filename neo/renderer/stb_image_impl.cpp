/*
===========================================================================
stb_image — single translation unit (Sean Barrett / public domain stb).
https://github.com/nothings/stb — see third_party/stb_image.h and STB_IMAGE_LICENSE.txt.
Allocators route through idTech 4 heap (Mem_Alloc / Mem_Free).
===========================================================================
*/

#include "precompiled.h"
#pragma hdrstop

#include <cstring>

static void *IdStbiReallocSized( void *p, size_t oldSize, size_t newSize ) {
	if ( newSize == 0 ) {
		Mem_Free( p );
		return NULL;
	}
	void *n = Mem_Alloc( (int)newSize );
	if ( !n ) {
		Mem_Free( p );
		return NULL;
	}
	if ( p ) {
		const size_t copyLen = oldSize < newSize ? oldSize : newSize;
		if ( copyLen ) {
			memcpy( n, p, copyLen );
		}
		Mem_Free( p );
	}
	return n;
}

#define STBI_MALLOC( sz )				Mem_Alloc( (int)( sz ) )
#define STBI_FREE( p )					Mem_Free( p )
#define STBI_REALLOC_SIZED( p, oldsz, newsz )	IdStbiReallocSized( (p), (oldsz), (newsz) )
#define STBI_NO_STDIO
#define STBI_ASSERT( x )				assert( (x) )

#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb_image.h"
