#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace aiml3 {

struct TmplNode {
	enum Kind { kText, kElement };
	Kind kind = kText;
	std::string text; // for kText
	std::string tag;  // for kElement: lowercased local name
	std::map<std::string, std::string> attrs;
	std::vector<std::shared_ptr<TmplNode>> children;
};

struct Category {
	std::string id;
	std::string pattern;
	std::string that;
	std::string topic; // "*" = default catch-all
	std::string cooldown; // parsed but not enforced in reference tool
	int fileOrder = 0;
	std::shared_ptr<TmplNode> tmpl;

	int specificityScore() const;
	int topicScore() const;
	int thatScore() const;
};

struct InterpreterConfig {
	int srai_max_depth = 32;
	std::string default_topic = "*";
};

class Interpreter {
public:
	explicit Interpreter( InterpreterConfig cfg = {} );

	void clear_graph();
	void add_category( Category c );
	void set_bot( const std::string &key, const std::string &val );
	void set_map( const std::string &mapName, const std::string &key, const std::string &val );

	// Returns primary reply string; updates session state (that, predicates, topic if set inside template).
	std::string respond( const std::string &rawInput );

	const std::string &last_that() const { return that_; }
	const std::string &topic() const { return topic_; }

private:
	InterpreterConfig cfg_;
	std::vector<Category> cats_;
	std::map<std::string, std::string> bot_;
	std::map<std::string, std::map<std::string, std::string>> maps_;
	std::map<std::string, std::string> pred_;
	std::string that_;
	std::string topic_;

	std::vector<std::string> normalize_tokens( const std::string &s ) const;
	std::vector<std::string> pattern_tokens( const std::string &pat ) const;

	bool match_pattern( const std::vector<std::string> &patTok, const std::vector<std::string> &uttTok,
		std::vector<std::string> &stars ) const;

	// Returns false if no category matched.
	bool find_best_match( const std::vector<std::string> &uttTok, const std::vector<std::string> &thatTok,
		const Category **outCat, std::vector<std::string> *outStars ) const;

	std::string eval_template( const Category &cat, const std::vector<std::string> &stars, int srai_depth,
		std::vector<std::string> &srai_stack );

	std::string eval_node( const std::shared_ptr<TmplNode> &n, const Category &cat, const std::vector<std::string> &stars,
		int srai_depth, std::vector<std::string> &srai_stack );

	std::string eval_srai( const std::shared_ptr<TmplNode> &n, const Category &cat, const std::vector<std::string> &stars,
		int srai_depth, std::vector<std::string> &srai_stack );

	static int star_index( const std::map<std::string, std::string> &attrs, int defaultOneBased );
};

bool load_json_file( Interpreter &interp, const char *path, std::string &err );
bool load_xml_file( Interpreter &interp, const char *path, std::string &err );

std::shared_ptr<TmplNode> parse_template_xmlish( const std::string &src, std::string &err );
std::shared_ptr<TmplNode> template_from_plain_string( const std::string &s );

} // namespace aiml3
