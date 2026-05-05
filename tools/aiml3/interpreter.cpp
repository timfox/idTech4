#include "aiml3_internal.h"

#include <cstdlib>
#include <sstream>

namespace aiml3 {

Interpreter::Interpreter( InterpreterConfig cfg ) : cfg_( std::move( cfg ) ) {
	topic_ = cfg_.default_topic;
	that_.clear();
}

void Interpreter::clear_graph() {
	cats_.clear();
}

void Interpreter::add_category( Category c ) {
	cats_.push_back( std::move( c ) );
}

void Interpreter::set_bot( const std::string &key, const std::string &val ) {
	bot_[key] = val;
}

void Interpreter::set_map( const std::string &mapName, const std::string &key, const std::string &val ) {
	maps_[mapName][key] = val;
}

static std::string canonical_that( const std::string &s ) {
	std::string o;
	bool sp = false;
	for ( char c : s ) {
		if ( c == ' ' || c == '\t' || c == '\n' || c == '\r' ) {
			sp = true;
		} else {
			if ( sp && !o.empty() ) {
				o.push_back( ' ' );
			}
			sp = false;
			o.push_back( c );
		}
	}
	// trim
	while ( !o.empty() && o.front() == ' ' ) {
		o.erase( 0, 1 );
	}
	while ( !o.empty() && o.back() == ' ' ) {
		o.pop_back();
	}
	return o;
}

std::string Interpreter::respond( const std::string &rawInput ) {
	std::vector<std::string> utt = normalize_tokens( rawInput );
	std::vector<std::string> thatTok = normalize_tokens( that_ );

	const Category *cat = nullptr;
	std::vector<std::string> stars;
	if ( !find_best_match( utt, thatTok, &cat, &stars ) || !cat ) {
		return "";
	}

	std::vector<std::string> stack;
	std::string out = eval_template( *cat, stars, 0, stack );
	that_ = canonical_that( out );
	return out;
}

} // namespace aiml3
