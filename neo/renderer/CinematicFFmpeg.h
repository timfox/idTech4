/*
===========================================================================
FFmpeg-backed idCinematic (optional, ID_HAVE_FFMPEG).
===========================================================================
*/

#ifndef __CINEMATIC_FFMPEG_H__
#define __CINEMATIC_FFMPEG_H__

#include "Cinematic.h"

class idCinematicFFmpeg : public idCinematic {
public:
	idCinematicFFmpeg();
	virtual ~idCinematicFFmpeg();

	virtual bool		InitFromFile( const char *qpath, bool looping );
	virtual int			AnimationLength();
	virtual cinData_t	ImageForTime( int milliseconds );
	virtual void		Close();
	virtual void		ResetTime( int time );

private:
	struct Pimpl;
	Pimpl *				p;
};

#endif
