#include "cli.h"

#include <cstdint>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <vector>

#ifdef _WIN32
#include <io.h>
#include <process.h>
#else
#include <unistd.h>
#endif

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

namespace bcli
{

namespace
{

std::string path_text(const fs::path &path)
{
	return path.lexically_normal().generic_string();
}

std::string lower_extension(const fs::path &path)
{
	std::string extension = path.extension().string();
	std::transform(extension.begin(), extension.end(), extension.begin(),
				   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return extension;
}

bool is_regular_executable(const fs::path &path)
{
	std::error_code ec;
	if (!fs::is_regular_file(path, ec)) return false;
#ifdef _WIN32
	return true;
#else
	return access(path.c_str(), X_OK) == 0;
#endif
}

std::string tool_filename(const std::string &name)
{
#ifdef _WIN32
	return name + ".exe";
#else
	return name;
#endif
}

fs::path executable_directory()
{
#ifdef _WIN32
	if (const char *program = _pgmptr) return fs::path(program).parent_path();
#elif defined(__APPLE__)
	uint32_t size = 4096;
	std::vector<char> buffer(size);
	if (_NSGetExecutablePath(buffer.data(), &size) == 0)
		return fs::path(buffer.data()).parent_path();
#else
	std::error_code ec;
	const fs::path	self = fs::read_symlink("/proc/self/exe", ec);
	if (!ec) return self.parent_path();
#endif
	return fs::current_path();
}

void append_engine_build_dirs(std::vector<fs::path> &dirs, const fs::path &engine)
{
	if (engine.empty()) return;
	const fs::path build = engine / "build";
	dirs.push_back(build);
	std::error_code ec;
	for (fs::directory_iterator it(build, ec), end; it != end; it.increment(ec)) {
		if (ec) break;
		std::error_code entry_ec;
		if (!it->is_directory(entry_ec)) continue;
		dirs.push_back(it->path());
		dirs.push_back(it->path() / "bin");
	}
}

fs::path find_tool(const std::string &name, const Project &project)
{
	const std::string	  filename = tool_filename(name);
	std::vector<fs::path> directories;

	if (const char *env = std::getenv("BRIDGEENGINE_TOOLS_DIR")) {
		if (env[0] != '\0') directories.emplace_back(env);
	}

	if (const char *env = std::getenv("PATH")) {
		const std::string path = env;
#ifdef _WIN32
		const char separator = ';';
#else
		const char separator = ':';
#endif
		size_t start = 0;
		while (start <= path.size()) {
			const size_t	  end = path.find(separator, start);
			const std::string entry =
				path.substr(start, end == std::string::npos ? std::string::npos : end - start);
			if (!entry.empty()) directories.emplace_back(entry);
			if (end == std::string::npos) break;
			start = end + 1;
		}
	}

	if (!project.engine.empty()) append_engine_build_dirs(directories, project.engine);
	const fs::path default_engine = default_engine_dir();
	if (!default_engine.empty() && default_engine.string() != project.engine)
		append_engine_build_dirs(directories, default_engine);

	directories.push_back(executable_directory());

	std::error_code ec;
	for (const auto &directory : directories) {
		const fs::path candidate = directory / filename;
		if (is_regular_executable(candidate)) return candidate;
	}
	return {};
}

fs::path require_tool(const std::string &name, const Project &project)
{
	const fs::path tool = find_tool(name, project);
	if (!tool.empty()) return tool;
	std::cerr << "bcli: missing tool: " << tool_filename(name) << "\n";
	std::cerr
		<< "bcli: looked in BRIDGEENGINE_TOOLS_DIR, PATH, <engine>/build/* and next to bcli\n";
	std::cerr << "bcli: build it with: cmake --build <engine>/build --target " << name << "\n";
	return {};
}

std::vector<std::string> collect_relative_files(const fs::path &directory)
{
	std::vector<std::string> files;
	std::error_code			 ec;
	for (fs::recursive_directory_iterator
			 it(directory, fs::directory_options::skip_permission_denied, ec),
		 end;
		 it != end; it.increment(ec)) {
		if (ec) break;
		if (!it->is_regular_file(ec)) continue;
		files.push_back(path_text(fs::relative(it->path(), directory, ec)));
		if (ec) {
			ec.clear();
			files.back() = path_text(it->path().lexically_relative(directory));
		}
	}
	std::sort(files.begin(), files.end());
	return files;
}

bool is_atlas_image(const fs::path &path)
{
	const std::string extension = lower_extension(path);
	return extension == ".png" || extension == ".jpg" || extension == ".jpeg" ||
		   extension == ".bmp" || extension == ".tga" || extension == ".webp";
}

std::vector<fs::path> collect_images(const fs::path &directory)
{
	std::vector<fs::path> images;
	std::error_code		  ec;
	for (fs::recursive_directory_iterator
			 it(directory, fs::directory_options::skip_permission_denied, ec),
		 end;
		 it != end; it.increment(ec)) {
		if (ec) break;
		if (!it->is_regular_file(ec)) continue;
		if (is_atlas_image(it->path())) images.push_back(it->path());
	}
	std::sort(images.begin(), images.end());
	return images;
}

int fail(const std::string &message)
{
	std::cerr << "bcli: " << message << "\n";
	return 1;
}

struct PackOptions {
	fs::path	assets;
	fs::path	out;
	fs::path	atlas;
	std::string bctex;
	bool		assets_set = false;
	bool		out_set	   = false;
	bool		atlas_set  = false;
	bool		bctex_set  = false;
	bool		rz		   = true;
};

bool parse_options(const std::vector<std::string> &args, PackOptions &options, std::string &error)
{
	for (size_t i = 0; i < args.size(); i++) {
		const std::string &arg	= args[i];
		const auto		   take = [&](const char *name, fs::path &slot, bool &flag) {
			if (arg != name) return false;
			if (i + 1 >= args.size()) {
				error = std::string("missing value for ") + name;
				return true;
			}
			slot = args[++i];
			flag = true;
			return true;
		};
		if (take("--assets", options.assets, options.assets_set)) {
			if (!error.empty()) return false;
			continue;
		}
		if (take("--out", options.out, options.out_set)) {
			if (!error.empty()) return false;
			continue;
		}
		if (take("--atlas", options.atlas, options.atlas_set)) {
			if (!error.empty()) return false;
			continue;
		}
		if (arg == "--bctex") {
			if (i + 1 >= args.size()) {
				error = "missing value for --bctex";
				return false;
			}
			options.bctex	  = args[++i];
			options.bctex_set = true;
			continue;
		}
		if (arg == "--rz") {
			options.rz = true;
			continue;
		}
		if (arg == "--no-rz") {
			options.rz = false;
			continue;
		}
		if (arg.rfind("--", 0) == 0) {
			error = "unknown option: " + arg;
			return false;
		}
		error = "unexpected argument: " + arg;
		return false;
	}
	return true;
}

fs::path resolve_from_root(const fs::path &root, const fs::path &path)
{
	if (path.is_absolute()) return path.lexically_normal();
	return (root / path).lexically_normal();
}

int pack_archive(const Project &project, const fs::path &tool, const fs::path &source_dir,
				 const fs::path &archive, const std::vector<std::string> &files)
{
	if (files.empty()) return fail("no files to pack under " + source_dir.string());

	std::error_code ec;
	fs::create_directories(archive.parent_path(), ec);
	if (ec) return fail("cannot create " + archive.parent_path().string() + ": " + ec.message());

	std::vector<std::string> command = {path_text(tool), "-C", path_text(source_dir), "-o",
										path_text(archive)};
	command.insert(command.end(), files.begin(), files.end());
	std::cout << "bcli: archive " << archive.string() << " (" << files.size() << " files)\n";
	ProcessResult result = run_process(command, project.root, false);
	if (!result.started) return fail("could not start " + tool.string());
	if (result.exit_code != 0) return fail("archive tool failed: " + tool.string());
	return 0;
}

int run_atlas(const Project &project, const PackOptions &options, const fs::path &archive)
{
	std::error_code ec;
	if (!fs::is_directory(options.atlas, ec))
		return fail("atlas image directory not found: " + options.atlas.string());
	const std::vector<fs::path> images = collect_images(options.atlas);
	if (images.empty()) return fail("no images found under " + options.atlas.string());
	if (!options.bctex.empty() && options.bctex != "bc1" && options.bctex != "bc3")
		return fail("--bctex must be bc1 or bc3");

	const fs::path atlas_tool = require_tool("bridgeengine_atlas_pack", project);
	if (atlas_tool.empty()) return 1;

	fs::path out_dir;
	if (archive.empty()) {
		out_dir = options.out;
	} else {
		out_dir = archive.parent_path() / archive.stem();
	}
	out_dir = resolve_from_root(project.root, out_dir);
	fs::create_directories(out_dir, ec);
	if (ec) return fail("cannot create " + out_dir.string() + ": " + ec.message());

	std::vector<std::string> command = {
		path_text(atlas_tool), "-o", path_text(out_dir), "-n", "atlas", "--format", "png"};
	for (const auto &image : images) command.push_back(path_text(image));

	std::cout << "bcli: atlas " << options.atlas.string() << " -> " << out_dir.string() << " ("
			  << images.size() << " images)\n";
	ProcessResult result = run_process(command, project.root, false);
	if (!result.started) return fail("could not start " + atlas_tool.string());
	if (result.exit_code != 0) return fail("atlas packer failed: " + atlas_tool.string());

	const fs::path page		= out_dir / "atlas.png";
	const fs::path manifest = out_dir / "atlas.atlas";
	if (!fs::is_regular_file(manifest, ec))
		return fail("atlas packer did not produce " + manifest.string());
	std::cout << "bcli: atlas page " << page.string() << "\n";
	std::cout << "bcli: atlas manifest " << manifest.string() << "\n";

	if (!options.bctex.empty()) {
		const fs::path bctex_tool = require_tool("bridgeengine_bctex_pack", project);
		if (bctex_tool.empty()) return 1;
		const fs::path			 bctex_output  = out_dir / "atlas.bctex";
		std::vector<std::string> bctex_command = {path_text(bctex_tool),   "-f",
												  options.bctex,		   "-o",
												  path_text(bctex_output), path_text(page)};
		ProcessResult			 bctex_result  = run_process(bctex_command, project.root, false);
		if (!bctex_result.started) return fail("could not start " + bctex_tool.string());
		if (bctex_result.exit_code != 0) return fail("bctex packer failed: " + bctex_tool.string());
		std::cout << "bcli: bctex " << bctex_output.string() << " (" << options.bctex << ")\n";
	}

	if (archive.empty()) return 0;
	const fs::path rz_tool = require_tool("bridgeengine_rz_pack", project);
	if (rz_tool.empty()) return 1;
	return pack_archive(project, rz_tool, out_dir, archive, collect_relative_files(out_dir));
}

} // namespace

int cmd_pack(const std::vector<std::string> &args)
{
	PackOptions options;
	std::string error;
	if (!parse_options(args, options, error)) return fail(error);

	Project		project;
	std::string load_error;
	if (!find_project(fs::current_path(), project, load_error)) {
		if (options.assets_set || options.atlas_set) {
			project.root = fs::current_path();
			project.name = project.root.filename().string();
		} else {
			return fail(load_error);
		}
	}
	project.root = fs::absolute(project.root).lexically_normal();

	if (options.atlas_set) {
		if (options.assets_set)
			std::cout << "bcli: note: --assets is ignored when --atlas is used\n";

		if (options.out_set) options.out = resolve_from_root(project.root, options.out);
		const bool out_is_directory =
			options.out_set && (fs::is_directory(options.out) || options.out.extension().empty());

		if (!options.rz || out_is_directory) {
			if (!options.out_set) {
				options.out = project.root / "build" / "atlas";
			} else if (options.out.extension() == ".rz") {
				return fail("--no-rz needs --out to name an output directory");
			}
			return run_atlas(project, options, {});
		}

		if (!options.out_set) options.out = project.root / "build" / "assets.rz";
		return run_atlas(project, options, options.out);
	}

	if (options.bctex_set) return fail("--bctex requires --atlas");

	if (!options.assets_set) options.assets = project.root / "assets";
	options.assets = resolve_from_root(project.root, options.assets);
	if (!fs::is_directory(options.assets))
		return fail("assets directory not found: " + options.assets.string());

	if (!options.out_set) options.out = project.root / "build" / "assets.rz";
	options.out = resolve_from_root(project.root, options.out);
	if (fs::is_directory(options.out))
		return fail("--out must be a file, not a directory: " + options.out.string());
	if (!options.rz) return fail("--no-rz only applies to --atlas");

	const fs::path rz_tool = require_tool("bridgeengine_rz_pack", project);
	if (rz_tool.empty()) return 1;

	const std::vector<std::string> files = collect_relative_files(options.assets);
	return pack_archive(project, rz_tool, options.assets, options.out, files);
}

} // namespace bcli
