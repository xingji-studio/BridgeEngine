#include "cli.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <sstream>

namespace bcli
{

namespace
{

std::string path_text(const fs::path &path)
{
	return path.lexically_normal().generic_string();
}

std::string local_pkgconfig_dir()
{
	const fs::path engine = default_engine_dir();
	if (engine.empty()) return {};
	const fs::path candidate = engine / ".bridgeengine-deps" / "lib" / "pkgconfig";
	return fs::is_directory(candidate) ? candidate.string() : std::string();
}

std::string json_escape(const std::string &value)
{
	std::string out;
	out.reserve(value.size() + 8);
	for (char c : value) {
		switch (c) {
		case '"':
			out += "\\\"";
			break;
		case '\\':
			out += "\\\\";
			break;
		case '\n':
			out += "\\n";
			break;
		case '\r':
			out += "\\r";
			break;
		case '\t':
			out += "\\t";
			break;
		default:
			out += c;
			break;
		}
	}
	return out;
}

bool command_available(const std::string &name, const fs::path &cwd)
{
	return run_process({name, "--version"}, cwd, true).exit_code == 0;
}

bool load_from_cwd(Project &project, std::string &error)
{
	return find_project(fs::current_path(), project, error);
}

int fail(const std::string &message)
{
	std::cerr << "bcli: " << message << "\n";
	return 1;
}

bool parse_option(const std::vector<std::string> &args, size_t &index, const std::string &name,
				  std::string &value)
{
	if (args[index] != name) return false;
	if (index + 1 >= args.size()) return false;
	value = args[++index];
	return true;
}

struct BuildOptions {
	std::string configuration = "Debug";
	bool		clean		  = false;
	bool		reconfigure	  = false;
};

BuildOptions parse_build_options(const std::vector<std::string> &args)
{
	BuildOptions options;
	for (const auto &arg : args) {
		if (arg == "--release" || arg == "-r")
			options.configuration = "Release";
		else if (arg == "--debug" || arg == "-d")
			options.configuration = "Debug";
		else if (arg == "--clean")
			options.clean = true;
		else if (arg == "--reconfigure")
			options.reconfigure = true;
	}
	return options;
}

} // namespace

int build_project(Project &project, const std::string &configuration, bool clean, bool reconfigure)
{
	if (project.engine.empty()) {
		const fs::path fallback = default_engine_dir();
		if (!fallback.empty()) project.engine = path_text(fallback);
	}
	if (project.engine.empty() || !fs::is_directory(project.engine)) {
		return fail("engine directory is missing; run `bcli set-engine <path>`");
	}
	const fs::path	build_dir = project.root / "build";
	std::error_code ec;

	const fs::path assets_manifest = project.root / "assets.manifest";
	if (fs::is_regular_file(assets_manifest, ec)) {
		std::cout << "bcli: verifying " << assets_manifest.string() << "\n";
		const int verification = cmd_assets({"verify", "--root", path_text(project.root),
											 "--manifest", path_text(assets_manifest)});
		if (verification != 0)
			return fail(
				"asset manifest verification failed; run `bcli assets scan` to refresh it");
	}

	if (clean) {
		fs::remove_all(build_dir, ec);
		if (ec) return fail("cannot clean " + build_dir.string() + ": " + ec.message());
	}

	if (reconfigure || !fs::exists(build_dir / "CMakeCache.txt", ec)) {
		std::vector<std::string> configure = {"cmake",
											  "-S",
											  path_text(project.root),
											  "-B",
											  path_text(build_dir),
											  "-DCMAKE_BUILD_TYPE=" + configuration};
		if (command_available("ninja", project.root)) {
			configure.push_back("-G");
			configure.push_back("Ninja");
		}
		std::cout << "bcli: configuring " << project.root.string() << " (" << configuration
				  << ")\n";
		ProcessResult result = run_process(configure, project.root, false);
		if (!result.started) return fail("could not start cmake");
		if (result.exit_code != 0) return fail("cmake configure failed");
	}

	std::vector<std::string> build = {"cmake",		"--build",	path_text(build_dir),
									  "--parallel", "--config", configuration};
	std::cout << "bcli: building " << project.name << "\n";
	ProcessResult result = run_process(build, project.root, false);
	if (!result.started) return fail("could not start cmake --build");
	if (result.exit_code != 0) return fail("build failed");

	const fs::path executable = find_built_executable(project, configuration);
	if (!executable.empty())
		std::cout << "bcli: executable " << executable.string() << "\n";
	else
		std::cout << "bcli: build succeeded (executable not found in build/)\n";
	return 0;
}

int cmd_new(const std::vector<std::string> &args)
{
	std::string name;
	fs::path	parent = fs::current_path();
	fs::path	engine = default_engine_dir();

	for (size_t i = 0; i < args.size(); i++) {
		std::string value;
		if (parse_option(args, i, "--dir", value))
			parent = value;
		else if (parse_option(args, i, "--engine", value))
			engine = value;
		else if (args[i].rfind("--", 0) == 0)
			return fail("unknown option: " + args[i]);
		else if (name.empty())
			name = args[i];
		else
			return fail("unexpected argument: " + args[i]);
	}
	if (name.empty()) return fail("usage: bcli new <name> [--dir DIR] [--engine ENGINE]");
	if (!valid_project_name(name)) return fail("project name must be a C identifier: " + name);
	if (engine.empty())
		return fail("engine path is required; pass --engine or set BRIDGEENGINE_SOURCE_DIR");
	if (!fs::is_directory(engine)) return fail("engine directory not found: " + engine.string());

	std::error_code ec;
	parent = fs::absolute(parent, ec);
	if (ec) return fail("cannot resolve --dir: " + ec.message());
	const fs::path root = parent / name;
	std::string	   error;
	if (!create_project(root, name, engine, false, error)) return fail(error);
	std::cout << "bcli: created " << root.string() << "\n";
	std::cout << "bcli: next: cd " << root.string() << " && bcli build && bcli run\n";
	return 0;
}

int cmd_init(const std::vector<std::string> &args)
{
	fs::path	root = fs::current_path();
	std::string name;
	fs::path	engine = default_engine_dir();

	for (size_t i = 0; i < args.size(); i++) {
		std::string value;
		if (parse_option(args, i, "--name", value))
			name = value;
		else if (parse_option(args, i, "--engine", value))
			engine = value;
		else if (args[i].rfind("--", 0) == 0)
			return fail("unknown option: " + args[i]);
		else
			return fail("unexpected argument: " + args[i]);
	}
	if (name.empty()) name = root.filename().string();
	if (!valid_project_name(name))
		return fail("project name must be a C identifier; pass --name: " + name);
	if (engine.empty())
		return fail("engine path is required; pass --engine or set BRIDGEENGINE_SOURCE_DIR");

	std::string error;
	if (!create_project(root, name, engine, true, error)) return fail(error);
	std::cout << "bcli: initialized " << root.string() << " as " << name << "\n";
	return 0;
}

int cmd_info(const std::vector<std::string> &args)
{
	Project		project;
	std::string error;
	if (!load_from_cwd(project, error)) return fail(error);

	bool		   json		  = std::find(args.begin(), args.end(), "--json") != args.end();
	const fs::path executable = find_built_executable(project, "Debug");
	if (json) {
		std::cout << "{\"root\":\"" << json_escape(path_text(project.root)) << "\",\"name\":\""
				  << json_escape(project.name) << "\",\"engine\":\"" << json_escape(project.engine)
				  << "\",\"bep\":\"" << json_escape(path_text(project.bep_path))
				  << "\",\"build_dir\":\"" << json_escape(path_text(project.root / "build"))
				  << "\",\"documents\":[";
		for (size_t i = 0; i < project.documents.size(); i++) {
			if (i) std::cout << ',';
			std::cout << '"' << json_escape(project.documents[i]) << '"';
		}
		std::cout << "],\"executable\":";
		if (executable.empty())
			std::cout << "null";
		else
			std::cout << '"' << json_escape(path_text(executable)) << '"';
		std::cout << "}\n";
		return 0;
	}

	std::cout << "project:  " << project.name << "\n";
	std::cout << "root:     " << project.root.string() << "\n";
	std::cout << "bep:      " << project.bep_path.string() << "\n";
	std::cout << "engine:   " << (project.engine.empty() ? "(not set)" : project.engine) << "\n";
	std::cout << "build:    " << (project.root / "build").string() << "\n";
	std::cout << "executable: " << (executable.empty() ? "(not built)" : executable.string())
			  << "\n";
	std::cout << "documents (" << project.documents.size() << "):\n";
	for (const auto &document : project.documents) std::cout << "  " << document << "\n";
	return 0;
}

int cmd_build(const std::vector<std::string> &args)
{
	Project		project;
	std::string error;
	if (!load_from_cwd(project, error)) return fail(error);
	BuildOptions options = parse_build_options(args);
	return build_project(project, options.configuration, options.clean, options.reconfigure);
}

int cmd_run(const std::vector<std::string> &args)
{
	Project		project;
	std::string error;
	if (!load_from_cwd(project, error)) return fail(error);

	BuildOptions			 options = parse_build_options(args);
	bool					 build	 = false;
	std::vector<std::string> program_args;
	bool					 after_separator = false;
	for (const auto &arg : args) {
		if (after_separator) {
			program_args.push_back(arg);
		} else if (arg == "--") {
			after_separator = true;
		} else if (arg == "--build") {
			build = true;
		}
	}
	if (build) {
		int result =
			build_project(project, options.configuration, options.clean, options.reconfigure);
		if (result != 0) return result;
	}

	fs::path executable = find_built_executable(project, options.configuration);
	if (executable.empty()) {
		std::cout << "bcli: executable not found; building first\n";
		int result = build_project(project, options.configuration, false, false);
		if (result != 0) return result;
		executable = find_built_executable(project, options.configuration);
	}
	if (executable.empty()) return fail("built executable not found under build/");

	std::vector<std::string> command = {path_text(executable)};
	command.insert(command.end(), program_args.begin(), program_args.end());
	std::cout << "bcli: running " << executable << "\n";
	ProcessResult result = run_process(command, project.root, false);
	if (!result.started) return fail("could not start " + executable.string());
	return result.exit_code;
}

int cmd_clean(const std::vector<std::string> &args)
{
	(void)args;
	Project		project;
	std::string error;
	if (!load_from_cwd(project, error)) return fail(error);
	std::error_code ec;
	fs::remove_all(project.root / "build", ec);
	if (ec) return fail("cannot remove build directory: " + ec.message());
	std::cout << "bcli: removed " << (project.root / "build").string() << "\n";
	return 0;
}

int cmd_add_document(const std::vector<std::string> &args)
{
	if (args.empty()) return fail("usage: bcli add-doc <relative-path>");
	Project		project;
	std::string error;
	if (!load_from_cwd(project, error)) return fail(error);
	if (!add_document(project, args.front(), "", error)) return fail(error);
	std::cout << "bcli: added " << project.documents.back() << " to " << project.bep_path.string()
			  << "\n";
	return 0;
}

int cmd_remove_document(const std::vector<std::string> &args)
{
	if (args.empty()) return fail("usage: bcli remove-doc <relative-path>");
	Project		project;
	std::string error;
	if (!load_from_cwd(project, error)) return fail(error);
	if (!remove_document(project, args.front(), error)) return fail(error);
	std::cout << "bcli: removed " << args.front() << " from " << project.bep_path.string() << "\n";
	return 0;
}

int cmd_list_documents(const std::vector<std::string> &args)
{
	(void)args;
	Project		project;
	std::string error;
	if (!load_from_cwd(project, error)) return fail(error);
	for (const auto &document : project.documents) std::cout << document << "\n";
	return 0;
}

int cmd_set_engine(const std::vector<std::string> &args)
{
	if (args.empty()) return fail("usage: bcli set-engine <path>");
	Project		project;
	std::string error;
	if (!load_from_cwd(project, error)) return fail(error);
	if (!fs::is_directory(args.front())) return fail("not a directory: " + args.front());
	if (!set_engine(project, args.front(), error)) return fail(error);
	std::cout << "bcli: engine = " << project.engine << "\n";
	return 0;
}

int cmd_doctor(const std::vector<std::string> &args)
{
	(void)args;
	const fs::path cwd = fs::current_path();

	// Prefer the engine-local dependency prefix so doctor works without a
	// system-wide SDL3_image/SDL3_ttf installation.
	const std::string local_pkgconfig	 = local_pkgconfig_dir();
	const char *const previous_pkgconfig = std::getenv("PKG_CONFIG_PATH");
	std::string		  saved_pkgconfig	 = previous_pkgconfig ? previous_pkgconfig : "";
	if (!local_pkgconfig.empty()) {
		std::string combined = local_pkgconfig;
		if (!saved_pkgconfig.empty()) {
#ifdef _WIN32
			combined += ";" + saved_pkgconfig;
#else
			combined += ":" + saved_pkgconfig;
#endif
		}
#ifdef _WIN32
		_putenv_s("PKG_CONFIG_PATH", combined.c_str());
#else
		setenv("PKG_CONFIG_PATH", combined.c_str(), 1);
#endif
	}
	struct Check {
		std::string label;
		bool		required;
		bool		ok;
		std::string detail;
	};
	std::vector<Check> checks;

	const auto add_check = [&](const std::string &label, bool required,
							   const std::vector<std::string> &command) {
		ProcessResult result = run_process(command, cwd, true);
		std::string	  detail = result.output;
		while (!detail.empty() && (detail.back() == '\n' || detail.back() == '\r'))
			detail.pop_back();
		if (detail.size() > 120) detail = detail.substr(0, 120) + "...";
		checks.push_back({label, required, result.started && result.exit_code == 0, detail});
	};

	add_check("cmake", true, {"cmake", "--version"});
	add_check("ninja", false, {"ninja", "--version"});
#ifdef _WIN32
	add_check("MSVC", true, {"cl"});
#else
	add_check("C compiler", true, {"cc", "--version"});
	add_check("C++ compiler", true, {"c++", "--version"});
#endif
	add_check("pkg-config", false, {"pkg-config", "--version"});
	add_check("SDL3", true, {"pkg-config", "--modversion", "sdl3"});
	add_check("SDL3_image", true, {"pkg-config", "--modversion", "sdl3-image"});
	add_check("SDL3_ttf", true, {"pkg-config", "--modversion", "sdl3-ttf"});
	add_check("FFmpeg avcodec", true, {"pkg-config", "--modversion", "libavcodec"});

	bool ok = true;
	for (const auto &check : checks) {
		std::cout << (check.ok ? "[ok]   " : "[miss] ") << check.label;
		if (!check.detail.empty()) std::cout << "  " << check.detail;
		std::cout << "\n";
		if (check.required && !check.ok) ok = false;
	}

	const fs::path tpl = template_dir();
	std::cout << (fs::is_directory(tpl) ? "[ok]   " : "[miss] ") << "project template  "
			  << tpl.string() << "\n";
	if (!fs::is_directory(tpl)) ok = false;
	const fs::path engine = default_engine_dir();
	if (!engine.empty())
		std::cout << "[ok]   engine source  " << engine.string() << "\n";
	else
		std::cout << "[info] engine source not known; pass --engine to bcli new\n";

#ifdef _WIN32
	_putenv_s("PKG_CONFIG_PATH", saved_pkgconfig.c_str());
#else
	if (saved_pkgconfig.empty())
		unsetenv("PKG_CONFIG_PATH");
	else
		setenv("PKG_CONFIG_PATH", saved_pkgconfig.c_str(), 1);
#endif
	return ok ? 0 : 1;
}

int cmd_self_test(const std::vector<std::string> &args)
{
	(void)args;
	int		   failures = 0;
	const auto check	= [&](bool condition, const char *message) {
		if (!condition) {
			std::cerr << "FAIL: " << message << "\n";
			failures++;
		}
	};

	check(valid_project_name("game"), "valid name");
	check(valid_project_name("game_2"), "valid name with underscore");
	check(!valid_project_name("2game"), "name starting with digit is rejected");
	check(!valid_project_name("my-game"), "name with dash is rejected");
	check(!valid_project_name(""), "empty name is rejected");

	std::string normalized, error;
	check(normalize_document_path("./ui/menu.xml", normalized, error) &&
			  normalized == "ui/menu.xml",
		  "document path normalization");
	check(!normalize_document_path("../outside.xml", normalized, error),
		  "parent traversal is rejected");
	check(!normalize_document_path("/absolute.xml", normalized, error),
		  "absolute document path is rejected");
	check(replace_tokens("__PROJECT_NAME__/__ENGINE_DIR__", "demo", "/engine") == "demo//engine",
		  "token replacement");

	const fs::path temp =
		fs::temp_directory_path() /
		("bcli-selftest-" +
		 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
	std::error_code ec;
	fs::create_directories(temp, ec);
	if (ec) {
		std::cerr << "FAIL: cannot create temp directory\n";
		return 1;
	}

	Project project;
	project.root	 = temp;
	project.bep_path = temp / "demo.bep";
	project.name	 = "demo";
	project.engine	 = "/engine";
	check(save_project(project, error), "save project");
	check(add_document(project, "ui/menu.xml", "", error), "add document");
	check(project.documents.size() == 1, "document count after add");

	Project loaded;
	check(load_project_file(project.bep_path, loaded, error), "load project");
	check(loaded.name == "demo" && loaded.engine == "/engine", "project roundtrip values");
	check(loaded.documents.size() == 1 && loaded.documents[0] == "ui/menu.xml",
		  "project roundtrip documents");
	check(remove_document(loaded, "ui/menu.xml", error), "remove document");
	check(loaded.documents.empty(), "document removed");

	const fs::path ui_document = temp / "ui.xml";
	const auto	   has_issue   = [](const std::vector<std::string> &issues, const char *needle) {
		for (const auto &issue : issues)
			if (issue.find(needle) != std::string::npos) return true;
		return false;
	};
	{
		std::vector<std::string> issues;
		check(validate_ui_text(
				  "<?xml version=\"1.0\"?><ui><panel id=\"menu\"><label text=\"x\" /></panel></ui>",
				  ui_document, issues) &&
				  issues.empty(),
			  "valid UI XML");
		check(
			!validate_ui_text("<ui><panel id=\"a\"/><panel id=\"a\"/></ui>", ui_document, issues) &&
				has_issue(issues, "duplicate id"),
			"duplicate UI id detected");
		check(!validate_ui_text("<ui><image src=\"missing.png\"/></ui>", ui_document, issues) &&
				  has_issue(issues, "missing resource"),
			  "missing UI resource detected");
		check(!validate_ui_text("<ui><panel></ui>", ui_document, issues) &&
				  has_issue(issues, "mismatched closing tag"),
			  "mismatched UI tag detected");
		check(!validate_ui_text("<ui><panel id=\"a></ui>", ui_document, issues) &&
				  has_issue(issues, "unterminated attribute quote"),
			  "unterminated UI attribute quote detected");
		check(!validate_ui_text("<a/><b/>", ui_document, issues) &&
				  has_issue(issues, "multiple root elements"),
			  "multiple UI roots detected");
	}

	const fs::path generated = temp / "generated";
	check(create_project(generated, "generated", fs::path("/engine"), false, error),
		  "create project from template");
	check(fs::is_regular_file(generated / "generated.bep"), "project bep renamed");
	check(fs::is_regular_file(generated / "CMakeLists.txt"), "CMakeLists copied");

	fs::remove_all(temp, ec);
	if (failures == 0) std::cout << "bcli self-test: ok\n";
	return failures == 0 ? 0 : 1;
}

} // namespace bcli
