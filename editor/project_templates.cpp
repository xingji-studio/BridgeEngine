#include "project_templates.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

// Compiled in by CMake: absolute path to the engine source root.
#ifndef BRIDGEENGINE_SOURCE_DIR
#define BRIDGEENGINE_SOURCE_DIR ""
#endif

namespace {

#ifdef _WIN32

const char kSep = '\\';

std::string native_seps(std::string s)
{
	for (char &c : s)
		if (c == '/') c = '\\';
	return s;
}

std::string template_dir()
{
	std::string base = native_seps(BRIDGEENGINE_SOURCE_DIR);
	if (!base.empty() && base.back() != '\\') base += '\\';
	return base + "templates\\project";
}

bool path_exists(const std::string &path)
{
	return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool make_dirs(const std::string &dir)
{
	std::vector<std::string> parts;
	std::string cur;
	for (char c : dir) {
		if (c == '\\' || c == '/') {
			parts.push_back(cur);
			cur.clear();
		} else {
			cur += c;
		}
	}
	if (!cur.empty()) parts.push_back(cur);

	std::string path;
	size_t first = 0;
	if (dir.size() >= 2 && dir[1] == ':') {
		path = dir.substr(0, 2); // drive prefix "C:"
		first = 1; // parts[0] is the drive, already consumed
	}
	for (size_t i = first; i < parts.size(); i++) {
		const std::string &part = parts[i];
		if (part.empty()) continue;
		if (!path.empty() && path.back() != '\\') path += '\\';
		path += part;
		if (GetFileAttributesA(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
			if (!CreateDirectoryA(path.c_str(), NULL)) return false;
		}
	}
	return true;
}

bool copy_file(const std::string &from, const std::string &to)
{
	return CopyFileA(from.c_str(), to.c_str(), FALSE) != 0;
}

struct DirEntry {
	std::string name;
	bool is_directory;
};

bool list_directory(const std::string &dir, std::vector<DirEntry> &out)
{
	WIN32_FIND_DATAA fd;
	std::string pattern = dir + "\\*";
	HANDLE h			   = FindFirstFileA(pattern.c_str(), &fd);
	if (h == INVALID_HANDLE_VALUE) return false;
	do {
		if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
		DirEntry e;
		e.name		   = fd.cFileName;
		e.is_directory = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
		out.push_back(e);
	} while (FindNextFileA(h, &fd) != 0);
	FindClose(h);
	return true;
}

#else // POSIX

const char kSep = '/';

std::string template_dir()
{
	std::string base = BRIDGEENGINE_SOURCE_DIR;
	if (!base.empty() && base.back() != '/') base += '/';
	return base + "templates/project";
}

bool path_exists(const std::string &path)
{
	struct stat st;
	return ::stat(path.c_str(), &st) == 0;
}

bool make_dirs(const std::string &dir)
{
	std::string path;
	size_t i = 0;
	if (!dir.empty() && dir[0] == '/') {
		path = "/";
		i = 1;
	}
	while (i < dir.size()) {
		size_t j = dir.find('/', i);
		if (j == std::string::npos) j = dir.size();
		std::string part = dir.substr(i, j - i);
		i = j + 1;
		if (part.empty()) continue;
		if (!path.empty() && path.back() != '/') path += '/';
		path += part;
		if (::mkdir(path.c_str(), 0755) != 0 && errno != EEXIST) return false;
	}
	return true;
}

bool copy_file(const std::string &from, const std::string &to)
{
	FILE *in = std::fopen(from.c_str(), "rb");
	if (!in) return false;
	FILE *out = std::fopen(to.c_str(), "wb");
	if (!out) {
		std::fclose(in);
		return false;
	}
	char buffer[65536];
	size_t n;
	bool ok = true;
	while ((n = std::fread(buffer, 1, sizeof(buffer), in)) > 0) {
		if (std::fwrite(buffer, 1, n, out) != n) {
			ok = false;
			break;
		}
	}
	if (ok && std::ferror(in)) ok = false;
	std::fclose(in);
	if (std::fclose(out) != 0) ok = false;
	return ok;
}

struct DirEntry {
	std::string name;
	bool is_directory;
};

bool list_directory(const std::string &dir, std::vector<DirEntry> &out)
{
	DIR *d = ::opendir(dir.c_str());
	if (!d) return false;
	struct dirent *ent;
	while ((ent = ::readdir(d)) != NULL) {
		if (std::strcmp(ent->d_name, ".") == 0 || std::strcmp(ent->d_name, "..") == 0) continue;
		DirEntry e;
		e.name		   = ent->d_name;
		e.is_directory = false;
#ifdef DT_DIR
		if (ent->d_type == DT_DIR) {
			e.is_directory = true;
		} else if (ent->d_type == DT_UNKNOWN)
#endif
		{
			struct stat st;
			if (::stat((dir + "/" + e.name).c_str(), &st) == 0) e.is_directory = S_ISDIR(st.st_mode);
		}
		out.push_back(e);
	}
	::closedir(d);
	return true;
}

#endif // _WIN32

std::string join_path(const std::string &dir, const std::string &name)
{
	if (!dir.empty() && dir.back() != kSep) return dir + kSep + name;
	return dir + name;
}

std::string replace_all(std::string s, const std::string &from, const std::string &to)
{
	if (from.empty()) return s;
	size_t pos = 0;
	while ((pos = s.find(from, pos)) != std::string::npos) {
		s.replace(pos, from.size(), to);
		pos += to.size();
	}
	return s;
}

std::string forward_slashes(std::string s)
{
	for (char &c : s)
		if (c == '\\') c = '/';
	return s;
}

bool valid_project_name(const std::string &name)
{
	if (name.empty()) return false;
	if (std::isdigit((unsigned char)name[0])) return false;
	for (char c : name) {
		if (!(std::isalnum((unsigned char)c) || c == '_')) return false;
	}
	return true;
}

bool read_file(const std::string &path, std::string &out)
{
	FILE *f = std::fopen(path.c_str(), "rb");
	if (!f) return false;
	std::fseek(f, 0, SEEK_END);
	long size = std::ftell(f);
	std::fseek(f, 0, SEEK_SET);
	out.resize(size > 0 ? (size_t)size : 0);
	if (size > 0 && std::fread(&out[0], 1, (size_t)size, f) != (size_t)size) {
		std::fclose(f);
		return false;
	}
	std::fclose(f);
	return true;
}

bool write_file(const std::string &path, const std::string &data)
{
	FILE *f = std::fopen(path.c_str(), "wb");
	if (!f) return false;
	bool ok = std::fwrite(data.data(), 1, data.size(), f) == data.size();
	std::fclose(f);
	return ok;
}

// Files whose contents get placeholder substitution after copying. Binary
// assets (e.g. the font) must never be touched. Paths are relative to the
// template root and always use forward slashes.
const std::set<std::string> &text_files()
{
	static const std::set<std::string> files = {
		"CMakeLists.txt",
		"main.c",
		"project.bep",
		"ui/menu.xml",
		"ui/game.xml",
		"ui/settings.xml",
	};
	return files;
}

struct Placeholders {
	std::string project_name;
	std::string engine_dir; // forward-slashed; empty means find_package
};

bool copy_tree(const std::string &from_dir, const std::string &to_dir, const std::string &rel_prefix,
			   const Placeholders &ph)
{
	if (!make_dirs(to_dir)) return false;

	std::vector<DirEntry> entries;
	if (!list_directory(from_dir, entries)) return false;

	for (const DirEntry &e : entries) {
		std::string from = join_path(from_dir, e.name);
		std::string to	 = join_path(to_dir, e.name);
		if (e.name == "project.bep") to = join_path(to_dir, ph.project_name + ".bep");

		if (e.is_directory) {
			if (!copy_tree(from, to, rel_prefix + e.name + "/", ph)) return false;
			continue;
		}

		if (!copy_file(from, to)) return false;

		// Substitute placeholders in text files, identified by their path
		// relative to the template root.
		std::string rel = rel_prefix + e.name;
		if (text_files().count(rel) == 0) continue;

		std::string content;
		if (!read_file(to, content)) return false;
		content = replace_all(content, "__PROJECT_NAME__", ph.project_name);
		content = replace_all(content, "__EXEC_NAME__", ph.project_name);
		content = replace_all(content, "__ENGINE_DIR__", ph.engine_dir);
		if (!write_file(to, content)) return false;
	}
	return true;
}

} // namespace

bool EditorCreateProject(const std::string &project_name, const std::string &parent_dir,
						 const std::string &engine_dir, std::string &error_out,
						 std::string *out_bep_path)
{
	if (!valid_project_name(project_name)) {
		error_out =
			"Project name must be a C identifier (letters, digits, underscore, not starting with a digit).";
		return false;
	}
	if (parent_dir.empty()) {
		error_out = "Choose a parent directory for the project.";
		return false;
	}

	std::string root = parent_dir;
	if (root.back() != '\\' && root.back() != '/') root += kSep;
	root += project_name;

	if (path_exists(root)) {
		error_out = "Directory already exists: " + root;
		return false;
	}

	std::string tpl = template_dir();
	if (!path_exists(tpl)) {
		error_out = "Project template not found: " + tpl;
		return false;
	}

	Placeholders ph;
	ph.project_name = project_name;
	ph.engine_dir	= engine_dir.empty() ? "" : forward_slashes(engine_dir);

	if (!copy_tree(tpl, root, "", ph)) {
		error_out = "Failed to copy the project template.";
		return false;
	}
	if (out_bep_path) *out_bep_path = root + kSep + project_name + ".bep";
	return true;
}
