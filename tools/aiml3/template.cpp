#include "aiml3_internal.h"

#include <cctype>
#include <cstdlib>
#include <sstream>

namespace aiml3 {

static std::string collapse_ws( const std::string &s ) {
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
	return o;
}

int Interpreter::star_index( const std::map<std::string, std::string> &attrs, int defaultOneBased ) {
	auto it = attrs.find( "index" );
	if ( it == attrs.end() ) {
		return defaultOneBased;
	}
	int v = std::atoi( it->second.c_str() );
	if ( v < 1 ) {
		v = 1;
	}
	return v;
}

std::string Interpreter::eval_node( const std::shared_ptr<TmplNode> &n, const Category &cat, const std::vector<std::string> &stars,
	int srai_depth, std::vector<std::string> &srai_stack ) {
	if ( !n ) {
		return "";
	}
	if ( n->kind == TmplNode::kText ) {
		return n->text;
	}

	const std::string &t = n->tag;
	if ( t == "uppercase" ) {
		std::string acc;
		for ( const auto &ch : n->children ) {
			acc += eval_node( ch, cat, stars, srai_depth, srai_stack );
		}
		for ( char &c : acc ) {
			c = ( char )std::toupper( ( unsigned char )c );
		}
		return acc;
	}
	if ( t == "lowercase" ) {
		std::string acc;
		for ( const auto &ch : n->children ) {
			acc += eval_node( ch, cat, stars, srai_depth, srai_stack );
		}
		for ( char &c : acc ) {
			c = ( char )std::tolower( ( unsigned char )c );
		}
		return acc;
	}
	if ( t == "redacted_thinking" || t == "think" ) {
		for ( const auto &ch : n->children ) {
			(void)eval_node( ch, cat, stars, srai_depth, srai_stack );
		}
		return "";
	}
	if ( t == "star" ) {
		int idx = star_index( n->attrs, 1 ) - 1;
		if ( idx >= 0 && idx < ( int )stars.size() ) {
			return stars[( size_t )idx];
		}
		return "";
	}
	if ( t == "get" ) {
		auto it = n->attrs.find( "name" );
		if ( it == n->attrs.end() ) {
			return "";
		}
		auto p = pred_.find( it->second );
		return p == pred_.end() ? std::string() : p->second;
	}
	if ( t == "bot" ) {
		auto it = n->attrs.find( "name" );
		if ( it == n->attrs.end() ) {
			return "";
		}
		auto p = bot_.find( it->second );
		return p == bot_.end() ? std::string() : p->second;
	}
	if ( t == "set" ) {
		auto it = n->attrs.find( "name" );
		if ( it == n->attrs.end() ) {
			return "";
		}
		std::string val;
		for ( const auto &ch : n->children ) {
			val += eval_node( ch, cat, stars, srai_depth, srai_stack );
		}
		pred_[it->second] = val;
		return "";
	}
	if ( t == "map" ) {
		auto it = n->attrs.find( "name" );
		if ( it == n->attrs.end() ) {
			return "";
		}
		std::string key;
		for ( const auto &ch : n->children ) {
			key += eval_node( ch, cat, stars, srai_depth, srai_stack );
		}
		auto m = maps_.find( it->second );
		if ( m == maps_.end() ) {
			return "";
		}
		auto e = m->second.find( key );
		return e == m->second.end() ? std::string() : e->second;
	}
	if ( t == "random" ) {
		std::vector<std::shared_ptr<TmplNode>> lis;
		for ( const auto &ch : n->children ) {
			if ( ch && ch->kind == TmplNode::kElement && ch->tag == "li" ) {
				lis.push_back( ch );
			}
		}
		if ( lis.empty() ) {
			return "";
		}
		const size_t pick = ( size_t )( std::rand() % ( int )lis.size() );
		std::string acc;
		for ( const auto &ch : lis[pick]->children ) {
			acc += eval_node( ch, cat, stars, srai_depth, srai_stack );
		}
		return acc;
	}
	if ( t == "condition" ) {
		auto nit = n->attrs.find( "name" );
		if ( nit == n->attrs.end() ) {
			return "";
		}
		const std::string &pname = nit->second;
		std::string pval;
		auto pit = pred_.find( pname );
		if ( pit != pred_.end() ) {
			pval = pit->second;
		}
		const TmplNode *fallback = nullptr;
		for ( const auto &ch : n->children ) {
			if ( !ch || ch->kind != TmplNode::kElement || ch->tag != "li" ) {
				continue;
			}
			auto vit = ch->attrs.find( "value" );
			if ( vit == ch->attrs.end() ) {
				fallback = ch.get();
			} else if ( vit->second == pval ) {
				std::string acc;
				for ( const auto &gc : ch->children ) {
					acc += eval_node( gc, cat, stars, srai_depth, srai_stack );
				}
				return acc;
			}
		}
		if ( fallback ) {
			std::string acc;
			for ( const auto &gc : fallback->children ) {
				acc += eval_node( gc, cat, stars, srai_depth, srai_stack );
			}
			return acc;
		}
		return "";
	}
	if ( t == "srai" ) {
		return eval_srai( n, cat, stars, srai_depth, srai_stack );
	}

	// Unknown element: evaluate children (lenient) for forward compatibility
	std::string acc;
	for ( const auto &ch : n->children ) {
		acc += eval_node( ch, cat, stars, srai_depth, srai_stack );
	}
	return acc;
}

std::string Interpreter::eval_srai( const std::shared_ptr<TmplNode> &n, const Category &cat, const std::vector<std::string> &stars,
	int srai_depth, std::vector<std::string> &srai_stack ) {
	if ( srai_depth >= cfg_.srai_max_depth ) {
		return "[srai depth limit]";
	}
	std::string q;
	for ( const auto &ch : n->children ) {
		q += eval_node( ch, cat, stars, srai_depth, srai_stack );
	}
	q = collapse_ws( q );

	const std::string stateKey = q + "\x1e" + topic_ + "\x1e" + that_;
	for ( const std::string &prev : srai_stack ) {
		if ( prev == stateKey ) {
			return "[srai cycle]";
		}
	}
	srai_stack.push_back( stateKey );

	std::vector<std::string> utt = normalize_tokens( q );
	std::vector<std::string> thatTok = normalize_tokens( that_ );
	const Category *cat2 = nullptr;
	std::vector<std::string> stars2;
	if ( !find_best_match( utt, thatTok, &cat2, &stars2 ) || !cat2 ) {
		srai_stack.pop_back();
		return "";
	}
	std::string out = eval_template( *cat2, stars2, srai_depth + 1, srai_stack );
	srai_stack.pop_back();
	return out;
}

std::string Interpreter::eval_template( const Category &cat, const std::vector<std::string> &stars, int srai_depth,
	std::vector<std::string> &srai_stack ) {
	if ( !cat.tmpl ) {
		return "";
	}
	std::string acc;
	for ( const auto &ch : cat.tmpl->children ) {
		acc += eval_node( ch, cat, stars, srai_depth, srai_stack );
	}
	return acc;
}

} // namespace aiml3
