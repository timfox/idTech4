/*
===========================================================================

Lightweight Bullet Physics link-time probe (Windows + ID_HAVE_BULLET).
Run console command: bulletProbe

===========================================================================
*/

#include "precompiled.h"
#pragma hdrstop

#include "tr_local.h"

#if defined( _WIN32 ) && defined( ID_HAVE_BULLET )
#include <btBulletDynamicsCommon.h>
#endif

void R_BulletProbe_f( const idCmdArgs &args ) {
	(void)args;
#if defined( _WIN32 ) && defined( ID_HAVE_BULLET )
	btDefaultCollisionConfiguration cfg;
	btCollisionDispatcher dispatcher( &cfg );
	btDbvtBroadphase broadphase;
	btSequentialImpulseConstraintSolver solver;
	btDiscreteDynamicsWorld world( &dispatcher, &broadphase, &solver, &cfg );
	world.setGravity( btVector3( 0, 0, -9.81f ) );

	btCollisionShape *ground = new btStaticPlaneShape( btVector3( 0, 0, 1 ), 0 );
	btCollisionShape *box = new btBoxShape( btVector3( 0.5f, 0.5f, 0.5f ) );

	btTransform groundT;
	groundT.setIdentity();
	btDefaultMotionState *groundMs = new btDefaultMotionState( groundT );
	btRigidBody::btRigidBodyConstructionInfo groundInfo( 0, groundMs, ground, btVector3( 0, 0, 0 ) );
	btRigidBody *groundBody = new btRigidBody( groundInfo );
	world.addRigidBody( groundBody );

	btTransform boxT;
	boxT.setIdentity();
	boxT.setOrigin( btVector3( 0, 0, 4 ) );
	btDefaultMotionState *boxMs = new btDefaultMotionState( boxT );
	const btScalar mass( 1 );
	btVector3 inertia( 0, 0, 0 );
	box->calculateLocalInertia( mass, inertia );
	btRigidBody::btRigidBodyConstructionInfo boxInfo( mass, boxMs, box, inertia );
	btRigidBody *boxBody = new btRigidBody( boxInfo );
	world.addRigidBody( boxBody );

	const int maxSteps = 120;
	for ( int i = 0; i < maxSteps; ++i ) {
		world.stepSimulation( 1.0f / 60.0f, 1, 1.0f / 60.0f );
	}
	const btVector3 p = boxBody->getWorldTransform().getOrigin();
	common->Printf( "bulletProbe: box world Z after sim ~= %.3f (libs linked OK)\n", ( double )p.z() );

	world.removeRigidBody( boxBody );
	world.removeRigidBody( groundBody );
	delete boxBody;
	delete boxMs;
	delete box;
	delete groundBody;
	delete groundMs;
	delete ground;
#else
	common->Printf( "bulletProbe: not available (Windows build with Bullet libs required).\n" );
#endif
}
