/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company.

Additional in-engine asset workflow helpers (studio-style iteration).

===========================================================================
*/

#include "precompiled.h"
#pragma hdrstop

#include "../Game_local.h"

namespace {

static const float kStudioDefaultPickDistance = 8192.0f;

static bool StudioCheatsOk( void ) {
	if ( !gameLocal.CheatsOk() ) {
		return false;
	}
	return true;
}

static idVec3 StudioGetEyeForward( idVec3 &outForward ) {
	idPlayer *player = gameLocal.GetLocalPlayer();
	if ( !player ) {
		outForward.Zero();
		return vec3_origin;
	}
	const renderView_t *rv = player->GetRenderView();
	idVec3 origin;
	idAngles angles;
	if ( rv ) {
		origin = rv->vieworg;
		angles = rv->viewaxis.ToAngles();
	} else {
		player->GetViewPos( origin, idMat3() );
		angles = player->viewAngles;
	}
	outForward = angles.ToForward();
	return origin;
}

static void StudioJoinArgs( const idCmdArgs &args, int start, idStr &out ) {
	out.Clear();
	for ( int i = start; i < args.Argc(); i++ ) {
		if ( i > start ) {
			out += ' ';
		}
		out += args.Argv( i );
	}
}

/*
=================
Cmd_studioHelp_f
=================
*/
static void Cmd_studioHelp_f( const idCmdArgs &args ) {
	gameLocal.Printf(
		"In-engine studio tools (cheat; use with dev assets on fs_devpath/fs_savepath):\n"
		"  studioHelp                 — this list\n"
		"  studioPick [distance]    — trace from view; print entity, model, skin, surface material\n"
		"  studioSpawnArgSet <entity> <key> <value...> — set spawn key on entity and UpdateVisuals\n"
		"  studioShaderParm <entity> <index 0-11> <float> — set renderEntity.shaderParms[index]\n"
		"  studioReloadDecl <type> <name> — invalidate + reparse decl (material, fx, particle, skin, ...)\n"
		"  studioReloadDeclFile <relativePath> — declManager->ReloadFile (e.g. materials/foo.mtr)\n"
		"  studioReloadMaterialImages <name> — reload images for one material\n"
		"  studioBrowseAssets <folder> [ext] — list files under base path (e.g. materials .mtr)\n"
		"  studioWriteDevFile <relativePath> <line...> — append one line to fs_devpath (UTF-8 text)\n"
		"  studioReloadMedia [force] — reloadImages all; reloadModels all; reloadSounds [force]; declManager->Reload\n"
	);
}

/*
=================
Cmd_studioPick_f
=================
*/
static void Cmd_studioPick_f( const idCmdArgs &args ) {
	if ( !StudioCheatsOk() ) {
		return;
	}
	idPlayer *player = gameLocal.GetLocalPlayer();
	if ( !player ) {
		gameLocal.Printf( "studioPick: no local player\n" );
		return;
	}
	float dist = kStudioDefaultPickDistance;
	if ( args.Argc() >= 2 ) {
		dist = atof( args.Argv( 1 ) );
		if ( dist <= 0.0f ) {
			dist = kStudioDefaultPickDistance;
		}
	}
	idVec3 forward;
	const idVec3 start = StudioGetEyeForward( forward );
	const idVec3 end = start + forward * dist;
	trace_t trace;
	gameLocal.clip.TracePoint( trace, start, end, MASK_SHOT_RENDERMODEL, player );
	if ( trace.fraction >= 1.0f ) {
		gameLocal.Printf( "studioPick: no hit (trace to %.0f units)\n", dist );
		return;
	}
	gameLocal.Printf( "studioPick: fraction=%.4f endpos=(%s)\n", trace.fraction, trace.endpos.ToString() );
	if ( trace.c.material ) {
		gameLocal.Printf( "  surface material: %s\n", trace.c.material->GetName() );
	}
	if ( trace.c.entityNum >= 0 && trace.c.entityNum < MAX_GENTITIES ) {
		idEntity *ent = gameLocal.entities[ trace.c.entityNum ];
		if ( ent ) {
			gameLocal.Printf( "  entity #%i name=%s class=%s\n", trace.c.entityNum, ent->name.c_str(), ent->GetClassname() );
			const char *mdl = ent->spawnArgs.GetString( "model", "" );
			if ( mdl && mdl[0] ) {
				gameLocal.Printf( "  model: %s\n", mdl );
			}
			const char *sk = ent->spawnArgs.GetString( "skin", "" );
			if ( sk && sk[0] ) {
				gameLocal.Printf( "  skin: %s\n", sk );
			}
		} else {
			gameLocal.Printf( "  entity #%i (null)\n", trace.c.entityNum );
		}
	}
}

/*
=================
Cmd_studioSpawnArgSet_f
=================
*/
static void Cmd_studioSpawnArgSet_f( const idCmdArgs &args ) {
	if ( !StudioCheatsOk() ) {
		return;
	}
	if ( args.Argc() < 4 ) {
		gameLocal.Printf( "usage: studioSpawnArgSet <entityName> <key> <value...>\n" );
		return;
	}
	idEntity *ent = gameLocal.FindEntity( args.Argv( 1 ) );
	if ( !ent ) {
		gameLocal.Printf( "studioSpawnArgSet: entity \"%s\" not found\n", args.Argv( 1 ) );
		return;
	}
	idStr value;
	StudioJoinArgs( args, 3, value );
	ent->spawnArgs.Set( args.Argv( 2 ), value );
	ent->UpdateVisuals();
	gameLocal.Printf( "studioSpawnArgSet: %s.%s = \"%s\"\n", ent->name.c_str(), args.Argv( 2 ), value.c_str() );
}

/*
=================
Cmd_studioShaderParm_f
=================
*/
static void Cmd_studioShaderParm_f( const idCmdArgs &args ) {
	if ( !StudioCheatsOk() ) {
		return;
	}
	if ( args.Argc() != 4 ) {
		gameLocal.Printf( "usage: studioShaderParm <entityName> <parmIndex 0-11> <floatValue>\n" );
		return;
	}
	idEntity *ent = gameLocal.FindEntity( args.Argv( 1 ) );
	if ( !ent ) {
		gameLocal.Printf( "studioShaderParm: entity \"%s\" not found\n", args.Argv( 1 ) );
		return;
	}
	int idx = atoi( args.Argv( 2 ) );
	if ( idx < 0 || idx >= MAX_ENTITY_SHADER_PARMS ) {
		gameLocal.Printf( "studioShaderParm: bad index %i\n", idx );
		return;
	}
	float v = atof( args.Argv( 3 ) );
	renderEntity_t *re = ent->GetRenderEntity();
	if ( !re ) {
		gameLocal.Printf( "studioShaderParm: entity has no renderEntity\n" );
		return;
	}
	re->shaderParms[idx] = v;
	ent->UpdateVisuals();
	gameLocal.Printf( "studioShaderParm: %s shaderParms[%i] = %f\n", ent->name.c_str(), idx, v );
}

static declType_t StudioDeclTypeFromArg( const char *name ) {
	if ( !name || !name[0] ) {
		return DECL_MAX_TYPES;
	}
	return declManager->GetDeclTypeFromName( name );
}

/*
=================
Cmd_studioReloadDecl_f
=================
*/
static void Cmd_studioReloadDecl_f( const idCmdArgs &args ) {
	if ( !StudioCheatsOk() ) {
		return;
	}
	if ( args.Argc() != 3 ) {
		gameLocal.Printf( "usage: studioReloadDecl <declType> <name>\n" );
		gameLocal.Printf( "  declType examples: material, skin, sound, fx, particle, entityDef, model\n" );
		return;
	}
	declType_t t = StudioDeclTypeFromArg( args.Argv( 1 ) );
	if ( t <= DECL_TABLE || t >= DECL_MAX_TYPES ) {
		gameLocal.Printf( "studioReloadDecl: unknown decl type \"%s\"\n", args.Argv( 1 ) );
		return;
	}
	const idDecl *d = declManager->FindType( t, args.Argv( 2 ), false );
	if ( !d ) {
		gameLocal.Printf( "studioReloadDecl: %s \"%s\" not found\n", args.Argv( 1 ), args.Argv( 2 ) );
		return;
	}
	idDecl *mut = const_cast<idDecl *>( d );
	mut->Invalidate();
	mut->EnsureNotPurged();
	if ( t == DECL_MATERIAL ) {
		const idMaterial *mat = declManager->FindMaterial( args.Argv( 2 ), false );
		if ( mat ) {
			const_cast<idMaterial *>( mat )->ReloadImages( true );
		}
	}
	gameLocal.Printf( "studioReloadDecl: reloaded %s \"%s\"\n", args.Argv( 1 ), args.Argv( 2 ) );
}

/*
=================
Cmd_studioReloadDeclFile_f
=================
*/
static void Cmd_studioReloadDeclFile_f( const idCmdArgs &args ) {
	if ( !StudioCheatsOk() ) {
		return;
	}
	if ( args.Argc() != 2 ) {
		gameLocal.Printf( "usage: studioReloadDeclFile <relativePath>\n" );
		return;
	}
	declManager->ReloadFile( args.Argv( 1 ), true );
	gameLocal.Printf( "studioReloadDeclFile: %s\n", args.Argv( 1 ) );
}

/*
=================
Cmd_studioReloadMaterialImages_f
=================
*/
static void Cmd_studioReloadMaterialImages_f( const idCmdArgs &args ) {
	if ( !StudioCheatsOk() ) {
		return;
	}
	if ( args.Argc() != 2 ) {
		gameLocal.Printf( "usage: studioReloadMaterialImages <materialName>\n" );
		return;
	}
	const idMaterial *mat = declManager->FindMaterial( args.Argv( 1 ), false );
	if ( !mat ) {
		gameLocal.Printf( "studioReloadMaterialImages: material \"%s\" not found\n", args.Argv( 1 ) );
		return;
	}
	const_cast<idMaterial *>( mat )->ReloadImages( true );
	gameLocal.Printf( "studioReloadMaterialImages: %s\n", args.Argv( 1 ) );
}

/*
=================
Cmd_studioBrowseAssets_f
=================
*/
static void Cmd_studioBrowseAssets_f( const idCmdArgs &args ) {
	if ( !StudioCheatsOk() ) {
		return;
	}
	if ( args.Argc() < 2 || args.Argc() > 3 ) {
		gameLocal.Printf( "usage: studioBrowseAssets <folder> [.ext]\n" );
		gameLocal.Printf( "  folder: materials, models, sounds, particles, fx, skins, def, textures, ...\n" );
		return;
	}
	const char *folder = args.Argv( 1 );
	const char *ext = ( args.Argc() == 3 ) ? args.Argv( 2 ) : "/";
	idFileList *list = fileSystem->ListFilesTree( folder, ext, true );
	if ( !list ) {
		gameLocal.Printf( "studioBrowseAssets: no list\n" );
		return;
	}
	const int n = list->GetNumFiles();
	gameLocal.Printf( "studioBrowseAssets: %s (%i files)\n", folder, n );
	const int maxPrint = idMath::Min( n, 200 );
	for ( int i = 0; i < maxPrint; i++ ) {
		gameLocal.Printf( "  %s\n", list->GetFile( i ) );
	}
	if ( n > maxPrint ) {
		gameLocal.Printf( "  ... %i more (narrow with extension)\n", n - maxPrint );
	}
	fileSystem->FreeFileList( list );
}

/*
=================
Cmd_studioWriteDevFile_f
=================
*/
static void Cmd_studioWriteDevFile_f( const idCmdArgs &args ) {
	if ( !StudioCheatsOk() ) {
		return;
	}
	if ( args.Argc() < 3 ) {
		gameLocal.Printf( "usage: studioWriteDevFile <relativePathUnderGame> <text...>\n" );
		gameLocal.Printf( "  writes to fs_devpath (set +set fs_devpath <dir> for your mod tree)\n" );
		return;
	}
	idStr line;
	StudioJoinArgs( args, 2, line );
	line += "\n";
	idFile *f = fileSystem->OpenFileAppend( args.Argv( 1 ), true, "fs_devpath" );
	if ( !f ) {
		gameLocal.Printf( "studioWriteDevFile: could not open for append (path=%s); is fs_devpath set?\n", args.Argv( 1 ) );
		return;
	}
	const int w = f->Write( line.c_str(), line.Length() );
	fileSystem->CloseFile( f );
	if ( w != line.Length() ) {
		gameLocal.Printf( "studioWriteDevFile: short write (%i of %i bytes)\n", w, line.Length() );
		return;
	}
	gameLocal.Printf( "studioWriteDevFile: appended %i bytes to %s\n", w, args.Argv( 1 ) );
}

/*
=================
Cmd_studioReloadMedia_f
=================
*/
static void Cmd_studioReloadMedia_f( const idCmdArgs &args ) {
	if ( !StudioCheatsOk() ) {
		return;
	}
	const bool forceSound = ( args.Argc() >= 2 && !idStr::Icmp( args.Argv( 1 ), "force" ) );
	gameLocal.Printf( "studioReloadMedia: starting...\n" );
	cmdSystem->BufferCommandText( CMD_EXEC_APPEND, "reloadImages all\n" );
	cmdSystem->BufferCommandText( CMD_EXEC_APPEND, "reloadModels all\n" );
	cmdSystem->BufferCommandText( CMD_EXEC_APPEND, forceSound ? "reloadSounds force\n" : "reloadSounds\n" );
	declManager->Reload( false );
	gameLocal.Printf( "studioReloadMedia: queued reloadImages/reloadModels/reloadSounds; declManager->Reload(false) done\n" );
}

} // namespace

