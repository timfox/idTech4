#include "aiml3_internal.h"

#include <cctype>
#include <cstring>
#include <fstream>
#include <sstream>

namespace aiml3 {

namespace {

std::string read_all( const char *path, std::string &err ) {
	std::ifstream in( path, std::ios::binary );
	if ( !in ) {
		err = "cannot open file";
		return {};
	}
	std::ostringstream ss;
	ss << in.rdbuf();
	return ss.str();
}

void trim_inplace( std::string &s ) {
	while ( !s.empty() && std::isspace( ( unsigned char )s.front() ) ) {
		s.erase( 0, 1 );
	}
	while ( !s.empty() && std::isspace( ( unsigned char )s.back() ) ) {
		s.pop_back();
	}
}

void skip_ws( const char *&p ) {
	while ( *p && std::isspace( ( unsigned char )*p ) ) {
		++p;
	}
}

bool parse_quoted_string( const char *&p, std::string &out, std::string &err ) {
	skip_ws( p );
	if ( *p != '"' ) {
		err = "expected '\"' for string";
		return false;
	}
	++p;
	out.clear();
	while ( *p && *p != '"' ) {
		if ( *p == '\\' && p[1] ) {
			++p;
			out.push_back( *p++ );
			continue;
		}
		out.push_back( *p++ );
	}
	if ( *p != '"' ) {
		err = "unterminated string";
		return false;
	}
	++p;
	return true;
}

bool parse_string_array( const char *&p, std::vector<std::string> &out, std::string &err ) {
	skip_ws( p );
	if ( *p != '[' ) {
		err = "expected '[' for string array";
		return false;
	}
	++p;
	out.clear();
	for ( ;; ) {
		skip_ws( p );
		if ( *p == ']' ) {
			++p;
			return true;
		}
		std::string item;
		if ( !parse_quoted_string( p, item, err ) ) {
			return false;
		}
		out.push_back( std::move( item ) );
		skip_ws( p );
		if ( *p == ']' ) {
			++p;
			return true;
		}
		if ( *p != ',' ) {
			err = "expected ',' or ']' in string array";
			return false;
		}
		++p;
	}
}

// Skip balanced parens/braces/brackets from *p (first char is opening), respecting double-quoted strings.
bool skip_balanced( const char *&p, char open, char close, std::string &err ) {
	if ( *p != open ) {
		err = "skip_balanced: bad open";
		return false;
	}
	int depth = 1;
	++p;
	while ( *p && depth > 0 ) {
		if ( *p == '"' ) {
			++p;
			while ( *p && *p != '"' ) {
				if ( *p == '\\' && p[1] ) {
					p += 2;
				} else {
					++p;
				}
			}
			if ( *p == '"' ) {
				++p;
			}
			continue;
		}
		if ( *p == open ) {
			++depth;
		} else if ( *p == close ) {
			--depth;
		}
		++p;
	}
	if ( depth != 0 ) {
		err = "unbalanced delimiters";
		return false;
	}
	return true;
}

void parse_custom_data_inner( const std::string &inner, std::string &cooldownOut ) {
	const char *b = inner.c_str();
	while ( *b ) {
		skip_ws( b );
		if ( !*b ) {
			break;
		}
		if ( strncmp( b, "string", 6 ) == 0 && std::isspace( ( unsigned char )b[6] ) ) {
			b += 6;
			skip_ws( b );
			std::string key;
			while ( *b && ( std::isalnum( ( unsigned char )*b ) || *b == '_' ) ) {
				key.push_back( *b++ );
			}
			skip_ws( b );
			if ( *b != '=' ) {
				continue;
			}
			++b;
			std::string val;
			std::string err;
			if ( !parse_quoted_string( b, val, err ) ) {
				return;
			}
			if ( key == "cooldown" ) {
				cooldownOut = val;
			}
			continue;
		}
		++b;
	}
}

bool parse_category_block( const char *body, const char *bodyEnd, Category &cat, std::string &err ) {
	const char *p = body;
	while ( p < bodyEnd ) {
		skip_ws( p );
		if ( p >= bodyEnd ) {
			break;
		}
		if ( strncmp( p, "customData", 10 ) == 0 && std::isspace( ( unsigned char )p[10] ) ) {
			p += 10;
			skip_ws( p );
			if ( *p != '=' ) {
				err = "customData missing '='";
				return false;
			}
			++p;
			skip_ws( p );
			if ( *p != '{' ) {
				err = "customData missing '{'";
				return false;
			}
			const char *innerStart = p;
			if ( !skip_balanced( p, '{', '}', err ) ) {
				return false;
			}
			std::string inner( innerStart + 1, p - innerStart - 2 );
			parse_custom_data_inner( inner, cat.cooldown );
			continue;
		}
		if ( strncmp( p, "string", 6 ) == 0 && std::isspace( ( unsigned char )p[6] ) ) {
			p += 6;
			skip_ws( p );
			std::string key;
			while ( p < bodyEnd && ( std::isalnum( ( unsigned char )*p ) || *p == '_' ) ) {
				key.push_back( *p++ );
			}
			skip_ws( p );
			if ( *p != '=' ) {
				err = "expected '=' after property name";
				return false;
			}
			++p;
			std::string val;
			if ( !parse_quoted_string( p, val, err ) ) {
				return false;
			}
			if ( key == "pattern" ) {
				cat.pattern = val;
			} else if ( key == "that" ) {
				cat.that = val;
			} else if ( key == "topic" ) {
				cat.topic = val;
			} else if ( key == "template" ) {
				if ( val.find( '<' ) != std::string::npos ) {
					cat.tmpl = parse_template_xmlish( val, err );
					if ( !cat.tmpl ) {
						return false;
					}
				} else {
					cat.tmpl = template_from_plain_string( val );
				}
			}
			skip_ws( p );
			if ( *p == ';' ) {
				++p;
			}
			continue;
		}
		if ( strncmp( p, "custom", 6 ) == 0 && std::isspace( ( unsigned char )p[6] ) ) {
			p += 6;
			skip_ws( p );
			if ( strncmp( p, "string[]", 8 ) != 0 ) {
				// unknown custom type — skip line
				while ( p < bodyEnd && *p != ';' ) {
					++p;
				}
				if ( *p == ';' ) {
					++p;
				}
				continue;
			}
			p += 8;
			skip_ws( p );
			std::string key;
			while ( p < bodyEnd && ( std::isalnum( ( unsigned char )*p ) || *p == '_' ) ) {
				key.push_back( *p++ );
			}
			skip_ws( p );
			if ( *p != '=' ) {
				err = "expected '=' after custom string[] name";
				return false;
			}
			++p;
			std::vector<std::string> arr;
			if ( !parse_string_array( p, arr, err ) ) {
				return false;
			}
			if ( key == "directives" ) {
				cat.directives = std::move( arr );
			} else if ( key == "requires" ) {
				cat.require_clauses = std::move( arr );
			} else if ( key == "unless" ) {
				cat.unless = std::move( arr );
			} else if ( key == "effects" ) {
				cat.effects = std::move( arr );
			}
			skip_ws( p );
			if ( *p == ';' ) {
				++p;
			}
			continue;
		}
		// skip unknown statement until ';'
		while ( p < bodyEnd && *p != ';' ) {
			++p;
		}
		if ( *p == ';' ) {
			++p;
		}
	}
	return true;
}

const char *find_def_aiml_category( const char *p, const char *end ) {
	while ( p < end ) {
		if ( strncmp( p, "def", 3 ) == 0 ) {
			const char *after = p + 3;
			if ( after < end && std::isspace( ( unsigned char )*after ) ) {
				const char *q = after;
				skip_ws( q );
				if ( end - q >= 12 && strncmp( q, "AimlCategory", 12 ) == 0 ) {
					return p;
				}
			}
		}
		++p;
	}
	return nullptr;
}

} // namespace

bool load_usda_file( Interpreter &interp, const char *path, std::string &err ) {
	std::string src = read_all( path, err );
	if ( src.empty() && !err.empty() ) {
		return false;
	}
	trim_inplace( src );
	if ( src.size() >= 3 && static_cast<unsigned char>( src[0] ) == 0xEF && static_cast<unsigned char>( src[1] ) == 0xBB
		&& static_cast<unsigned char>( src[2] ) == 0xBF ) {
		src.erase( 0, 3 );
	}

	const char *end = src.c_str() + src.size();
	const char *scan = src.c_str();
	int order = 0;

	while ( scan < end ) {
		const char *defp = find_def_aiml_category( scan, end );
		if ( !defp ) {
			break;
		}
		const char *q = defp + 3;
		skip_ws( q );
		q += 12; // AimlCategory
		skip_ws( q );

		Category cat;
		cat.topic = "*";

		if ( *q == '"' ) {
			if ( !parse_quoted_string( q, cat.id, err ) ) {
				return false;
			}
		} else {
			while ( q < end && !std::isspace( ( unsigned char )*q ) && *q != '(' ) {
				cat.id.push_back( *q++ );
			}
		}
		skip_ws( q );
		if ( *q == '(' ) {
			if ( !skip_balanced( q, '(', ')', err ) ) {
				return false;
			}
		}
		skip_ws( q );
		if ( *q != '{' ) {
			err = "AimlCategory missing body '{'";
			return false;
		}
		const char *body = q + 1;
		int depth = 1;
		const char *cur = body;
		while ( cur < end && depth > 0 ) {
			if ( *cur == '"' ) {
				++cur;
				while ( cur < end && *cur != '"' ) {
					if ( *cur == '\\' && cur + 1 < end ) {
						cur += 2;
					} else {
						++cur;
					}
				}
				if ( cur < end ) {
					++cur;
				}
				continue;
			}
			if ( *cur == '{' ) {
				++depth;
			} else if ( *cur == '}' ) {
				--depth;
			}
			++cur;
		}
		if ( depth != 0 ) {
			err = "unterminated AimlCategory body";
			return false;
		}
		const char *bodyEnd = cur - 1;
		if ( !parse_category_block( body, bodyEnd, cat, err ) ) {
			return false;
		}
		if ( cat.pattern.empty() || !cat.tmpl ) {
			err = "AimlCategory missing pattern or template: " + cat.id;
			return false;
		}
		cat.fileOrder = order++;
		interp.add_category( std::move( cat ) );
		scan = cur;
	}

	if ( order == 0 ) {
		err = "no def AimlCategory found in USDA";
		return false;
	}
	return true;
}

} // namespace aiml3
