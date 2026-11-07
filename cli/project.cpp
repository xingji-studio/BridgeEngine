#include "cli.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace bcli
{

namespace
{

std::string trim(const std::string &value)
{
	size_t start = 0;
	size_t end	 = value.size();
	while (start < end && (value[start] == ' ' || value[start] == '\t' || value[start] == '\r'))
		start++;
	while (end > start &&
		   (value[end - 1] == ' ' || value[end - 1] == '\t' || value[end - 1] == '\r'))
		end--;
	return value.substr(start, end - start);
}

bool read_text_file(const fs::path &path, std::string &out, std::string &error)
{
	std::ifstream file(path, std::ios::binary);
	if (!file) {
		error = "cannot open " + path.string();
		return false;
	}
	std::ostringstream buffer;
	buffer << file.rdbuf();
	if (!file.good() && !file.eof()) {
		error = "cannot read " + path.string();
		return false;
	}
	out = buffer.str();
	return true;
}

bool write_text_file(const fs::path &path, const std::string &text, std::string &error)
{
	const fs::path temporary = path.string() + ".tmp";
	{
		std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
		if (!file) {
			error = "cannot write " + temporary.string();
			return false;
		}
		file.write(text.data(), static_cast<std::streamsize>(text.size()));
		if (!file) {
			error = "cannot write " + temporary.string();
			return false;
		}
	}
	std::error_code ec;
	fs::remove(path, ec);
	fs::rename(temporary, path, ec);
	if (ec) {
		error = "cannot replace " + path.string() + ": " + ec.message();
		fs::remove(temporary, ec);
		return false;
	}
	return true;
}

} // namespace

bool valid_project_name(const std::string &name)
{
	if (name.empty()) return false;
	if (std::isdigit(static_cast<unsigned char>(name[0]))) return false;
	for (char c : name) {
		if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')) return false;
	}
	return true;
}

bool load_project_file(const fs::path &bep_path, Project &project, std::string &error)
{
	std::string text;
	if (!read_text_file(bep_path, text, error)) return false;

	project			 = Project{};
	project.root	 = bep_path.parent_path();
	project.bep_path = bep_path;

	std::istringstream stream(text);
	std::string		   line;
	bool			   in_documents = false;
	while (std::getline(stream, line)) {
		line = trim(line);
		if (line.empty() || line[0] == '#' || line[0] == ';') continue;
		if (line.front() == '[' && line.back() == ']') {
			in_documents = (line == "[documents]");
			continue;
		}
		if (in_documents) {
			project.documents.push_back(line);
			continue;
		}
		size_t equals = line.find('=');
		if (equals == std::string::npos) continue;
		std::string key	  = trim(line.substr(0, equals));
		std::string value = trim(line.substr(equals + 1));
		if (key == "name") project.name = value;
		if (key == "engine") project.engine = value;
	}
	if (project.name.empty()) project.name = project.root.filename().string();
	return true;
}

bool find_project(const fs::path &start, Project &project, std::string &error)
{
	std::error_code ec;
	fs::path		dir = fs::absolute(start, ec);
	if (ec) dir = start;
	if (fs::is_regular_file(dir, ec)) dir = dir.parent_path();

	while (!dir.empty()) {
		std::vector<fs::path> candidates;
		for (const auto &entry : fs::directory_iterator(dir, ec)) {
			if (ec) break;
			if (entry.is_regular_file() && entry.path().extension() == ".bep")
				candidates.push_back(entry.path());
		}
		if (!candidates.empty()) {
			std::sort(candidates.begin(), candidates.end());
			const fs::path preferred = dir / (dir.filename().string() + ".bep");
			fs::path	   chosen	 = candidates.front();
			if (fs::exists(preferred)) chosen = preferred;
			return load_project_file(chosen, project, error);
		}
		const fs::path parent = dir.parent_path();
		if (parent == dir || parent.empty()) break;
		dir = parent;
	}
	error = "no .bep project file found (run bcli new or bcli init first)";
	return false;
}

bool save_project(Project &project, std::string &error)
{
	if (!valid_project_name(project.name)) {
		error = "invalid project name: " + project.name;
		return false;
	}
	std::ostringstream out;
	out << "# BridgeEngine Project\n";
	out << "name = " << project.name << "\n";
	out << "engine = " << project.engine << "\n\n";
	out << "[documents]\n";
	for (const auto &document : project.documents) out << document << "\n";
	return write_text_file(project.bep_path, out.str(), error);
}

bool normalize_document_path(const std::string &input, std::string &normalized, std::string &error)
{
	std::string value = trim(input);
	if (value.empty()) {
		error = "document path is empty";
		return false;
	}
	std::replace(value.begin(), value.end(), '\\', '/');
	while (value.rfind("./", 0) == 0) value.erase(0, 2);
	if (value.empty()) {
		error = "document path is empty";
		return false;
	}
	fs::path path(value);
	if (path.is_absolute()) {
		error = "document path must be relative to the project: " + value;
		return false;
	}
	fs::path	clean	   = path.lexically_normal();
	std::string clean_text = clean.generic_string();
	if (clean_text == ".." || clean_text.rfind("../", 0) == 0) {
		error = "document path escapes the project: " + value;
		return false;
	}
	normalized = clean_text;
	return true;
}

bool add_document(Project &project, const std::string &path, const std::string &,
				  std::string &error)
{
	std::string normalized;
	if (!normalize_document_path(path, normalized, error)) return false;
	if (std::find(project.documents.begin(), project.documents.end(), normalized) !=
		project.documents.end()) {
		error = "document is already listed: " + normalized;
		return false;
	}
	project.documents.push_back(normalized);
	return save_project(project, error);
}

bool remove_document(Project &project, const std::string &path, std::string &error)
{
	std::string normalized;
	if (!normalize_document_path(path, normalized, error)) return false;
	auto it = std::find(project.documents.begin(), project.documents.end(), normalized);
	if (it == project.documents.end()) {
		error = "document is not listed: " + normalized;
		return false;
	}
	project.documents.erase(it);
	return save_project(project, error);
}

bool set_engine(Project &project, const fs::path &engine, std::string &error)
{
	std::error_code ec;
	fs::path		absolute = fs::absolute(engine, ec);
	if (ec) absolute = engine;
	project.engine = absolute.lexically_normal().generic_string();
	return save_project(project, error);
}

fs::path default_engine_dir()
{
	if (const char *env = std::getenv("BRIDGEENGINE_SOURCE_DIR")) {
		if (env[0] && fs::is_directory(env)) return fs::path(env);
	}
#ifdef BCLI_ENGINE_DIR
	if (BCLI_ENGINE_DIR[0] && fs::is_directory(BCLI_ENGINE_DIR)) return fs::path(BCLI_ENGINE_DIR);
#endif
	return {};
}

} // namespace bcli
