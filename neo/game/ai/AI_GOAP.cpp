/*
===========================================================================

Goal-Oriented Action Planning (GOAP) — optional layer for idAI.

Bitmask world model + A* over discrete actions. Runs after UpdateScript()
so it augments (not replaces) existing Doom 3 AI scripts.

===========================================================================
*/

#include "precompiled.h"
#pragma hdrstop

#include "../Game_local.h"

static const unsigned int GOAP_FACT_HAS_ENEMY		= BIT( 0 );
static const unsigned int GOAP_FACT_ENEMY_VISIBLE	= BIT( 1 );
static const unsigned int GOAP_FACT_ENEMY_REACHABLE	= BIT( 2 );
static const unsigned int GOAP_FACT_IN_MELEE_RANGE	= BIT( 3 );

typedef bool ( *goapCanFn )( const idAI *self );
typedef void ( *goapApplyFn )( idAI *self );

struct goapActionDef_t {
	const char *	name;
	unsigned int	preMask;
	goapCanFn		canApply;
	goapApplyFn		apply;
	bool			waitMoveDone;
	int				cost;
};

static bool GoapCan_MoveToEnemy( const idAI *self ) {
	if ( !self->enemy.GetEntity() ) {
		return false;
	}
	return !self->AI_ENEMY_REACHABLE;
}

static void GoapApply_MoveToEnemy( idAI *self ) {
	self->MoveToEnemy();
}

static bool GoapCan_FaceEnemy( const idAI *self ) {
	return self->enemy.GetEntity() != NULL && !self->AI_ENEMY_IN_FOV;
}

static void GoapApply_FaceEnemy( idAI *self ) {
	self->FaceEnemy();
}

static bool GoapCan_Melee( const idAI *self ) {
	if ( !self->enemy.GetEntity() ) {
		return false;
	}
	return self->TestMelee() && self->spawnArgs.GetString( "goap_meleeDef", NULL ) != NULL;
}

static void GoapApply_Melee( idAI *self ) {
	const char *def = self->spawnArgs.GetString( "goap_meleeDef", "" );
	if ( def && def[0] ) {
		self->AttackMelee( def );
	}
}

static unsigned int GoapSimulateMoveToEnemy( unsigned int st ) {
	st |= GOAP_FACT_ENEMY_REACHABLE;
	return st;
}

static unsigned int GoapSimulateFaceEnemy( unsigned int st ) {
	st |= GOAP_FACT_ENEMY_VISIBLE;
	return st;
}

static unsigned int GoapSimulateMelee( unsigned int st ) {
	st |= GOAP_FACT_IN_MELEE_RANGE;
	return st;
}

static const goapActionDef_t g_goapActions[] = {
	{ "goap_move_to_enemy", GOAP_FACT_HAS_ENEMY, GoapCan_MoveToEnemy, GoapApply_MoveToEnemy, true, 4 },
	{ "goap_face_enemy", GOAP_FACT_HAS_ENEMY, GoapCan_FaceEnemy, GoapApply_FaceEnemy, true, 2 },
	{ "goap_melee", GOAP_FACT_HAS_ENEMY | GOAP_FACT_ENEMY_VISIBLE | GOAP_FACT_ENEMY_REACHABLE, GoapCan_Melee, GoapApply_Melee, false, 1 },
};

static const int g_goapNumActions = sizeof( g_goapActions ) / sizeof( g_goapActions[0] );

static unsigned int GoapSimulate( int actionIndex, unsigned int st ) {
	switch ( actionIndex ) {
		case 0: return GoapSimulateMoveToEnemy( st );
		case 1: return GoapSimulateFaceEnemy( st );
		case 2: return GoapSimulateMelee( st );
		default: return st;
	}
}

static int GoapPopMin( idList<int> &openIdx, idList<float> &openF ) {
	int best = 0;
	float bestF = 1e30f;
	for ( int i = 0; i < openF.Num(); i++ ) {
		if ( openF[i] < bestF ) {
			bestF = openF[i];
			best = i;
		}
	}
	const int node = openIdx[best];
	openIdx.RemoveIndex( best );
	openF.RemoveIndex( best );
	return node;
}

static bool GoapClosedHas( const idList<unsigned int> &closed, unsigned int st ) {
	for ( int i = 0; i < closed.Num(); i++ ) {
		if ( closed[i] == st ) {
			return true;
		}
	}
	return false;
}

/*
================
idAI::GoapBuildWorldMask
================
*/
unsigned int idAI::GoapBuildWorldMask( void ) const {
	unsigned int m = 0;
	if ( enemy.GetEntity() ) {
		m |= GOAP_FACT_HAS_ENEMY;
	}
	if ( AI_ENEMY_VISIBLE ) {
		m |= GOAP_FACT_ENEMY_VISIBLE;
	}
	if ( AI_ENEMY_REACHABLE ) {
		m |= GOAP_FACT_ENEMY_REACHABLE;
	}
	if ( TestMelee() ) {
		m |= GOAP_FACT_IN_MELEE_RANGE;
	}
	return m;
}