/*
=================
idGameLocal::RegisterStudioCommands
=================
*/
void idGameLocal::RegisterStudioCommands( void ) {
	cmdSystem->AddCommand( "studioHelp",					Cmd_studioHelp_f,					CMD_FL_GAME|CMD_FL_CHEAT,	"in-engine asset studio: command summary" );
	cmdSystem->AddCommand( "studioPick",					Cmd_studioPick_f,					CMD_FL_GAME|CMD_FL_CHEAT,	"trace from view; print entity / model / skin / material" );
	cmdSystem->AddCommand( "studioSpawnArgSet",				Cmd_studioSpawnArgSet_f,			CMD_FL_GAME|CMD_FL_CHEAT,	"set entity spawn key and refresh visuals", idGameLocal::ArgCompletion_EntityName );
	cmdSystem->AddCommand( "studioShaderParm",				Cmd_studioShaderParm_f,				CMD_FL_GAME|CMD_FL_CHEAT,	"set entity shaderParm by index", idGameLocal::ArgCompletion_EntityName );
	cmdSystem->AddCommand( "studioReloadDecl",				Cmd_studioReloadDecl_f,				CMD_FL_GAME|CMD_FL_CHEAT,	"invalidate and reparse one decl by type and name" );
	cmdSystem->AddCommand( "studioReloadDeclFile",			Cmd_studioReloadDeclFile_f,			CMD_FL_GAME|CMD_FL_CHEAT,	"reload all decls from one source file" );
	cmdSystem->AddCommand( "studioReloadMaterialImages",	Cmd_studioReloadMaterialImages_f,	CMD_FL_GAME|CMD_FL_CHEAT,	"reload images for a material", idCmdSystem::ArgCompletion_Decl<DECL_MATERIAL> );
	cmdSystem->AddCommand( "studioBrowseAssets",			Cmd_studioBrowseAssets_f,			CMD_FL_GAME|CMD_FL_CHEAT,	"list asset files under a vfs folder" );
	cmdSystem->AddCommand( "studioWriteDevFile",			Cmd_studioWriteDevFile_f,			CMD_FL_GAME|CMD_FL_CHEAT,	"append a line of text to a file under fs_devpath" );
	cmdSystem->AddCommand( "studioReloadMedia",				Cmd_studioReloadMedia_f,			CMD_FL_GAME|CMD_FL_CHEAT,	"reload images, models, sounds, and decls (optional: force for sounds)" );
}
