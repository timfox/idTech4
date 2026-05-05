#include "aiml3_internal.h"

#include <cctype>
#include <fstream>
#include <sstream>

namespace aiml3 {

std::shared_ptr<TmplNode> template_from_plain_string( const std::string &s ) {
	auto root = std::make_shared<TmplNode>();
	root->kind = TmplNode::kElement;
	root->tag = "template";
	auto text = std::make_shared<TmplNode>();
	text->kind = TmplNode::kText;
	text->text = s;
	root->children.push_back( text );
	return root;
}

namespace {

struct Lex {
	const char *p = nullptr;
	std::string err;

	void ws() {
		while ( p && *p && std::isspace( ( unsigned char )*p ) ) {
			++p;
		}
	}

	bool ch( char c ) {
		ws();
		if ( p && *p == c ) {
			++p;
			return true;
		}
		return false;
	}

	bool str( std::string &out ) {
		ws();
		if ( !p || *p != '"' ) {
			err = "expected \"";
			return false;
		}
		++p;
		out.clear();
		while ( p && *p ) {
			if ( *p == '"' ) {
				++p;
				return true;
			}
			if ( *p == '\\' ) {
				++p;
				if ( !p || !*p ) {
					break;
				}
				if ( *p == 'n' ) {
					out.push_back( '\n' );
				} else if ( *p == 't' ) {
					out.push_back( '\t' );
				} else if ( *p == 'r' ) {
					out.push_back( '\r' );
				} else if ( *p == '"' || *p == '\\' || *p == '/' ) {
					out.push_back( *p );
				} else if ( *p == 'u' ) {
					++p;
					unsigned cp = 0;
					for ( int i = 0; i < 4 && p && *p; ++i, ++p ) {
						int v = -1;
						if ( *p >= '0' && *p <= '9' ) {
							v = *p - '0';
						} else if ( *p >= 'a' && *p <= 'f' ) {
							v = 10 + *p - 'a';
						} else if ( *p >= 'A' && *p <= 'F' ) {
							v = 10 + *p - 'A';
						}
						if ( v < 0 ) {
							err = "bad \\u";
							return false;
						}
						cp = ( cp << 4 ) | ( unsigned )v;
					}
					if ( cp < 0x80 ) {
						out.push_back( ( char )cp );
					} else if ( cp < 0x800 ) {
						out.push_back( ( char )( 0xC0 | ( cp >> 6 ) ) );
						out.push_back( ( char )( 0x80 | ( cp & 0x3F ) ) );
					} else {
						out.push_back( ( char )( 0xE0 | ( cp >> 12 ) ) );
						out.push_back( ( char )( 0x80 | ( ( cp >> 6 ) & 0x3F ) ) );
						out.push_back( ( char )( 0x80 | ( cp & 0x3F ) ) );
					}
					continue;
				} else {
					out.push_back( *p );
				}
				++p;
				continue;
			}
			out.push_back( *p++ );
		}
		err = "unterminated string";
		return false;
	}