/*
================
idAI::GoapInitFromSpawn
================
*/
void idAI::GoapInitFromSpawn( void ) {
	goapEnabled = spawnArgs.GetBool( "goap", "0" );
	goapGoalMask = (unsigned int)spawnArgs.GetInt( "goap_goal", "15" );
	goapReplanInterval = spawnArgs.GetInt( "goap_replan_ms", "500" );
	goapPlanStep = 0;
	goapPlan.Clear();
	goapLastPlanTime = -999999;
	goapWaitingMove = false;
}

/*
================
idAI::GoapPlan
================
*/
bool idAI::GoapPlan( void ) {
	const unsigned int goal = goapGoalMask;
	const unsigned int start = GoapBuildWorldMask();

	if ( ( start & goal ) == goal ) {
		goapPlan.Clear();
		goapPlanStep = 0;
		goapWaitingMove = false;
		return true;
	}

	const int maxNodes = idMath::ClampInt( 32, 4096, g_goapMaxNodes.GetInteger() );

	struct goapNode_t {
		unsigned int	state;
		float			g;
		int				parent;
		int				actionFromParent;
	};

	idList<goapNode_t> nodes;
	idList<unsigned int> closedStates;
	nodes.SetGranularity( 256 );
	closedStates.SetGranularity( 256 );

	auto h = [goal]( unsigned int st ) -> float {
		const unsigned int missing = goal & ~st;
		return (float)idMath::BitCount( (int)missing );
	};

	idList<int> open;
	idList<float> openF;

	goapNode_t root;
	root.state = start;
	root.g = 0.0f;
	root.parent = -1;
	root.actionFromParent = -1;
	nodes.Append( root );
	open.Append( 0 );
	openF.Append( root.g + h( start ) );

	int expansions = 0;

	while ( open.Num() > 0 && expansions < maxNodes ) {
		const int curIdx = GoapPopMin( open, openF );
		const goapNode_t &cur = nodes[curIdx];

		if ( ( cur.state & goal ) == goal ) {
			idList<int> rev;
			int trace = curIdx;
			while ( nodes[trace].parent >= 0 ) {
				rev.Append( nodes[trace].actionFromParent );
				trace = nodes[trace].parent;
			}
			goapPlan.Clear();
			for ( int i = rev.Num() - 1; i >= 0; i-- ) {
				goapPlan.Append( rev[i] );
			}
			goapPlanStep = 0;
			goapWaitingMove = false;
			if ( g_goapDebug.GetBool() ) {
				gameLocal.Printf( "GOAP %s: plan len %i\n", name.c_str(), goapPlan.Num() );
			}
			return true;
		}

		if ( GoapClosedHas( closedStates, cur.state ) ) {
			continue;
		}
		closedStates.Append( cur.state );

		expansions++;

		for ( int a = 0; a < g_goapNumActions; a++ ) {
			const goapActionDef_t &def = g_goapActions[a];
			if ( ( cur.state & def.preMask ) != def.preMask ) {
				continue;
			}
			if ( !def.canApply( this ) ) {
				continue;
			}
			const unsigned int next = GoapSimulate( a, cur.state );
			if ( next == cur.state ) {
				continue;
			}

			if ( GoapClosedHas( closedStates, next ) ) {
				continue;
			}

			goapNode_t n;
			n.state = next;
			n.g = cur.g + (float)def.cost;
			n.parent = curIdx;
			n.actionFromParent = a;
			const int ni = nodes.Append( n );
			open.Append( ni );
			openF.Append( n.g + h( next ) );
		}
	}

	goapPlan.Clear();
	goapPlanStep = 0;
	goapWaitingMove = false;
	if ( g_goapDebug.GetBool() ) {
		gameLocal.Printf( "GOAP %s: no plan (expansions %i)\n", name.c_str(), expansions );
	}
	return false;
}

/*
================
idAI::GoapExecute
================
*/
void idAI::GoapExecute( void ) {
	if ( !goapEnabled || move.moveType == MOVETYPE_DEAD ) {
		return;
	}

	const unsigned int goal = goapGoalMask;
	const unsigned int w = GoapBuildWorldMask();

	if ( ( w & goal ) == goal ) {
		goapPlan.Clear();
		goapPlanStep = 0;
		goapWaitingMove = false;
		return;
	}

	if ( goapWaitingMove ) {
		if ( AI_MOVE_DONE ) {
			goapWaitingMove = false;
			goapPlanStep++;
		} else {
			return;
		}
	}

	if ( goapPlanStep >= goapPlan.Num() || gameLocal.time - goapLastPlanTime > goapReplanInterval ) {
		goapLastPlanTime = gameLocal.time;
		if ( !GoapPlan() ) {
			return;
		}
		if ( goapPlan.Num() == 0 ) {
			return;
		}
	}

	if ( goapPlanStep >= goapPlan.Num() ) {
		return;
	}

	const int ai = goapPlan[goapPlanStep];
	const goapActionDef_t &def = g_goapActions[ai];

	if ( !def.canApply( this ) ) {
		goapPlan.Clear();
		goapPlanStep = 0;
		goapWaitingMove = false;
		return;
	}

	def.apply( this );

	if ( def.waitMoveDone ) {
		goapWaitingMove = true;
	} else {
		goapPlanStep++;
	}
}
