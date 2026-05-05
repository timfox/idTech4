/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company.

Linux: poll /dev/input/js* (joydev) so Steam Deck / gamepad sticks feed
idUsercmdGenLocal::JoystickMove. Other platforms: no-op stub.

===========================================================================
*/

#include "../../idlib/precompiled.h"
#pragma hdrstop

#include "../framework/CVarSystem.h"
#include "../sys/sys_public.h"

#if defined( __linux__ ) && !defined( ID_DEDICATED )

#include <fcntl.h>
#include <unistd.h>
#include <linux/joystick.h>
#include <errno.h>
#include <cstring>

static idCVar in_joydev( "in_joydev", "/dev/input/js0", CVAR_SYSTEM | CVAR_ARCHIVE,
	"Linux joydev path (Steam Deck: js0 is usually the combined virtual gamepad). Empty = disabled." );
static idCVar in_joy_axisSide( "in_joy_axisSide", "0", CVAR_SYSTEM | CVAR_ARCHIVE | CVAR_INTEGER,
	"joydev axis index for move strafe (AXIS_SIDE)", 0, 7, idCmdSystem::ArgCompletion_Integer<0,7> );
static idCVar in_joy_axisForward( "in_joy_axisForward", "1", CVAR_SYSTEM | CVAR_ARCHIVE | CVAR_INTEGER,
	"joydev axis index for move forward/back (AXIS_FORWARD)", 0, 7, idCmdSystem::ArgCompletion_Integer<0,7> );
static idCVar in_joy_axisUp( "in_joy_axisUp", "2", CVAR_SYSTEM | CVAR_ARCHIVE | CVAR_INTEGER,
	"joydev axis index for jump/crouch (AXIS_UP), often L2 analog", 0, 7, idCmdSystem::ArgCompletion_Integer<0,7> );
static idCVar in_joy_axisLookYaw( "in_joy_axisLookYaw", "3", CVAR_SYSTEM | CVAR_ARCHIVE | CVAR_INTEGER,
	"joydev axis index for right-stick look yaw (Deck: often axis 2 or 3)", 0, 7, idCmdSystem::ArgCompletion_Integer<0,7> );
static idCVar in_joy_axisLookPitch( "in_joy_axisLookPitch", "4", CVAR_SYSTEM | CVAR_ARCHIVE | CVAR_INTEGER,
	"joydev axis index for right-stick look pitch", 0, 7, idCmdSystem::ArgCompletion_Integer<0,7> );
static idCVar in_joy_deadzone( "in_joy_deadzone", "20", CVAR_SYSTEM | CVAR_ARCHIVE | CVAR_INTEGER,
	"Percent deadzone for analog sticks (0-90)", 0, 90, idCmdSystem::ArgCompletion_Integer<0,90> );

namespace {
int g_joyFd = -1;
int g_axisAccum[8];
bool g_warnedOpenFail = false;

static void joydev_shutdown() {
	if ( g_joyFd >= 0 ) {
		close( g_joyFd );
		g_joyFd = -1;
	}
	memset( g_axisAccum, 0, sizeof( g_axisAccum ) );
}

static int joydev_scale( int v ) {
	const int64_t x = (int64_t)v * 127LL / 32767LL;
	if ( x > 127 ) {
		return 127;
	}
	if ( x < -127 ) {
		return -127;
	}
	return (int)x;
}

static void joydev_apply_deadzone( int *out8 ) {
	const int dz = in_joy_deadzone.GetInteger();
	if ( dz <= 0 ) {
		return;
	}
	const int thresh = (int)( 32767.0f * (float)dz / 100.0f );
	for ( int i = 0; i < 8; ++i ) {
		if ( out8[i] > -thresh && out8[i] < thresh ) {
			out8[i] = 0;
		}
	}
}
} // namespace

void Sys_ShutdownJoystickFill( void ) {
	joydev_shutdown();
	g_warnedOpenFail = false;
}

void Sys_FillJoystickAxes( int *axes, int num ) {
	if ( !axes || num <= 0 ) {
		return;
	}
	memset( axes, 0, (size_t)num * sizeof( axes[0] ) );

	const char *path = in_joydev.GetString();
	if ( !path || !path[0] ) {
		return;
	}

	if ( g_joyFd < 0 ) {
		g_joyFd = open( path, O_RDONLY | O_NONBLOCK );
		if ( g_joyFd < 0 ) {
			if ( !g_warnedOpenFail && common ) {
				common->Warning( "in_joydev: could not open %s (%s) — gamepad move disabled", path, strerror( errno ) );
				g_warnedOpenFail = true;
			}
			return;
		}
		g_warnedOpenFail = false;
		memset( g_axisAccum, 0, sizeof( g_axisAccum ) );
		if ( common ) {
			common->Printf( "Linux joydev: opened %s\n", path );
		}
	}

	js_event ev;
	int nread;
	while ( ( nread = read( g_joyFd, &ev, sizeof( ev ) ) ) == (int)sizeof( ev ) ) {
		if ( ev.type & JS_EVENT_INIT ) {
			continue;
		}
		if ( ev.type == JS_EVENT_AXIS && ev.number < 8 ) {
			g_axisAccum[ev.number] = (int)ev.value;
		}
	}
	if ( nread < 0 && errno != EAGAIN && errno != EWOULDBLOCK ) {
		common->Warning( "in_joydev: read error on %s (%s), closing", path, strerror( errno ) );
		joydev_shutdown();
		return;
	}

	int mapped[8];
	memcpy( mapped, g_axisAccum, sizeof( mapped ) );
	joydev_apply_deadzone( mapped );

	const int as = idMath::ClampInt( 0, 7, in_joy_axisSide.GetInteger() );
	const int af = idMath::ClampInt( 0, 7, in_joy_axisForward.GetInteger() );
	const int au = idMath::ClampInt( 0, 7, in_joy_axisUp.GetInteger() );
	if ( as < num ) {
		axes[AXIS_SIDE] = joydev_scale( mapped[as] );
	}
	if ( af < num ) {
		axes[AXIS_FORWARD] = joydev_scale( mapped[af] );
	}
	if ( au < num ) {
		axes[AXIS_UP] = joydev_scale( mapped[au] );
	}
	if ( num > AXIS_ROLL ) {
		const int ay = idMath::ClampInt( 0, 7, in_joy_axisLookYaw.GetInteger() );
		const int ap = idMath::ClampInt( 0, 7, in_joy_axisLookPitch.GetInteger() );
		if ( ay < num ) {
			axes[AXIS_YAW] = joydev_scale( mapped[ay] );
		}
		if ( ap < num ) {
			axes[AXIS_PITCH] = joydev_scale( mapped[ap] );
		}
	}
}

#else

void Sys_ShutdownJoystickFill( void ) {
}

void Sys_FillJoystickAxes( int *axes, int num ) {
	if ( !axes || num <= 0 ) {
		return;
	}
	memset( axes, 0, (size_t)num * sizeof( axes[0] ) );
}

#endif
