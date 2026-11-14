#include "cli.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>

namespace bcli
{

namespace
{

constexpr int kAssetManifestVersion = 1;

std::string trim(const std::string &value)
{
	size_t start = 0;
	size_t end	 = value.size();
	while (start < end && std::isspace(static_cast<unsigned char>(value[start]))) start++;
	while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) end--;
	return value.substr(start, end - start);
}

bool read_text(const fs::path &path, std::string &out)
{
	std::ifstream file(path, std::ios::binary);
	if (!file) return false;
	std::ostringstream buffer;
	buffer << file.rdbuf();
	out = buffer.str();
	return true;
}

bool is_under(const fs::path &path, const fs::path &directory)
{
	const fs::path relative = path.lexically_relative(directory);
	if (relative.empty()) return false;
	auto first = relative.begin();
	if (first == relative.end()) return false;
	return *first != "..";
}

void add_entry(std::set<std::string> &entries, const fs::path &root, const fs::path &path)
{
	if (!is_under(path, root)) return;
	const fs::path relative = path.lexically_relative(root);
	if (relative.empty() || relative == ".") return;
	auto first = relative.begin();
	if (first != relative.end() && *first == "..") return;
	entries.insert(relative.generic_string());
}

void add_reference(std::set<std::string> &entries, const fs::path &root, const fs::path &base,
				   const std::string &raw)
{
	std::string value = trim(raw);
	if (value.empty()) return;
	if (value.find("://") != std::string::npos || value.rfind("data:", 0) == 0) return;
	std::replace(value.begin(), value.end(), '\\', '/');
	fs::path path(value);
	fs::path resolved =
		path.is_absolute() ? path.lexically_normal() : (base / path).lexically_normal();
	add_entry(entries, root, resolved);
}

bool is_extension(const fs::path &path, const char *extension)
{
	std::string value = path.extension().string();
	std::transform(value.begin(), value.end(), value.begin(),
				   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return value == extension;
}

void scan_map_file(const fs::path &path, const fs::path &root, std::set<std::string> &entries)
{
	std::string text;
	if (!read_text(path, text)) return;
	const std::string key = is_extension(path, ".btm") ? "tileset" : "sprite";

	std::istringstream lines(text);
	std::string		   line;
	while (std::getline(lines, line)) {
		const std::string trimmed = trim(line);
		if (trimmed.empty() || trimmed[0] == '#') continue;
		std::istringstream tokens(trimmed);
		std::string		   directive;
		std::string		   reference;
		tokens >> directive;
		if (directive != key) continue;
		tokens >> reference;
		if (!reference.empty()) add_reference(entries, root, root, reference);
	}
}

bool is_name_char(char c)
{
	const unsigned char value = static_cast<unsigned char>(c);
	return std::isalnum(value) || c == '_' || c == ':' || c == '.' || c == '-';
}

void scan_xml_file(const fs::path &path, const fs::path &root, std::set<std::string> &entries)
{
	std::string text;
	if (!read_text(path, text)) return;

	size_t index = 0;
	while ((index = text.find("src", index)) != std::string::npos) {
		const bool boundary = index == 0 || !is_name_char(text[index - 1]);
		if (!boundary) {
			index += 3;
			continue;
		}
		size_t cursor = index + 3;
		while (cursor < text.size() && std::isspace(static_cast<unsigned char>(text[cursor])))
			cursor++;
		if (cursor >= text.size() || text[cursor] != '=') {
			index += 3;
			continue;
		}
		cursor++;
		while (cursor < text.size() && std::isspace(static_cast<unsigned char>(text[cursor])))
			cursor++;
		if (cursor >= text.size() || (text[cursor] != '"' && text[cursor] != '\'')) {
			index += 3;
			continue;
		}
		const char	 quote = text[cursor++];
		const size_t end   = text.find(quote, cursor);
		if (end == std::string::npos) break;
		add_reference(entries, root, path.parent_path(), text.substr(cursor, end - cursor));
		index = end + 1;
	}
}

void walk_project(const fs::path &root, std::set<std::string> &entries,
				  std::vector<fs::path> &xml_files, std::vector<fs::path> &map_files,
				  std::vector<fs::path> &world_files)
{
	const fs::path	assets = root / "assets";
	const fs::path	ui	   = root / "ui";
	std::error_code ec;
	for (fs::recursive_directory_iterator
			 it(root, fs::directory_options::skip_permission_denied, ec),
		 end;
		 it != end; it.increment(ec)) {
		if (ec) break;
		if (it->is_directory(ec)) {
			const std::string name = it->path().filename().string();
			if (name == "build" || name == ".git" || name == ".bridgeengine-deps")
				it.disable_recursion_pending();
			continue;
		}
		if (!it->is_regular_file(ec)) continue;

		const fs::path &path = it->path();
		if (is_under(path, assets)) add_entry(entries, root, path);
		if (is_extension(path, ".btm"))
			map_files.push_back(path);
		else if (is_extension(path, ".bworld"))
			world_files.push_back(path);
		else if (is_extension(path, ".xml") && (is_under(path, ui) || is_under(path, assets)))
			xml_files.push_back(path);
	}
}

bool load_project_from_root(const fs::path &root, Project &project)
{
	std::error_code ec;
	std::string		error;
	const fs::path	preferred = root / (root.filename().string() + ".bep");
	if (fs::is_regular_file(preferred, ec)) return load_project_file(preferred, project, error);
	// Fall back to the first .bep directly inside the root.
	std::vector<fs::path> candidates;
	for (fs::directory_iterator it(root, ec), end; it != end; it.increment(ec)) {
		if (ec) break;
		if (it->is_regular_file(ec) && it->path().extension() == ".bep")
			candidates.push_back(it->path());
	}
	if (candidates.empty()) return false;
	std::sort(candidates.begin(), candidates.end());
	return load_project_file(candidates.front(), project, error);
}

bool resolve_root(const fs::path &explicit_root, bool has_root, Project &project, fs::path &root,
				  std::string &error)
{
	if (has_root) {
		std::error_code ec;
		root = fs::absolute(explicit_root, ec).lexically_normal();
		if (ec) {
			error = "cannot resolve --root: " + ec.message();
			return false;
		}
		if (!fs::is_directory(root, ec)) {
			error = "root is not a directory: " + root.string();
			return false;
		}
		project		 = Project{};
		project.root = root;
		load_project_from_root(root, project);
		return true;
	}

	if (!find_project(fs::current_path(), project, error)) return false;
	root = project.root;
	return true;
}

bool load_manifest(const fs::path &path, std::vector<std::string> &entries, std::string &error)
{
	std::string text;
	if (!read_text(path, text)) {
		error = "cannot read manifest: " + path.string();
		return false;
	}

	std::istringstream lines(text);
	std::string		   line;
	bool			   header_seen = false;
	int				   line_number = 0;
	while (std::getline(lines, line)) {
		line_number++;
		std::string	 trimmed = trim(line);
		const size_t comment = trimmed.find('#');
		if (comment != std::string::npos) trimmed = trim(trimmed.substr(0, comment));
		if (trimmed.empty()) continue;

		if (!header_seen) {
			std::istringstream tokens(trimmed);
			std::string		   magic;
			std::string		   version_text;
			tokens >> magic >> version_text;
			if (magic != "BASSETS") {
				error = path.string() + ":" + std::to_string(line_number) +
						": expected 'BASSETS <version>' header";
				return false;
			}
			char *end	  = nullptr;
			long  version = std::strtol(version_text.c_str(), &end, 10);
			if (end == version_text.c_str() || *end != '\0' || version < 1 ||
				version > kAssetManifestVersion) {
				error = path.string() + ":" + std::to_string(line_number) +
						": unsupported manifest version '" + version_text + "' (expected 1)";
				return false;
			}
			header_seen = true;
			continue;
		}

		std::istringstream tokens(trimmed);
		std::string		   key;
		tokens >> key;
		if (key != "asset") continue;
		const size_t	  key_end = trimmed.find_first_of(" \t");
		const std::string value =
			key_end == std::string::npos ? std::string() : trim(trimmed.substr(key_end + 1));
		if (value.empty()) {
			error = path.string() + ":" + std::to_string(line_number) + ": asset path is empty";
			return false;
		}
		entries.push_back(value);
	}
	if (!header_seen) {
		error = path.string() + ": missing BASSETS header";
		return false;
	}
	return true;
}

bool file_exists(const fs::path &path)
{
	FILE *file = std::fopen(path.string().c_str(), "rb");
	if (file == nullptr) return false;
	std::fclose(file);
	return true;
}

int usage()
{
	std::cerr << "bcli: usage: bcli assets scan [--root DIR] [--out FILE]\n";
	std::cerr << "       bcli assets verify [--root DIR] [--manifest FILE]\n";
	return 1;
}

int fail(const std::string &message)
{
	std::cerr << "bcli: " << message << "\n";
	return 1;
}

} // namespace

int cmd_assets(const std::vector<std::string> &args)
{
	if (args.empty()) return usage();
	const std::string action = args.front();

	if (action != "scan" && action != "verify") {
		std::cerr << "bcli: unknown assets command: " << action << "\n";
		return usage();
	}

	fs::path	explicit_root;
	bool		has_root = false;
	fs::path	output;
	std::string output_label = action == "scan" ? "--out" : "--manifest";
	bool		has_output	 = false;

	for (size_t i = 1; i < args.size(); i++) {
		if (args[i] == "--root") {
			if (i + 1 >= args.size()) return fail("missing value for --root");
			explicit_root = args[++i];
			has_root	  = true;
		} else if (args[i] == output_label) {
			if (i + 1 >= args.size()) return fail("missing value for " + output_label);
			output	   = args[++i];
			has_output = true;
		} else {
			return fail("unknown option: " + args[i]);
		}
	}

	Project		project;
	fs::path	root;
	std::string error;
	if (!resolve_root(explicit_root, has_root, project, root, error)) return fail(error);

	fs::path manifest = has_output ? output : root / "assets.manifest";
	if (!manifest.is_absolute()) manifest = root / manifest;
	manifest = manifest.lexically_normal();

	if (action == "scan") {
		std::set<std::string> entries;
		for (const auto &document : project.documents) add_reference(entries, root, root, document);

		std::vector<fs::path> xml_files;
		std::vector<fs::path> map_files;
		std::vector<fs::path> world_files;
		walk_project(root, entries, xml_files, map_files, world_files);

		for (const auto &path : map_files) scan_map_file(path, root, entries);
		for (const auto &path : world_files) scan_map_file(path, root, entries);
		for (const auto &path : xml_files) scan_xml_file(path, root, entries);

		std::error_code ec;
		if (!manifest.parent_path().empty()) fs::create_directories(manifest.parent_path(), ec);
		if (ec)
			return fail("cannot create " + manifest.parent_path().string() + ": " + ec.message());

		std::ofstream file(manifest, std::ios::binary | std::ios::trunc);
		if (!file) return fail("cannot write " + manifest.string());
		file << "BASSETS " << kAssetManifestVersion << "\n";
		for (const auto &entry : entries) file << "asset " << entry << "\n";
		if (!file) return fail("cannot write " + manifest.string());
		file.close();
		if (!file) return fail("cannot write " + manifest.string());

		std::cout << "bcli: wrote " << entries.size() << " asset entries to " << manifest.string()
				  << "\n";
		return 0;
	}

	if (!file_exists(manifest)) {
		std::cerr << "bcli: manifest not found: " << manifest.string() << "\n";
		std::cerr << "bcli: run `bcli assets scan` to create it\n";
		return 1;
	}

	std::vector<std::string> entries;
	if (!load_manifest(manifest, entries, error)) return fail(error);

	std::vector<std::string> missing;
	for (const auto &entry : entries) {
		const fs::path relative(entry);
		const fs::path path = relative.is_absolute() ? relative : root / relative;
		if (!file_exists(path)) missing.push_back(entry);
	}

	for (const auto &entry : missing) std::cout << "missing " << entry << "\n";
	if (!missing.empty()) {
		std::cout << "assets verify: " << missing.size() << " missing of " << entries.size()
				  << " entries (root " << root.string() << ")\n";
		return 1;
	}
	std::cout << "assets verify: ok (" << entries.size() << " entries, root " << root.string()
			  << ")\n";
	return 0;
}

} // namespace bcli
