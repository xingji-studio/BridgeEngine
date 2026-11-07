#include "cli.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>

namespace bcli
{

namespace
{

bool read_text(const fs::path &path, std::string &out)
{
	std::ifstream file(path, std::ios::binary);
	if (!file) return false;
	std::ostringstream buffer;
	buffer << file.rdbuf();
	out = buffer.str();
	return true;
}

bool is_name_start(char c)
{
	const unsigned char value = static_cast<unsigned char>(c);
	return std::isalpha(value) || c == '_' || c == ':';
}

bool is_name_char(char c)
{
	const unsigned char value = static_cast<unsigned char>(c);
	return std::isalnum(value) || c == '_' || c == ':' || c == '.' || c == '-';
}

std::string path_text(const fs::path &path)
{
	return path.lexically_normal().generic_string();
}

bool is_xml_file(const fs::path &path)
{
	std::string extension = path.extension().string();
	std::transform(extension.begin(), extension.end(), extension.begin(),
				   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return extension == ".xml";
}

struct Attribute {
	std::string name;
	std::string value;
	int			line = 1;
};

struct OpenTag {
	std::string name;
	int			line = 1;
};

class XmlScanner
{
  public:
	XmlScanner(const std::string &text, const fs::path &document, std::vector<std::string> &issues)
		: text_(text), document_(document), issues_(issues)
	{
	}

	void run()
	{
		while (index_ < text_.size()) {
			if (text_[index_] != '<') {
				advance();
				continue;
			}
			if (starts_with("<!--")) {
				skip_comment();
				continue;
			}
			if (starts_with("<![CDATA[")) {
				skip_cdata();
				continue;
			}
			if (starts_with("<!")) {
				skip_declaration();
				continue;
			}
			if (starts_with("<?")) {
				skip_processing_instruction();
				continue;
			}
			if (starts_with("</")) {
				parse_close_tag();
				continue;
			}
			parse_open_tag();
		}

		for (const auto &tag : stack_) {
			error(tag.line, "unclosed tag <" + tag.name + ">");
		}
		if (root_count_ == 0) error(1, "no root element");
	}

  private:
	bool starts_with(const char *needle) const
	{
		const std::string value(needle);
		return text_.compare(index_, value.size(), value) == 0;
	}

	void advance(size_t count = 1)
	{
		for (size_t i = 0; i < count && index_ < text_.size(); i++) {
			if (text_[index_] == '\n') line_++;
			index_++;
		}
	}

	void error(int line, const std::string &message)
	{
		issues_.push_back(path_text(document_) + ":" + std::to_string(line) + ": " + message);
	}

	void skip_comment()
	{
		const int line = line_;
		advance(4);
		if (!skip_until("-->")) error(line, "unterminated comment");
	}

	void skip_cdata()
	{
		const int line = line_;
		advance(9);
		if (!skip_until("]]>")) error(line, "unterminated CDATA section");
	}

	void skip_declaration()
	{
		const int line = line_;
		advance(2);
		if (!skip_to_tag_end()) error(line, "unterminated declaration");
	}

	void skip_processing_instruction()
	{
		const int line = line_;
		advance(2);
		if (!skip_until("?>")) error(line, "unterminated processing instruction");
	}

	bool skip_until(const std::string &needle)
	{
		while (index_ < text_.size()) {
			if (text_.compare(index_, needle.size(), needle) == 0) {
				advance(needle.size());
				return true;
			}
			advance();
		}
		return false;
	}

	/* Skip to the next '>' while respecting quoted attribute values. */
	bool skip_to_tag_end()
	{
		char quote = 0;
		while (index_ < text_.size()) {
			const char c = text_[index_];
			if (quote != 0) {
				if (c == quote) quote = 0;
			} else if (c == '"' || c == '\'') {
				quote = c;
			} else if (c == '>') {
				advance();
				return true;
			}
			advance();
		}
		return false;
	}

	void skip_space()
	{
		while (index_ < text_.size()) {
			const char c = text_[index_];
			if (c != ' ' && c != '\t' && c != '\r' && c != '\n') return;
			advance();
		}
	}

	bool read_name(std::string &out)
	{
		if (index_ >= text_.size() || !is_name_start(text_[index_])) return false;
		while (index_ < text_.size() && is_name_char(text_[index_])) {
			out += text_[index_];
			advance();
		}
		return true;
	}

	void parse_open_tag()
	{
		const int tag_line = line_;
		advance(); // '<'

		std::string name;
		if (!read_name(name)) {
			error(tag_line, "invalid tag name");
			recover_tag();
			return;
		}

		std::vector<Attribute> attributes;
		bool				   self_closing = false;
		while (true) {
			skip_space();
			if (index_ >= text_.size()) {
				error(tag_line, "unterminated tag <" + name + ">");
				return;
			}
			const char c = text_[index_];
			if (c == '>') {
				advance();
				break;
			}
			if (c == '/') {
				advance();
				skip_space();
				if (index_ < text_.size() && text_[index_] == '>') {
					advance();
					self_closing = true;
					break;
				}
				error(tag_line, "malformed self-closing tag <" + name + ">");
				recover_tag();
				return;
			}

			Attribute attribute;
			attribute.line = line_;
			if (!read_name(attribute.name)) {
				error(attribute.line, "malformed attribute in <" + name + ">");
				recover_tag();
				return;
			}
			skip_space();
			if (index_ >= text_.size() || text_[index_] != '=') {
				error(attribute.line, "expected '=' after attribute '" + attribute.name + "'");
				recover_tag();
				return;
			}
			advance();
			skip_space();
			if (index_ >= text_.size() || (text_[index_] != '"' && text_[index_] != '\'')) {
				error(attribute.line, "unterminated attribute quote for '" + attribute.name + "'");
				recover_tag();
				return;
			}
			const char quote = text_[index_];
			advance();
			bool closed = false;
			while (index_ < text_.size()) {
				if (text_[index_] == quote) {
					advance();
					closed = true;
					break;
				}
				attribute.value += text_[index_];
				advance();
			}
			if (!closed) {
				error(attribute.line, "unterminated attribute quote for '" + attribute.name + "'");
				recover_tag();
				return;
			}
			attributes.push_back(std::move(attribute));
		}

		if (stack_.empty()) root_count_++;
		if (root_count_ > 1) error(tag_line, "multiple root elements");

		for (const auto &attribute : attributes) {
			if (attribute.name == "id") {
				auto found = ids_.find(attribute.value);
				if (found != ids_.end()) {
					error(attribute.line, "duplicate id '" + attribute.value + "' (first at line " +
											  std::to_string(found->second) + ")");
				} else {
					ids_[attribute.value] = attribute.line;
				}
			} else if (attribute.name == "src") {
				check_source(attribute);
			}
		}

		if (!self_closing) stack_.push_back({name, tag_line});
	}

	void check_source(const Attribute &attribute)
	{
		if (attribute.value.empty()) {
			error(attribute.line, "empty src attribute");
			return;
		}
		if (attribute.value.find("://") != std::string::npos ||
			attribute.value.rfind("data:", 0) == 0) {
			return;
		}
		std::string value = attribute.value;
		std::replace(value.begin(), value.end(), '\\', '/');
		const fs::path	source(value);
		const fs::path	resolved = source.is_absolute() ? source : document_.parent_path() / source;
		std::error_code ec;
		if (!fs::exists(resolved, ec))
			error(attribute.line,
				  "missing resource '" + attribute.value + "' (" + path_text(resolved) + ")");
	}

	void parse_close_tag()
	{
		const int line = line_;
		advance(2);

		std::string name;
		if (!read_name(name)) {
			error(line, "invalid closing tag");
			recover_tag();
			return;
		}
		skip_space();
		if (index_ >= text_.size() || text_[index_] != '>') {
			error(line, "malformed closing tag </" + name + ">");
			recover_tag();
			return;
		}
		advance();

		if (stack_.empty()) {
			error(line, "unexpected closing tag </" + name + ">");
			return;
		}
		if (stack_.back().name == name) {
			stack_.pop_back();
			return;
		}

		error(line,
			  "mismatched closing tag </" + name + ">, expected </" + stack_.back().name + ">");
		for (size_t i = stack_.size(); i > 0; i--) {
			if (stack_[i - 1].name == name) {
				stack_.resize(i - 1);
				return;
			}
		}
	}

	void recover_tag()
	{
		skip_to_tag_end();
	}

	const std::string		  &text_;
	const fs::path			   document_;
	std::vector<std::string>  &issues_;
	size_t					   index_	   = 0;
	int						   line_	   = 1;
	int						   root_count_ = 0;
	std::vector<OpenTag>	   stack_;
	std::map<std::string, int> ids_;
};

std::vector<fs::path> collect_xml_files(const fs::path &directory)
{
	std::vector<fs::path> files;
	std::error_code		  ec;
	for (fs::recursive_directory_iterator
			 it(directory, fs::directory_options::skip_permission_denied, ec),
		 end;
		 it != end; it.increment(ec)) {
		if (ec) break;
		const fs::path &path = it->path();
		if (!it->is_regular_file(ec)) continue;
		if (is_xml_file(path)) files.push_back(path);
	}
	std::sort(files.begin(), files.end());
	return files;
}

} // namespace

bool validate_ui_text(const std::string &text, const fs::path &document,
					  std::vector<std::string> &issues)
{
	issues.clear();
	XmlScanner scanner(text, document, issues);
	scanner.run();
	return issues.empty();
}

int cmd_ui(const std::vector<std::string> &args)
{
	if (args.empty() || args.front() != "validate") {
		std::cerr << "bcli: usage: bcli ui validate [FILE|DIR]\n";
		return 1;
	}

	fs::path explicit_path;
	bool	 has_path = false;
	for (size_t i = 1; i < args.size(); i++) {
		if (args[i].rfind("--", 0) == 0) {
			std::cerr << "bcli: unknown option: " << args[i] << "\n";
			return 1;
		}
		if (has_path) {
			std::cerr << "bcli: unexpected argument: " << args[i] << "\n";
			return 1;
		}
		explicit_path = args[i];
		has_path	  = true;
	}

	std::vector<fs::path>	 files;
	std::vector<std::string> issues;

	if (has_path) {
		std::error_code ec;
		if (fs::is_directory(explicit_path, ec)) {
			files = collect_xml_files(explicit_path);
			if (files.empty()) {
				std::cout << "ui validate: no XML files under " << explicit_path.string() << "\n";
				return 0;
			}
		} else if (fs::is_regular_file(explicit_path, ec)) {
			files.push_back(explicit_path);
		} else {
			std::cerr << "bcli: not a file or directory: " << explicit_path.string() << "\n";
			return 1;
		}
	} else {
		Project		project;
		std::string error;
		if (!find_project(fs::current_path(), project, error)) {
			std::cerr << "bcli: " << error << "\n";
			return 1;
		}
		for (const auto &document : project.documents) {
			const fs::path	path = project.root / document;
			std::error_code ec;
			if (!fs::is_regular_file(path, ec)) {
				issues.push_back(path_text(path) + ": document listed in " +
								 path_text(project.bep_path) + " does not exist");
				continue;
			}
			files.push_back(path);
		}
		if (files.empty()) {
			std::cout << "ui validate: ok (0 documents)\n";
			return 0;
		}
	}

	for (const auto &file : files) {
		std::string text;
		if (!read_text(file, text)) {
			issues.push_back(path_text(file) + ": cannot read file");
			continue;
		}
		std::vector<std::string> file_issues;
		validate_ui_text(text, file, file_issues);
		issues.insert(issues.end(), file_issues.begin(), file_issues.end());
	}

	for (const auto &issue : issues) std::cout << issue << "\n";
	if (issues.empty()) {
		std::cout << "ui validate: ok (" << files.size() << " file"
				  << (files.size() == 1 ? "" : "s") << ")\n";
		return 0;
	}
	std::cout << "ui validate: " << issues.size() << " error" << (issues.size() == 1 ? "" : "s")
			  << " in " << files.size() << " file" << (files.size() == 1 ? "" : "s") << "\n";
	return 1;
}

} // namespace bcli
