/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company.

===========================================================================
*/

#include "precompiled.h"
#pragma hdrstop

#include "ParallelJob.h"

#include <algorithm>
#include <thread>
#include <vector>

namespace {

struct parallelChunk_t {
	void ( *fn )( void *, int );
	void *	ctx;
	int		y0;
	int		y1;
};

static void RunChunk( parallelChunk_t chunk ) {
	for ( int y = chunk.y0; y < chunk.y1; y++ ) {
		chunk.fn( chunk.ctx, y );
	}
}

} // namespace

/*
================
idParallelForRows
================
*/
void idParallelForRows( int rowBegin, int rowEndExclusive, void ( *rowFn )( void *context, int rowIndex ), void *context, const idParallelDispatchParms *parms ) {
	if ( !rowFn || rowEndExclusive <= rowBegin ) {
		return;
	}

	idParallelDispatchParms defaults;
	defaults.enabled = true;
	defaults.minRowsForParallel = 64;
	defaults.maxThreads = 0;
	const idParallelDispatchParms &p = parms ? *parms : defaults;

	const int span = rowEndExclusive - rowBegin;
	if ( !p.enabled || span < idMath::Max( 8, p.minRowsForParallel ) ) {
		for ( int y = rowBegin; y < rowEndExclusive; y++ ) {
			rowFn( context, y );
		}
		return;
	}

	int hc = (int)std::thread::hardware_concurrency();
	if ( hc < 1 ) {
		hc = 1;
	}
	int maxT = p.maxThreads > 0 ? p.maxThreads : std::min( 8, hc );
	maxT = idMath::ClampInt( 1, 32, maxT );
	if ( maxT <= 1 ) {
		for ( int y = rowBegin; y < rowEndExclusive; y++ ) {
			rowFn( context, y );
		}
		return;
	}

	const int chunkRows = idMath::Max( 1, ( span + maxT - 1 ) / maxT );
	std::vector<parallelChunk_t> chunks;
	chunks.reserve( maxT );
	for ( int y = rowBegin; y < rowEndExclusive; y += chunkRows ) {
		parallelChunk_t c;
		c.fn = rowFn;
		c.ctx = context;
		c.y0 = y;
		c.y1 = idMath::Min( rowEndExclusive, y + chunkRows );
		chunks.push_back( c );
	}

	if ( chunks.size() <= 1 ) {
		RunChunk( chunks[0] );
		return;
	}

	std::vector<std::thread> workers;
	workers.reserve( chunks.size() - 1 );
	for ( size_t i = 1; i < chunks.size(); i++ ) {
		workers.emplace_back( RunChunk, chunks[i] );
	}
	RunChunk( chunks[0] );
	for ( size_t i = 0; i < workers.size(); i++ ) {
		workers[i].join();
	}
}
