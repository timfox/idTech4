#include "aiml3_internal.h"

#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>

static void usage() {
	std::fprintf( stderr,
		"aiml3 — AIML 3.0 Core Dialog reference interpreter (experimental)\n"
		"  aiml3 --load <file.aiml|file.json> [--srai-depth N]\n"
		"  Reads lines from stdin; type quit to exit.\n"
		"  Spec submodule: third_party/aiml-3.0-spec\n" );
}

int main( int argc, char **argv ) {
	aiml3::InterpreterConfig cfg;
	const char *loadPath = nullptr;
	for ( int i = 1; i < argc; ++i ) {
		if ( std::strcmp( argv[i], "--load" ) == 0 && i + 1 < argc ) {
			loadPath = argv[++i];
		} else if ( std::strcmp( argv[i], "--srai-depth" ) == 0 && i + 1 < argc ) {
			cfg.srai_max_depth = std::atoi( argv[++i] );
			if ( cfg.srai_max_depth < 1 ) {
				cfg.srai_max_depth = 1;
			}
		} else if ( std::strcmp( argv[i], "-h" ) == 0 || std::strcmp( argv[i], "--help" ) == 0 ) {
			usage();
			return 0;
		} else {
			std::fprintf( stderr, "unknown arg: %s\n", argv[i] );
			usage();
			return 1;
		}
	}
	if ( !loadPath ) {
		usage();
		return 1;
	}

	aiml3::Interpreter interp( cfg );
	std::string err;
	const char *ext = std::strrchr( loadPath, '.' );
	bool ok = false;
	if ( ext && ( std::strcmp( ext, ".json" ) == 0 || std::strcmp( ext, ".aimlj" ) == 0 ) ) {
		ok = aiml3::load_json_file( interp, loadPath, err );
	} else {
		ok = aiml3::load_xml_file( interp, loadPath, err );
	}
	if ( !ok ) {
		std::fprintf( stderr, "load failed: %s\n", err.c_str() );
		return 1;
	}

	std::string line;
	std::cout << "aiml3 loaded. Enter input (quit to exit).\n";
	while ( std::getline( std::cin, line ) ) {
		if ( line == "quit" || line == "exit" ) {
			break;
		}
		std::string out = interp.respond( line );
		if ( out.empty() ) {
			std::cout << "(no match)\n";
		} else {
			std::cout << out << "\n";
		}
	}
	return 0;
}
