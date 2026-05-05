/*
 * Demo: Clipper2 polygon union + optional GLPK (glpsol) solve of a MathProg .mod file.
 * id Tech 5 used AMPL + Clipper in the tools chain; we mirror that with portable open tools.
 */

#include <clipper2/clipper.h>

#include <cstdio>
#include <cstdlib>
#include <string>

#if defined(_WIN32)
#include <io.h>
#define PATH_SEP '\\'
#else
#include <unistd.h>
#define PATH_SEP '/'
#endif

static std::string DirOfExecutable( const char *argv0 ) {
	std::string s = argv0 ? argv0 : "";
	const size_t pos = s.find_last_of( "/\\" );
	if ( pos == std::string::npos ) {
		return ".";
	}
	return s.substr( 0, pos );
}

static std::string JoinPath( const std::string &a, const std::string &b ) {
	if ( a.empty() ) {
		return b;
	}
	if ( !a.empty() && a.back() != '/' && a.back() != '\\' ) {
		return a + PATH_SEP + b;
	}
	return a + b;
}

static int RunGlpsolIfAvailable( const std::string &modPath ) {
#if defined(_WIN32)
	if ( std::system( "where glpsol >nul 2>nul" ) != 0 ) {
		std::printf( "AMPL/MathProg demo: glpsol not on PATH — install GLPK for Windows or use AMPL with this .mod:\n  %s\n", modPath.c_str() );
		return 0;
	}
	std::string cmd = "glpsol -m \"" + modPath + "\" >nul 2>&1";
#else
	if ( std::system( "command -v glpsol >/dev/null 2>&1" ) != 0 ) {
		std::printf( "AMPL/MathProg demo: glpsol not on PATH — install glpk-utils or use AMPL with this .mod:\n  %s\n", modPath.c_str() );
		return 0;
	}
	std::string cmd = "glpsol -m \"" + modPath + "\" >/dev/null 2>&1";
#endif
	std::printf( "Running: glpsol -m %s\n", modPath.c_str() );
	const int rc = std::system( cmd.c_str() );
	if ( rc != 0 ) {
		std::printf( "glpsol exited with code %d (model may be infeasible or glpsol missing)\n", rc );
		return 0;
	}
	std::printf( "glpsol: completed successfully (see glpsol output above if logging enabled).\n" );
	return 0;
}

static void ClipperDemo() {
	using namespace Clipper2Lib;
	Path64 a;
	a.push_back( Point64( 0, 0 ) );
	a.push_back( Point64( 100, 0 ) );
	a.push_back( Point64( 100, 100 ) );
	a.push_back( Point64( 0, 100 ) );

	Path64 b;
	b.push_back( Point64( 50, 50 ) );
	b.push_back( Point64( 150, 50 ) );
	b.push_back( Point64( 150, 150 ) );
	b.push_back( Point64( 50, 150 ) );

	Paths64 subj;
	subj.push_back( a );
	Paths64 clip;
	clip.push_back( b );

	const Paths64 u = Union( subj, clip, FillRule::NonZero );
	std::printf( "Clipper2: union of two axis-aligned squares -> %zu path(s)\n", u.size() );
	for ( size_t i = 0; i < u.size(); i++ ) {
		std::printf( "  path %zu: %zu vertices\n", i, u[i].size() );
	}
}

int main( int argc, char **argv ) {
	(void)argc;
	const std::string here = DirOfExecutable( argv[0] );
	const std::string mod = JoinPath( here, JoinPath( "examples", "page_budget.mod" ) );

	RunGlpsolIfAvailable( mod );
	ClipperDemo();
	return 0;
}
