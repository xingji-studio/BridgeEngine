#include "project_file.h"

#include "editor.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

static const char *kRecentProjectFile = "recent_projects.txt";
static const char *kSessionFile		   = "project_sessions.txt";

static void trim(std::string &value)
{
	const char *spaces = " \t";
	size_t first = value.find_first_not_of(spaces);
	if (first == std::string::npos) {
		value.clear();
		return;
	}
	size_t last = value.find_last_not_of(spaces);
	value = value.substr(first, last - first + 1);
}

static std::string parent_dir(const std::string &path)
{
	size_t slash = path.find_last_of("\\/");
	return slash == std::string::npos ? "." : path.substr(0, slash);
}

static std::string join_path(const std::string &base, const std::string &relative)
{
	// Forward slashes work for fopen on every platform, including Windows.
	if (relative.empty()) return base;
	if (base.empty()) return relative;
	char last = base.back();
	if (last == '\\' || last == '/') return base + relative;
	return base + "/" + relative;
}

struct ProjectConfig {
	std::string name;
	std::string engine;
	std::vector<std::string> documents;
};

static bool parse_project(const char *path, ProjectConfig &config, std::string &error)
{
	FILE *file = std::fopen(path, "rb");
	if (!file) {
		error = std::string("Cannot open project file: ") + path;
		return false;
	}

	char line[1024];
	bool in_documents = false;
	while (std::fgets(line, sizeof(line), file)) {
		std::string value(line);
		while (!value.empty() && (value.back() == '\n' || value.back() == '\r')) value.pop_back();
		trim(value);
		if (value.empty() || value[0] == '#' || value[0] == ';') continue;
		if (value.front() == '[' && value.back() == ']') {
			in_documents = value == "[documents]";
			continue;
		}
		if (in_documents) {
			config.documents.push_back(value);
			continue;
		}

		size_t equals = value.find('=');
		if (equals == std::string::npos) continue;
		std::string key = value.substr(0, equals);
		std::string data = value.substr(equals + 1);
		trim(key);
		trim(data);
		if (key == "name") config.name = data;
		else if (key == "engine") config.engine = data;
	}
	std::fclose(file);

	if (config.documents.empty()) {
		error = "Project file contains no documents.";
		return false;
	}
	return true;
}

} // namespace

void EditorPushRecentProject(EditorState &state, const char *path)
{
	if (!path || !path[0]) return;
	std::string value(path);
	auto it = std::find(state.recent_projects.begin(), state.recent_projects.end(), value);
	if (it != state.recent_projects.end()) state.recent_projects.erase(it);
	state.recent_projects.insert(state.recent_projects.begin(), value);
	if (state.recent_projects.size() > 8) state.recent_projects.resize(8);
	EditorSaveRecentProjects(state);
}

void EditorLoadRecentProjects(EditorState &state)
{
	FILE *file = std::fopen(kRecentProjectFile, "rb");
	if (!file) return;
	char line[1024];
	while (std::fgets(line, sizeof(line), file)) {
		std::string value(line);
		while (!value.empty() && (value.back() == '\n' || value.back() == '\r')) value.pop_back();
		trim(value);
		if (!value.empty()) state.recent_projects.push_back(value);
	}
	std::fclose(file);
	if (state.recent_projects.size() > 8) state.recent_projects.resize(8);
}

void EditorSaveRecentProjects(EditorState &state)
{
	FILE *file = std::fopen(kRecentProjectFile, "wb");
	if (!file) return;
	for (const std::string &path : state.recent_projects)
		std::fprintf(file, "%s\n", path.c_str());
	std::fclose(file);
}

// Convert an absolute document path into a path relative to the project root
// directory (the folder holding the .bep file). Falls back to the absolute
// path when the document is not under the project root. All comparisons and
// stored paths use forward slashes.
static std::string normalize_slashes(const std::string &value)
{
	std::string out = value;
	for (char &c : out)
		if (c == '\\') c = '/';
	return out;
}

