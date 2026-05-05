/*
===============================================================================
Optional EnTT registry + simple event queue (EDA-style). Parallel to idEntity.
===============================================================================
*/

#include "precompiled.h"
#pragma hdrstop

#include "../Game_local.h"
#include "GameExperimentalECS.h"

#include <entt/entt.hpp>

idGameExperimentalECS *gameExperimentalECS = nullptr;

namespace {

struct ExpPosition {
	idVec3	origin;
};

struct ExpTag {
	idStr	name;
};

class idGameExperimentalECSLocal final : public idGameExperimentalECS {
public:
	idGameExperimentalECSLocal() : mapActive( false ) {}

	void InitForMap( void ) override {
		registry.clear();
		pendingEvents.Clear();
		mapActive = true;

		for ( int i = 0; i < 3; ++i ) {
			const auto e = registry.create();
			registry.emplace<ExpPosition>( e, idVec3( ( float )i, 0.0f, 0.0f ) );
			registry.emplace<ExpTag>( e, idStr::Format( "exp_ecs_seed_%i", i ) );
		}
		gameLocal.DPrintf( "experimental ECS: map init, seeded %i entities\n", LiveEntityCount() );
	}

	void ShutdownForMap( void ) override {
		registry.clear();
		pendingEvents.Clear();
		mapActive = false;
	}

	void Frame( int framenum, int timeMs ) override {
		if ( !mapActive || !g_ecsExperimental.GetBool() ) {
			return;
		}

		for ( int i = 0; i < pendingEvents.Num(); ++i ) {
			gameLocal.DPrintf( "experimental ECS event: %s\n", pendingEvents[i].c_str() );
		}
		pendingEvents.Clear();

		const float t = ( float )timeMs * 0.001f;
		auto view = registry.view<ExpPosition, ExpTag>();
		view.each( [&]( entt::entity, ExpPosition &p, ExpTag &tag ) {
			p.origin.z = idMath::Sin( t + ( float )tag.name.Length() ) * 0.01f;
		} );
		(void)framenum;
	}

	void EnqueueTestEvent( const char *message ) override {
		if ( message && message[0] ) {
			pendingEvents.Append( idStr( message ) );
		}
	}

	int LiveEntityCount( void ) const override {
		int n = 0;
		for ( auto e : registry.view<ExpPosition>() ) {
			(void)e;
			++n;
		}
		return n;
	}

private:
	entt::registry		registry;
	idStrList			pendingEvents;
	bool				mapActive;
};

} // namespace

void GameExperimentalECS_Alloc( void ) {
	if ( gameExperimentalECS ) {
		return;
	}
	gameExperimentalECS = new idGameExperimentalECSLocal();
}

void GameExperimentalECS_Free( void ) {
	if ( gameExperimentalECS ) {
		gameExperimentalECS->ShutdownForMap();
		delete gameExperimentalECS;
		gameExperimentalECS = nullptr;
	}
}

void Cmd_ECSExperimentalProbe_f( const idCmdArgs &args ) {
	if ( !gameExperimentalECS ) {
		common->Printf( "experimental ECS: not initialized\n" );
		return;
	}
	if ( args.Argc() > 1 && !idStr::Icmp( args.Argv( 1 ), "event" ) && args.Argc() > 2 ) {
		idStr msg;
		for ( int i = 2; i < args.Argc(); ++i ) {
			if ( i > 2 ) {
				msg += " ";
			}
			msg += args.Argv( i );
		}
		gameExperimentalECS->EnqueueTestEvent( msg.c_str() );
		common->Printf( "experimental ECS: queued event\n" );
		return;
	}
	common->Printf( "experimental ECS: g_ecsExperimental=%i live_entities=%i\n",
		g_ecsExperimental.GetBool(), gameExperimentalECS->LiveEntityCount() );
	common->Printf( "usage: ecsExperimentalProbe event <message>\n" );
}
