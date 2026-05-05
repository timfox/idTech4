#include "aiml3_internal.h"

#include <algorithm>

namespace aiml3 {

int Category::specificityScore() const {
	int lit = 0, stars = 0, us = 0;
	for ( char c : pattern ) {
		if ( c == '*' ) {
			stars++;
		} else if ( c == '_' ) {
			us++;
		} else if ( c != ' ' && c != '\t' ) {
			lit++;
		}
	}
	// Higher = more specific: many literal chars, few wildcards (§5.3 note).
	return lit * 1000 - stars * 10 - us * 5;
}

int Category::topicScore() const {
	if ( topic.empty() || topic == "*" ) {
		return 0;
	}
	return 1;
}

int Category::thatScore() const {
	return that.empty() ? 0 : 1;
}

bool Interpreter::match_pattern( const std::vector<std::string> &patTok, const std::vector<std::string> &uttTok,
	std::vector<std::string> &stars ) const {
	stars.clear();

	// Recursive matcher: pat index i, utt index j
	struct R {
		static bool go( const std::vector<std::string> &p, const std::vector<std::string> &u, size_t i, size_t j,
			std::vector<std::string> &stars ) {
			if ( i == p.size() ) {
				return j == u.size();
			}
			if ( p[i] == "*" ) {
				// try consume 0..remaining utt tokens
				for ( size_t k = j; k <= u.size(); ++k ) {
					std::string cap;
					for ( size_t t = j; t < k; ++t ) {
						if ( !cap.empty() ) {
							cap.push_back( ' ' );
						}
						cap += u[t];
					}
					const size_t starIdx = stars.size();
					stars.push_back( cap );
					if ( go( p, u, i + 1, k, stars ) ) {
						return true;
					}
					stars.resize( starIdx );
				}
				return false;
			}
			if ( p[i] == "_" ) {
				if ( j >= u.size() ) {
					return false;
				}
				stars.push_back( u[j] );
				return go( p, u, i + 1, j + 1, stars );
			}
			if ( j >= u.size() || p[i] != u[j] ) {
				return false;
			}
			return go( p, u, i + 1, j + 1, stars );
		}
	};

	return R::go( patTok, uttTok, 0, 0, stars );
}

std::vector<std::string> Interpreter::pattern_tokens( const std::string &pat ) const {
	std::vector<std::string> out;
	std::string cur;
	auto flush = [&]() {
		if ( !cur.empty() ) {
			out.push_back( cur );
			cur.clear();
		}
	};
	for ( unsigned char uc : pat ) {
		char c = ( char )uc;
		if ( c == '*' || c == '_' ) {
			flush();
			out.push_back( c == '*' ? std::string( "*" ) : std::string( "_" ) );
		} else if ( c == ' ' || c == '\t' || c == '\n' || c == '\r' ) {
			flush();
		} else {
			cur.push_back( ( char )std::tolower( c ) );
		}
	}
	flush();
	return out;
}

bool Interpreter::find_best_match( const std::vector<std::string> &uttTok, const std::vector<std::string> &thatTok,
	const Category **outCat, std::vector<std::string> *outStars ) const {
	const Category *best = nullptr;
	std::vector<std::string> bestStars;
	int bestScore[4] = { -0x7fffffff, -0x7fffffff, -0x7fffffff, -0x7fffffff };

	for ( const Category &c : cats_ ) {
		const std::string &top = c.topic.empty() ? std::string( "*" ) : c.topic;
		if ( top != "*" && top != topic_ ) {
			continue;
		}
		std::vector<std::string> pat = pattern_tokens( c.pattern );
		std::vector<std::string> stars;
		if ( !match_pattern( pat, uttTok, stars ) ) {
			continue;
		}
		if ( !c.that.empty() ) {
			std::vector<std::string> thatPat = pattern_tokens( c.that );
			std::vector<std::string> thStars;
			if ( !match_pattern( thatPat, thatTok, thStars ) ) {
				continue;
			}
		}

		const int s0 = c.specificityScore();
		const int s1 = c.topicScore();
		const int s2 = c.thatScore();
		// Earlier-defined category wins on tie: smaller fileOrder is better → use large tie score
		const int s3 = 10000000 - c.fileOrder;

		int cand[4] = { s0, s1, s2, s3 };
		if ( !best ) {
			best = &c;
			bestStars = std::move( stars );
			for ( int i = 0; i < 4; ++i ) {
				bestScore[i] = cand[i];
			}
			continue;
		}
		bool better = false;
		for ( int i = 0; i < 4; ++i ) {
			if ( cand[i] > bestScore[i] ) {
				better = true;
				break;
			}
			if ( cand[i] < bestScore[i] ) {
				break;
			}
		}
		if ( better ) {
			best = &c;
			bestStars = std::move( stars );
			for ( int i = 0; i < 4; ++i ) {
				bestScore[i] = cand[i];
			}
		}
	}
	if ( !best ) {
		return false;
	}
	if ( outCat ) {
		*outCat = best;
	}
	if ( outStars ) {
		*outStars = std::move( bestStars );
	}
	return true;
}

} // namespace aiml3
