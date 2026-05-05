#include "aiml3_internal.h"

#include <cctype>
#include <fstream>
#include <sstream>

namespace aiml3 {

namespace {

std::string read_all( const char *path, std::string &err ) {
	std::ifstream in( path, std::ios::binary );
	if ( !in ) {
		err = "cannot open file";
		return "";
	}
	std::ostringstream ss;
	ss << in.rdbuf();
	return ss.str();
}

void strip_xml_decl_and_comments( std::string &s ) {
	size_t i = 0;
	while ( i < s.size() ) {
		size_t st = s.find( "<!--", i );
		if ( st == std::string::npos ) {
			break;
		}
		size_t en = s.find( "-->", st + 4 );
		if ( en == std::string::npos ) {
			s.erase( st );
			break;
		}
		s.erase( st, en + 3 - st );
		i = st;
	}
	if ( s.size() >= 5 && s.compare( 0, 5, "<?xml" ) == 0 ) {
		size_t e = s.find( "?>" );
		if ( e != std::string::npos ) {
			s.erase( 0, e + 2 );
		}
	}
}

size_t skip_ws( const std::string &s, size_t i ) {
	while ( i < s.size() && std::isspace( ( unsigned char )s[i] ) ) {
		++i;
	}
	return i;
}

static std::string to_lower( std::string t ) {
	for ( char &c : t ) {
		c = ( char )std::tolower( ( unsigned char )c );
	}
	return t;
}

static std::string trim( std::string t ) {
	while ( !t.empty() && std::isspace( ( unsigned char )t.front() ) ) {
		t.erase( 0, 1 );
	}
	while ( !t.empty() && std::isspace( ( unsigned char )t.back() ) ) {
		t.pop_back();
	}
	for ( char &c : t ) {
		if ( c == '\n' || c == '\r' || c == '\t' ) {
			c = ' ';
		}
	}
	return t;
}

// Find "<tag" or "<tag>" or "<tag " at position >= from (case-insensitive tag)
static size_t find_open( const std::string &s, size_t from, const std::string &tag ) {
	const std::string low = to_lower( tag );
	for ( size_t i = from; i < s.size(); ++i ) {
		if ( s[i] != '<' ) {
			continue;
		}
		size_t j = i + 1;
		j = skip_ws( s, j );
		if ( j < s.size() && s[j] == '/' ) {
			continue;
		}
		size_t k = j;
		while ( k < s.size() && !std::isspace( ( unsigned char )s[k] ) && s[k] != '>' && s[k] != '/' ) {
			++k;
		}
		std::string name = to_lower( s.substr( j, k - j ) );
		if ( name == low ) {
			return i;
		}
	}
	return std::string::npos;
}

// After open tag of `tag`, find matching </tag> (no nesting of same tag assumed for pattern/that/template)
static bool extract_simple_block( const std::string &s, size_t open_lt, const std::string &tag, std::string &body, size_t &end_after_close, std::string &err ) {
	size_t gt = s.find( '>', open_lt );
	if ( gt == std::string::npos ) {
		err = "unclosed open " + tag;
		return false;
	}
	const std::string close = "</" + to_lower( tag ) + ">";
	size_t closePos = s.find( close, gt + 1 );
	if ( closePos == std::string::npos ) {
		err = "missing " + close;
		return false;
	}
	body = s.substr( gt + 1, closePos - ( gt + 1 ) );
	end_after_close = closePos + close.size();
	return true;
}

} // namespace

std::shared_ptr<TmplNode> parse_template_xmlish( const std::string &src, std::string &err ) {
	auto root = std::make_shared<TmplNode>();
	root->kind = TmplNode::kElement;
	root->tag = "template";

	size_t i = 0;
	auto emit_text = [&]( const std::string &t ) {
		if ( t.empty() ) {
			return;
		}
		auto n = std::make_shared<TmplNode>();
		n->kind = TmplNode::kText;
		n->text = t;
		root->children.push_back( n );
	};

	while ( i < src.size() ) {
		size_t lt = src.find( '<', i );
		if ( lt == std::string::npos ) {
			emit_text( src.substr( i ) );
			break;
		}
		if ( lt > i ) {
			emit_text( src.substr( i, lt - i ) );
		}
		if ( src.compare( lt, 9, "<![CDATA[" ) == 0 ) {
			size_t end = src.find( "]]>", lt + 9 );
			if ( end == std::string::npos ) {
				err = "CDATA";
				return nullptr;
			}
			emit_text( src.substr( lt + 9, end - ( lt + 9 ) ) );
			i = end + 3;
			continue;
		}
		if ( src.compare( lt, 4, "<!--" ) == 0 ) {
			size_t end = src.find( "-->", lt + 4 );
			if ( end == std::string::npos ) {
				err = "comment";
				return nullptr;
			}
			i = end + 3;
			continue;
		}
		size_t p = lt + 1;
		p = skip_ws( src, p );
		if ( p < src.size() && src[p] == '/' ) {
			err = "unexpected close in template";
			return nullptr;
		}
		size_t nameStart = p;
		while ( p < src.size() && !std::isspace( ( unsigned char )src[p] ) && src[p] != '>' && src[p] != '/' ) {
			++p;
		}
		std::string tname = to_lower( src.substr( nameStart, p - nameStart ) );
		std::map<std::string, std::string> attrs;
		bool self_close = false;
		for ( ;; ) {
			p = skip_ws( src, p );
			if ( p >= src.size() ) {
				err = "eof tag";
				return nullptr;
			}
			if ( src[p] == '>' ) {
				++p;
				break;
			}
			if ( src[p] == '/' && p + 1 < src.size() && src[p + 1] == '>' ) {
				self_close = true;
				p += 2;
				break;
			}
			size_t an = p;
			while ( p < src.size() && !std::isspace( ( unsigned char )src[p] ) && src[p] != '=' ) {
				++p;
			}
			std::string aname = to_lower( src.substr( an, p - an ) );
			p = skip_ws( src, p );
			if ( p >= src.size() || src[p] != '=' ) {
				err = "attr";
				return nullptr;
			}
			++p;
			p = skip_ws( src, p );
			if ( p >= src.size() || ( src[p] != '"' && src[p] != '\'' ) ) {
				err = "quote";
				return nullptr;
			}
			char q = src[p++];
			size_t avs = p;
			while ( p < src.size() && src[p] != q ) {
				++p;
			}
			if ( p >= src.size() ) {
				err = "attr end";
				return nullptr;
			}
			attrs[aname] = src.substr( avs, p - avs );
			++p;
		}
		if ( self_close ) {
			auto el = std::make_shared<TmplNode>();
			el->kind = TmplNode::kElement;
			el->tag = tname;
			el->attrs = std::move( attrs );
			root->children.push_back( el );
			i = p;
			continue;
		}
		const std::string close = "</" + tname + ">";
		int depth = 1;
		size_t innerStart = p;
		size_t scan = p;
		while ( depth > 0 && scan < src.size() ) {
			size_t nx = src.find( '<', scan );
			if ( nx == std::string::npos ) {
				err = "unclosed " + tname;
				return nullptr;
			}
			if ( src.compare( nx, close.size(), close ) == 0 ) {
				depth--;
				if ( depth == 0 ) {
					std::string inner = src.substr( innerStart, nx - innerStart );
					auto el = std::make_shared<TmplNode>();
					el->kind = TmplNode::kElement;
					el->tag = tname;
					el->attrs = std::move( attrs );
					std::string ierr;
					auto sub = parse_template_xmlish( inner, ierr );
					if ( !sub ) {
						err = ierr;
						return nullptr;
					}
					el->children = std::move( sub->children );
					root->children.push_back( el );
					i = nx + close.size();
					break;
				}
				scan = nx + close.size();
				continue;
			}
			if ( nx + 1 < src.size() && src[nx + 1] != '/' ) {
				size_t j = nx + 1;
				j = skip_ws( src, j );
				size_t t0 = j;
				while ( j < src.size() && !std::isspace( ( unsigned char )src[j] ) && src[j] != '>' && src[j] != '/' ) {
					++j;
				}
				std::string tn = to_lower( src.substr( t0, j - t0 ) );
				if ( tn == tname ) {
					while ( j < src.size() && src[j] != '>' ) {
						++j;
					}
					if ( j >= 2 && src[j - 1] == '/' ) {
						scan = j + 1;
						continue;
					}
					depth++;
				}
			}
			scan = nx + 1;
		}
		if ( depth > 0 ) {
			return nullptr;
		}
	}
	return root;
}

static bool parse_category_inner( const std::string &inner, const std::string &topic, int fileOrder, Interpreter &interp, std::string &err ) {
	std::string pat, th, tmplSrc;
	size_t ppos = find_open( inner, 0, "pattern" );
	if ( ppos == std::string::npos ) {
		err = "category missing pattern";
		return false;
	}
	size_t after = 0;
	if ( !extract_simple_block( inner, ppos, "pattern", pat, after, err ) ) {
		return false;
	}
	size_t pos = after;
	const size_t mpos = find_open( inner, pos, "template" );
	if ( mpos == std::string::npos ) {
		err = "category missing template";
		return false;
	}
	const size_t tpos = find_open( inner, pos, "that" );
	if ( tpos != std::string::npos && tpos < mpos ) {
		if ( !extract_simple_block( inner, tpos, "that", th, after, err ) ) {
			return false;
		}
		pos = after;
	}
	if ( !extract_simple_block( inner, mpos, "template", tmplSrc, after, err ) ) {
		return false;
	}
	std::string terr;
	auto tmpl = parse_template_xmlish( tmplSrc, terr );
	if ( !tmpl ) {
		err = terr.empty() ? "template parse" : terr;
		return false;
	}
	Category c;
	c.fileOrder = fileOrder;
	c.pattern = trim( pat );
	c.that = trim( th );
	c.topic = topic;
	c.tmpl = std::move( tmpl );
	interp.add_category( std::move( c ) );
	return true;
}

bool load_xml_file( Interpreter &interp, const char *path, std::string &err ) {
	std::string s = read_all( path, err );
	if ( s.empty() && !err.empty() ) {
		return false;
	}
	strip_xml_decl_and_comments( s );

	size_t aimlOpen = find_open( s, 0, "aiml" );
	if ( aimlOpen == std::string::npos ) {
		err = "missing <aiml>";
		return false;
	}
	size_t aiml_gt = s.find( '>', aimlOpen );
	if ( aiml_gt == std::string::npos ) {
		err = "bad <aiml>";
		return false;
	}
	const std::string aimlClose = "</aiml>";
	size_t aimlEnd = s.rfind( aimlClose );
	if ( aimlEnd == std::string::npos || aimlEnd <= aiml_gt ) {
		err = "missing </aiml>";
		return false;
	}
	const std::string body = s.substr( aiml_gt + 1, aimlEnd - ( aiml_gt + 1 ) );

	std::string topic = "*";
	int ord = 0;
	size_t pos = 0;
	while ( pos < body.size() ) {
		pos = skip_ws( body, pos );
		if ( pos >= body.size() ) {
			break;
		}
		size_t top = find_open( body, pos, "topic" );
		size_t cat = find_open( body, pos, "category" );
		if ( top != std::string::npos && ( cat == std::string::npos || top < cat ) ) {
			// parse topic name from open tag (crude: find name="...")
			size_t gt = body.find( '>', top );
			if ( gt == std::string::npos ) {
				err = "topic >";
				return false;
			}
			std::string open = body.substr( top, gt - top + 1 );
			topic = "*";
			size_t npos = open.find( "name=\"" );
			if ( npos != std::string::npos ) {
				npos += 6;
				size_t nend = open.find( '"', npos );
				if ( nend != std::string::npos ) {
					topic = open.substr( npos, nend - npos );
				}
			}
			const std::string tclose = "</topic>";
			size_t tend = body.find( tclose, gt + 1 );
			if ( tend == std::string::npos ) {
				err = "missing </topic>";
				return false;
			}
			std::string tinner = body.substr( gt + 1, tend - ( gt + 1 ) );
			size_t ip = 0;
			while ( ip < tinner.size() ) {
				ip = skip_ws( tinner, ip );
				size_t c0 = find_open( tinner, ip, "category" );
				if ( c0 == std::string::npos ) {
					break;
				}
				size_t cgt = tinner.find( '>', c0 );
				if ( cgt == std::string::npos ) {
					err = "category >";
					return false;
				}
				const std::string cclose = "</category>";
				size_t cend = tinner.find( cclose, cgt + 1 );
				if ( cend == std::string::npos ) {
					err = "missing </category> in topic";
					return false;
				}
				std::string cinner = tinner.substr( cgt + 1, cend - ( cgt + 1 ) );
				if ( !parse_category_inner( cinner, topic, ord++, interp, err ) ) {
					return false;
				}
				ip = cend + cclose.size();
			}
			pos = tend + tclose.size();
			topic = "*";
			continue;
		}
		if ( cat != std::string::npos ) {
			size_t cgt = body.find( '>', cat );
			if ( cgt == std::string::npos ) {
				err = "category >";
				return false;
			}
			const std::string cclose = "</category>";
			size_t cend = body.find( cclose, cgt + 1 );
			if ( cend == std::string::npos ) {
				err = "missing </category>";
				return false;
			}
			std::string cinner = body.substr( cgt + 1, cend - ( cgt + 1 ) );
			if ( !parse_category_inner( cinner, topic, ord++, interp, err ) ) {
				return false;
			}
			pos = cend + cclose.size();
			continue;
		}
		break;
	}
	return true;
}

} // namespace aiml3
