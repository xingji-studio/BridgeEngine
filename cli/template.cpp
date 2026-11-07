#include "cli.h"

#include <cstdint>

#include <cstdlib>
#include <fstream>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#include <set>
#include <sstream>

namespace bcli
{

namespace
{

const std::set<std::string> &text_suffixes()
{
	static const std::set<std::string> suffixes = {
		".c",	".h",	".cpp", ".hpp", ".cc",	 ".hh",	 ".cmake",
		".bep", ".xml", ".txt", ".md",	".json", ".ini",
	};
	return suffixes;
}

bool read_file(const fs::path &path, std::string &out)
{
	std::ifstream file(path, std::ios::binary);
	if (!file) return false;
	std::ostringstream buffer;
	buffer << file.rdbuf();
	out = buffer.str();
	return true;
}

bool write_file(const fs::path &path, const std::string &text)
{
	std::ofstream file(path, std::ios::binary | std::ios::trunc);
	if (!file) return false;
	file.write(text.data(), static_cast<std::streamsize>(text.size()));
	return static_cast<bool>(file);
}

bool is_text_file(const fs::path &path)
{
	if (path.filename() == "CMakeLists.txt") return true;
	return text_suffixes().count(path.extension().string()) != 0;
}

fs::path self_executable()
{
#ifdef _WIN32
	return fs::path(_pgmptr);
#elif defined(__APPLE__)
	char	 buffer[4096];
	uint32_t size = sizeof(buffer);
	if (_NSGetExecutablePath(buffer, &size) == 0) return fs::path(buffer);
	return {};
#else
	std::error_code ec;
	return fs::read_symlink("/proc/self/exe", ec);
#endif
}

} // namespace

std::string replace_tokens(std::string text, const std::string &project_name,
						   const std::string &engine_dir)
{
	const auto replace_all = [](std::string &value, const std::string &from,
								const std::string &to) {
		if (from.empty()) return;
		size_t position = 0;
		while ((position = value.find(from, position)) != std::string::npos) {
			value.replace(position, from.size(), to);
			position += to.size();
		}
	};
	replace_all(text, "__PROJECT_NAME__", project_name);
	replace_all(text, "__EXEC_NAME__", project_name);
	replace_all(text, "__ENGINE_DIR__", engine_dir);
	return text;
}

fs::path template_dir()
{
	if (const char *env = std::getenv("BRIDGEENGINE_TEMPLATE_DIR")) {
		if (env[0] && fs::is_directory(env)) return fs::path(env);
	}
#ifdef BCLI_TEMPLATE_DIR
	if (BCLI_TEMPLATE_DIR[0] && fs::is_directory(BCLI_TEMPLATE_DIR))
		return fs::path(BCLI_TEMPLATE_DIR);
#endif
	// Installed layout: <prefix>/bin/bcli and <prefix>/share/bridgeengine/templates/project.
	const fs::path relative = fs::path("..") / "share" / "bridgeengine" / "templates" / "project";
	const fs::path self		= self_executable();
	if (!self.empty()) {
		fs::path candidate = self.parent_path() / relative;
		if (fs::is_directory(candidate)) return candidate.lexically_normal();
	}
	return fs::path("templates") / "project";
}

bool create_project(const fs::path &root, const std::string &name, const fs::path &engine,
					bool init_mode, std::string &error)
{
	if (!valid_project_name(name)) {
		error = "project name must be a C identifier (letters, digits, underscore, not starting "
				"with a digit)";
		return false;
	}
	const fs::path tpl = template_dir();
	if (!fs::is_directory(tpl)) {
		error = "project template not found: " + tpl.string();
		return false;
	}

	std::error_code ec;
	if (!init_mode) {
		if (fs::exists(root, ec)) {
			error = "directory already exists: " + root.string();
			return false;
		}
		if (!fs::create_directories(root, ec) || ec) {
			error = "cannot create " + root.string() + ": " + ec.message();
			return false;
		}
	} else {
		if (!fs::is_directory(root, ec)) {
			error = "not a directory: " + root.string();
			return false;
		}
		for (const char *reserved : {"CMakeLists.txt", "main.c", "project.bep"}) {
			if (fs::exists(root / reserved, ec)) {
				error = "current directory already contains " + std::string(reserved) +
						"; use bcli new instead";
				return false;
			}
		}
	}

	const std::string engine_text =
		engine.empty() ? std::string() : engine.lexically_normal().generic_string();

	for (const auto &entry : fs::recursive_directory_iterator(tpl, ec)) {
		if (ec) break;
		const fs::path relative = fs::relative(entry.path(), tpl, ec);
		if (ec) break;
		fs::path destination = root / relative;
		if (relative.filename() == "project.bep") destination = root / (name + ".bep");

		if (entry.is_directory()) {
			fs::create_directories(destination, ec);
			if (ec) {
				error = "cannot create " + destination.string() + ": " + ec.message();
				return false;
			}
			continue;
		}
		fs::create_directories(destination.parent_path(), ec);
		if (ec) {
			error = "cannot create " + destination.parent_path().string() + ": " + ec.message();
			return false;
		}
		if (is_text_file(entry.path())) {
			std::string text;
			if (!read_file(entry.path(), text) ||
				!write_file(destination, replace_tokens(text, name, engine_text))) {
				error = "cannot render " + destination.string();
				return false;
			}
		} else {
			fs::copy_file(entry.path(), destination, fs::copy_options::overwrite_existing, ec);
			if (ec) {
				error = "cannot copy " + entry.path().string() + ": " + ec.message();
				return false;
			}
		}
	}
	if (ec) {
		error = "cannot read template: " + ec.message();
		return false;
	}
	return true;
}

} // namespace bcli
