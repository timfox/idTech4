#include "aiml3_internal.h"

#include <algorithm>
#include <cctype>

namespace aiml3 {

static std::string trim_copy( std::string s ) {
	while ( !s.empty() && ( unsigned char )s.front() <= ' ' ) {
		s.erase( 0, 1 );
	}
	while ( !s.empty() && ( unsigned char )s.back() <= ' ' ) {
		s.pop_back();
	}
	return s;
}

static char ascii_fold( char c ) {
	if ( c >= 'A' && c <= 'Z' ) {
		return ( char )( c - 'A' + 'a' );
	}
	return c;
}

std::vector<std::string> Interpreter::normalize_tokens( const std::string &s ) const {
	std::string t = trim_copy( s );
	std::string collapsed;
	bool in_ws = false;
	for ( char c : t ) {
		if ( c == ' ' || c == '\t' || c == '\n' || c == '\r' ) {
			if ( !in_ws ) {
				collapsed.push_back( ' ' );
				in_ws = true;
			}
		} else {
			in_ws = false;
			collapsed.push_back( ascii_fold( c ) );
		}
	}
	t = trim_copy( collapsed );

	std::vector<std::string> out;
	std::string cur;
	for ( char c : t ) {
		if ( c == ' ' ) {
			if ( !cur.empty() ) {
				out.push_back( cur );
				cur.clear();
			}
		} else {
			cur.push_back( c );
		}
	}
	if ( !cur.empty() ) {
		out.push_back( cur );
	}
	return out;
}

} // namespace aiml3
