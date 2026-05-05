/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company.

Small parallel-for helper (std::thread) for embarrassingly parallel row work.

===========================================================================
*/

#ifndef __PARALLELJOB_H__
#define __PARALLELJOB_H__

struct idParallelDispatchParms {
	bool	enabled;			// if false, always run serially
	int		minRowsForParallel;	// minimum row count before using threads
	int		maxThreads;			// 0 = choose a small default from hardware_concurrency()
};

/*
================
idParallelForRows

Invokes rowFn( context, rowIndex ) for each rowIndex in
[rowBegin, rowEndExclusive). When enabled and the span is large enough,
splits rows across worker threads; otherwise runs serially on the caller.

The row function must not touch idLib heap allocators or other shared
mutable engine state unless externally synchronized.
================
*/
void idParallelForRows( int rowBegin, int rowEndExclusive, void (*rowFn)( void *context, int rowIndex ), void *context, const idParallelDispatchParms *parms );

#endif /* !__PARALLELJOB_H__ */