static std::string relative_to_root(const std::string &root, const std::string &path)
{
	std::string norm_root = normalize_slashes(root);
	if (!norm_root.empty() && norm_root.back() != '/') norm_root += '/';
	std::string norm_path = normalize_slashes(path);
	if (norm_path.find(norm_root) == 0) return norm_path.substr(norm_root.size());
	return norm_path;
}

// Per-project sessions: one line per project, "project_path|doc1|doc2|...",
// documents relative to the project root. Written on save, read on open.

static std::string escape_session_entry(const std::string &value)
{
	std::string out = value;
	for (char &c : out)
		if (c == '|' || c == '\n' || c == '\r') c = '_';
	return out;
}

void EditorSaveProjectSession(EditorState &state)
{
	if (state.project_path.empty()) return;
	std::string root	 = parent_dir(state.project_path);
	FILE	   *file	 = std::fopen(kSessionFile, "rb");
	std::string contents;
	char		line[1024];
	bool		found	 = false;
	bool		replaced = false;
	if (file) {
		while (std::fgets(line, sizeof(line), file)) {
			std::string value(line);
			while (!value.empty() && (value.back() == '\n' || value.back() == '\r')) value.pop_back();
			if (!replaced && value.rfind(state.project_path + "|", 0) == 0) {
				// Rebuild this project's line from the current open documents.
				std::string rebuilt = escape_session_entry(state.project_path);
				for (int i = 0; i < (int)state.documents.size(); i++) {
					const std::string &doc_path = EditorDocumentPath(state, i);
					if (!doc_path.empty()) rebuilt += "|" + relative_to_root(root, doc_path);
				}
				contents += rebuilt;
				contents += "\n";
				replaced = true;
				found	 = true;
			} else {
				contents += value;
				contents += "\n";
			}
		}
		std::fclose(file);
	}
	if (!replaced) {
		std::string rebuilt = escape_session_entry(state.project_path);
		for (int i = 0; i < (int)state.documents.size(); i++) {
			const std::string &doc_path = EditorDocumentPath(state, i);
			if (!doc_path.empty()) rebuilt += "|" + relative_to_root(root, doc_path);
		}
		contents += rebuilt;
		contents += "\n";
	}

	file = std::fopen(kSessionFile, "wb");
	if (!file) return;
	std::fwrite(contents.data(), 1, contents.size(), file);
	std::fclose(file);
	(void)found;
}

std::vector<std::string> EditorLoadProjectSession(EditorState &state,
												  const std::string &project_path)
{
	std::vector<std::string> result;
	(void)state;
	if (project_path.empty()) return result;

	FILE *file = std::fopen(kSessionFile, "rb");
	if (!file) return result;
	char line[1024];
	while (std::fgets(line, sizeof(line), file)) {
		std::string value(line);
		while (!value.empty() && (value.back() == '\n' || value.back() == '\r')) value.pop_back();
		if (value.rfind(project_path + "|", 0) != 0) continue;
		std::string rest = value.substr(project_path.size() + 1);
		size_t		pos	 = 0;
		while (pos < rest.size()) {
			size_t next = rest.find('|', pos);
			if (next == std::string::npos) next = rest.size();
			std::string doc = rest.substr(pos, next - pos);
			if (!doc.empty()) result.push_back(doc);
			pos = next + 1;
		}
		break;
	}
	std::fclose(file);
	return result;
}

// Sync the UI encryption key into <project>/.uixkey so the project's build
// encrypts ui/*.xml -> *.uix automatically. An empty key removes the file
// (build then copies plain XML, matching the old behaviour). No-op when no
// project is open.
void EditorSyncUiKeyFile(EditorState &state)
{
	if (state.project_path.empty()) return;
	std::string root		 = parent_dir(state.project_path);
	std::string key_path	 = root + "/" + ".uixkey";
	if (state.ui_key[0]) {
		FILE *kf = std::fopen(key_path.c_str(), "wb");
		if (kf) {
			std::fprintf(kf, "%s", state.ui_key);
			std::fclose(kf);
		}
	} else {
		std::remove(key_path.c_str());
	}
}

