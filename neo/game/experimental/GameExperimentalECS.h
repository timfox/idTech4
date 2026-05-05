/*
===============================================================================
Experimental ECS / EDA layer (EnTT). Does NOT replace idEntity / idEvent.

This is an optional side registry for new subsystems (AI, tools, AIML
bridges, etc.). The stock game loop, networking, and savegames are unchanged.
===============================================================================
*/

#ifndef __GAME_EXPERIMENTAL_ECS_H__
#define __GAME_EXPERIMENTAL_ECS_H__

class idCmdArgs;

class idGameExperimentalECS {
public:
	virtual					~idGameExperimentalECS() {}

	virtual void			InitForMap( void ) = 0;
	virtual void			ShutdownForMap( void ) = 0;
	virtual void			Frame( int framenum, int timeMs ) = 0;

	virtual void			EnqueueTestEvent( const char *message ) = 0;
	virtual int				LiveEntityCount( void ) const = 0;
};

// Owned by idGameLocal; null when subsystem disabled at compile time.
extern idGameExperimentalECS *	gameExperimentalECS;

void								GameExperimentalECS_Alloc( void );
void								GameExperimentalECS_Free( void );

void								Cmd_ECSExperimentalProbe_f( const idCmdArgs &args );

#endif