	bool skip_value();
};

bool Lex::skip_value() {
	ws();
	if ( !p || !*p ) {
		return false;
	}
	if ( *p == '"' ) {
		std::string tmp;
		return str( tmp );
	}
	if ( *p == '{' ) {
		++p;
		for ( ;; ) {
			ws();
			if ( ch( '}' ) ) {
				return true;
			}
			std::string k;
			if ( !str( k ) ) {
				return false;
			}
			if ( !ch( ':' ) ) {
				return false;
			}
			if ( !skip_value() ) {
				return false;
			}
			ws();
			if ( ch( ',' ) ) {
				continue;
			}
			if ( ch( '}' ) ) {
				return true;
			}
			err = "object syntax";
			return false;
		}
	}
	if ( *p == '[' ) {
		++p;
		for ( ;; ) {
			ws();
			if ( ch( ']' ) ) {
				return true;
			}
			if ( !skip_value() ) {
				return false;
			}
			ws();
			if ( ch( ',' ) ) {
				continue;
			}
			if ( ch( ']' ) ) {
				return true;
			}
			err = "array syntax";
			return false;
		}
	}
	while ( p && *p && *p != ',' && *p != '}' && *p != ']' ) {
		++p;
	}
	return true;
}

static bool parse_template_field( Lex &L, std::shared_ptr<TmplNode> &tmpl, std::string &err ) {
	L.ws();
	if ( L.p && *L.p == '"' ) {
		std::string v;
		if ( !L.str( v ) ) {
			err = L.err;
			return false;
		}
		if ( v.find( '<' ) != std::string::npos ) {
			std::string perr;
			tmpl = parse_template_xmlish( v, perr );
			if ( !tmpl ) {
				err = perr.empty() ? "template xmlish" : perr;
				return false;
			}
		} else {
			tmpl = template_from_plain_string( v );
		}
		return true;
	}
	if ( !L.ch( '{' ) ) {
		err = "template must be string or object";
		return false;
	}
	std::string say;
	bool got = false;
	for ( ;; ) {
		L.ws();
		if ( L.ch( '}' ) ) {
			break;
		}
		std::string k;
		if ( !L.str( k ) ) {
			err = L.err;
			return false;
		}
		if ( !L.ch( ':' ) ) {
			err = "expected :";
			return false;
		}
		if ( k == "say" ) {
			if ( !L.str( say ) ) {
				err = L.err;
				return false;
			}
			got = true;
		} else {
			if ( !L.skip_value() ) {
				err = L.err;
				return false;
			}
		}
		L.ws();
		if ( L.ch( ',' ) ) {
			continue;
		}
		if ( L.ch( '}' ) ) {
			break;
		}
		err = "template object";
		return false;
	}
	if ( !got ) {
		err = "template object needs say";
		return false;
	}
	if ( say.find( '<' ) != std::string::npos ) {
		std::string perr;
		tmpl = parse_template_xmlish( say, perr );
		if ( !tmpl ) {
			err = perr.empty() ? "template xmlish" : perr;
			return false;
		}
	} else {
		tmpl = template_from_plain_string( say );
	}
	return true;
}

static bool parse_category( Lex &L, Category &cat, std::string &err ) {
	if ( !L.ch( '{' ) ) {
		err = "category {";
		return false;
	}
	cat.pattern.clear();
	cat.that.clear();
	cat.topic.clear();
	cat.id.clear();
	cat.cooldown.clear();
	cat.tmpl.reset();
	for ( ;; ) {
		L.ws();
		if ( L.ch( '}' ) ) {
			break;
		}
		std::string key;
		if ( !L.str( key ) ) {
			err = L.err;
			return false;
		}
		if ( !L.ch( ':' ) ) {
			err = "expected :";
			return false;
		}
		if ( key == "pattern" || key == "that" || key == "topic" || key == "id" || key == "cooldown" ) {
			std::string v;
			if ( !L.str( v ) ) {
				err = L.err;
				return false;
			}
			if ( key == "pattern" ) {
				cat.pattern = v;
			} else if ( key == "that" ) {
				cat.that = v;
			} else if ( key == "topic" ) {
				cat.topic = v;
			} else if ( key == "id" ) {
				cat.id = v;
			} else if ( key == "cooldown" ) {
				cat.cooldown = v;
			}
		} else if ( key == "template" ) {
			if ( !parse_template_field( L, cat.tmpl, err ) ) {
				return false;
			}
		} else {
			if ( !L.skip_value() ) {
				err = L.err;
				return false;
			}
		}
		L.ws();
		if ( L.ch( ',' ) ) {
			continue;
		}
		if ( L.ch( '}' ) ) {
			break;
		}
		err = "category syntax";
		return false;
	}
	if ( cat.pattern.empty() || !cat.tmpl ) {
		err = "category needs pattern and template";
		return false;
	}
	return true;
}

static bool parse_categories_array( Lex &L, Interpreter &interp, int baseOrder, std::string &err ) {
	if ( !L.ch( '[' ) ) {
		err = "expected [";
		return false;
	}
	int ord = baseOrder;
	for ( ;; ) {
		L.ws();
		if ( L.ch( ']' ) ) {
			return true;
		}
		Category cat;
		cat.fileOrder = ord++;
		if ( !parse_category( L, cat, err ) ) {
			return false;
		}
		interp.add_category( std::move( cat ) );
		L.ws();
		if ( L.ch( ',' ) ) {
			continue;
		}
		if ( L.ch( ']' ) ) {
			return true;
		}
		err = "categories ]";
		return false;
	}
}

static bool parse_aiml_inner_categories( Lex &L, Interpreter &interp, int &ord, std::string &err ) {
	if ( !L.ch( '{' ) ) {
		err = "aiml {";
		return false;
	}
	for ( ;; ) {
		L.ws();
		if ( L.ch( '}' ) ) {
			err = "aiml object has no categories";
			return false;
		}
		std::string k2;
		if ( !L.str( k2 ) ) {
			err = L.err;
			return false;
		}
		if ( !L.ch( ':' ) ) {
			err = "aiml :";
			return false;
		}
		if ( k2 == "categories" ) {
			if ( !parse_categories_array( L, interp, ord, err ) ) {
				return false;
			}
			L.ws();
			while ( L.ch( ',' ) ) {
				L.ws();
				if ( L.ch( '}' ) ) {
					return true;
				}
				std::string skipKey;
				if ( !L.str( skipKey ) ) {
					err = L.err;
					return false;
				}
				if ( !L.ch( ':' ) ) {
					err = "aiml inner :";
					return false;
				}
				if ( !L.skip_value() ) {
					err = L.err;
					return false;
				}
			}
			if ( !L.ch( '}' ) ) {
				err = "aiml unclosed";
				return false;
			}
			return true;
		}
		if ( !L.skip_value() ) {
			err = L.err;
			return false;
		}
		L.ws();
		if ( L.ch( ',' ) ) {
			continue;
		}
		if ( L.ch( '}' ) ) {
			err = "aiml missing categories";
			return false;
		}
		err = "aiml syntax";
		return false;
	}
}

static bool parse_root_object( Lex &L, Interpreter &interp, std::string &err ) {
	if ( !L.ch( '{' ) ) {
		err = "root {";
		return false;
	}
	int ord = 0;
	bool got = false;
	for ( ;; ) {
		L.ws();
		if ( L.ch( '}' ) ) {
			break;
		}
		std::string key;
		if ( !L.str( key ) ) {
			err = L.err;
			return false;
		}
		if ( !L.ch( ':' ) ) {
			err = "root :";
			return false;
		}
		if ( key == "categories" ) {
			if ( !parse_categories_array( L, interp, ord, err ) ) {
				return false;
			}
			got = true;
		} else if ( key == "aiml" ) {
			if ( !parse_aiml_inner_categories( L, interp, ord, err ) ) {
				return false;
			}
			got = true;
		} else {
			if ( !L.skip_value() ) {
				err = L.err;
				return false;
			}
		}
		L.ws();
		if ( L.ch( ',' ) ) {
			continue;
		}
		if ( L.ch( '}' ) ) {
			break;
		}
		err = "root syntax";
		return false;
	}
	if ( !got ) {
		err = "root missing categories or aiml";
		return false;
	}
	return true;
}

} // namespace

bool load_json_file( Interpreter &interp, const char *path, std::string &err ) {
	std::ifstream in( path, std::ios::binary );
	if ( !in ) {
		err = "cannot open file";
		return false;
	}
	std::ostringstream ss;
	ss << in.rdbuf();
	std::string buf = ss.str();

	Lex L;
	L.p = buf.c_str();
	L.ws();
	if ( !L.p || !*L.p ) {
		err = "empty file";
		return false;
	}
	if ( *L.p == '[' ) {
		if ( !parse_categories_array( L, interp, 0, err ) ) {
			return false;
		}
	} else if ( *L.p == '{' ) {
		if ( !parse_root_object( L, interp, err ) ) {
			return false;
		}
	} else {
		err = "JSON must start with { or [";
		return false;
	}
	L.ws();
	while ( L.p && *L.p && std::isspace( ( unsigned char )*L.p ) ) {
		++L.p;
	}
	if ( L.p && *L.p ) {
		err = "trailing data after JSON";
		return false;
	}
	return true;
}

} // namespace aiml3