bool EditorSaveProject(EditorState &state, std::string &error_out)
{
	if (state.project_path.empty()) {
		error_out = "No project is open.";
		return false;
	}
	std::string root = parent_dir(state.project_path);
	FILE *file		= std::fopen(state.project_path.c_str(), "wb");
	if (!file) {
		error_out = "Cannot write project file: " + state.project_path;
		return false;
	}

	ProjectConfig config;
	config.name = state.project_path;
	{
		size_t slash = config.name.find_last_of("\\/");
		if (slash != std::string::npos) config.name = config.name.substr(slash + 1);
		if (config.name.size() > 4 && config.name.compare(config.name.size() - 4, 4, ".bep") == 0)
			config.name.resize(config.name.size() - 4);
	}

	for (int i = 0; i < (int)state.documents.size(); i++) {
		const std::string &doc_path = EditorDocumentPath(state, i);
		if (!doc_path.empty()) config.documents.push_back(relative_to_root(root, doc_path));
	}
	if (config.documents.empty()) config.documents.push_back("ui/game.xml");

	std::fprintf(file, "; BridgeEngine project file - written by BridgeEngine Edit\n");
	std::fprintf(file, "name = %s\n", config.name.c_str());
	if (!state.project_engine.empty())
		std::fprintf(file, "engine = %s\n", state.project_engine.c_str());
	std::fprintf(file, "\n[documents]\n");
	for (const std::string &doc : config.documents) std::fprintf(file, "%s\n", doc.c_str());
	std::fclose(file);

	EditorSyncUiKeyFile(state);

	EditorSaveProjectSession(state);
	return true;
}

bool EditorOpenProject(EditorState &state, const char *path, std::string &error_out)
{
	if (!path || !path[0]) {
		error_out = "Project path is empty.";
		return false;
	}

	ProjectConfig config;
	if (!parse_project(path, config, error_out)) return false;
	for (int i = 0; i < (int)state.documents.size(); i++) {
		if (EditorDocumentIsDirty(state, i)) {
			error_out = "Save or discard changes in all open documents before opening a project.";
			return false;
		}
	}

	std::string root = parent_dir(path);
	// EditorCloseDocument deliberately creates an untitled replacement after the
	// final tab closes. Keep one document, replace it with the first project XML,
	// then discard that now-parked untitled document.
	while (state.documents.size() > 1) EditorCloseDocument(state, 0);

	// Reopen the documents from the last session (if any) so the working set is
	// restored, falling back to every document listed in the .bep.
	std::vector<std::string> session_docs = EditorLoadProjectSession(state, path);
	if (!session_docs.empty()) config.documents = session_docs;

	bool loaded_any = false;
	for (const std::string &relative : config.documents) {
		std::string document_path = join_path(root, relative);
		if (EditorLoadFile(state, document_path.c_str())) {
			if (!loaded_any && state.documents.size() > 1) EditorCloseDocument(state, 0);
			loaded_any = true;
		} else if (error_out.empty()) {
			error_out = "Failed to open project document: " + document_path;
		}
	}

	if (!loaded_any) {
		error_out = error_out.empty() ? "No project documents could be opened." : error_out;
		return false;
	}

	state.project_path = path;
	state.project_engine = config.engine;
	state.show_welcome = false;
	EditorPushRecentProject(state, path);
	EditorSaveProjectSession(state);

	// Restore the UI encryption key from <project>/.uixkey so the editor's
	// save/load and the build stay consistent with the stored key. An absent
	// file clears the key (project builds unencrypted).
	std::string key_path = root + "/" + ".uixkey";
	FILE	   *kf		 = std::fopen(key_path.c_str(), "rb");
	if (kf) {
		size_t n = std::fread(state.ui_key, 1, sizeof(state.ui_key) - 1, kf);
		state.ui_key[n] = '\0';
		std::fclose(kf);
	} else {
		state.ui_key[0] = '\0';
	}
	return true;
}
