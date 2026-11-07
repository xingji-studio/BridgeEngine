#include "cli.h"

#include <chrono>
#include <ctime>
#include <iostream>
#include <map>
#include <thread>
#include <vector>

namespace bcli
{

namespace
{

using FileMap = std::map<std::string, fs::file_time_type>;

std::string path_text(const fs::path &path)
{
	return path.lexically_normal().generic_string();
}

std::string timestamp()
{
	const std::time_t now = std::time(nullptr);
	std::tm			  tm{};
#ifdef _WIN32
	localtime_s(&tm, &now);
#else
	localtime_r(&now, &tm);
#endif
	char buffer[32];
	if (std::strftime(buffer, sizeof(buffer), "%H:%M:%S", &tm) == 0) return "??:??:??";
	return buffer;
}

void record_file(const fs::path &path, FileMap &files)
{
	std::error_code ec;
	const auto		time = fs::last_write_time(path, ec);
	if (ec) return;
	files[path_text(path)] = time;
}

void record_tree(const fs::path &directory, FileMap &files)
{
	std::error_code ec;
	for (fs::recursive_directory_iterator
			 it(directory, fs::directory_options::skip_permission_denied, ec),
		 end;
		 it != end; it.increment(ec)) {
		if (ec) break;
		if (it->is_directory(ec)) {
			if (it->path().filename() == "build") it.disable_recursion_pending();
			continue;
		}
		if (it->is_regular_file(ec)) record_file(it->path(), files);
	}
}

FileMap snapshot(const fs::path &root)
{
	FileMap			files;
	std::error_code ec;
	record_file(root / "CMakeLists.txt", files);
	for (fs::directory_iterator it(root, ec), end; it != end; it.increment(ec)) {
		if (ec) break;
		if (it->is_regular_file(ec) && it->path().extension() == ".bep")
			record_file(it->path(), files);
	}
	for (const char *name : {"src", "assets", "ui"}) {
		const fs::path directory = root / name;
		if (fs::is_directory(directory, ec)) record_tree(directory, files);
	}
	return files;
}

std::vector<std::string> differences(const FileMap &previous, const FileMap &current)
{
	std::vector<std::string> changes;
	for (const auto &entry : current) {
		auto found = previous.find(entry.first);
		if (found == previous.end())
			changes.push_back(entry.first + " (new)");
		else if (found->second != entry.second)
			changes.push_back(entry.first);
	}
	for (const auto &entry : previous) {
		if (current.find(entry.first) == current.end())
			changes.push_back(entry.first + " (removed)");
	}
	return changes;
}

int run_project(const Project &project)
{
	const fs::path executable = find_built_executable(project, "Debug");
	if (executable.empty()) {
		std::cout << "bcli: executable not found under build/" << std::endl;
		return 1;
	}
	std::cout << "[" << timestamp() << "] running " << executable.string() << std::endl;
	ProcessResult result = run_process({path_text(executable)}, project.root, false);
	if (!result.started) return 1;
	return result.exit_code;
}

int fail(const std::string &message)
{
	std::cerr << "bcli: " << message << "\n";
	return 1;
}

} // namespace

int cmd_watch(const std::vector<std::string> &args)
{
	bool	  run		  = false;
	long long interval_ms = 250;

	for (size_t i = 0; i < args.size(); i++) {
		const std::string &arg = args[i];
		if (arg == "--run") {
			run = true;
		} else if (arg == "--build") {
			// Accepted for compatibility: watch always builds on change.
		} else if (arg == "--interval") {
			if (i + 1 >= args.size()) return fail("missing value for --interval");
			try {
				interval_ms = std::stoll(args[++i]);
			} catch (...) {
				return fail("invalid --interval value: " + args[i]);
			}
			if (interval_ms < 50 || interval_ms > 600000)
				return fail("--interval must be between 50 and 600000 ms");
		} else if (arg.rfind("--", 0) == 0) {
			return fail("unknown option: " + arg);
		} else {
			return fail("unexpected argument: " + arg);
		}
	}

	Project		project;
	std::string error;
	if (!find_project(fs::current_path(), project, error)) return fail(error);

	std::cout << "bcli: watching " << project.root.string() << " every " << interval_ms << " ms"
			  << std::endl;
	std::cout << "bcli: press Ctrl+C to stop" << std::endl;

	FileMap previous = snapshot(project.root);
	while (true) {
		std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
		FileMap current = snapshot(project.root);
		if (current == previous) continue;

		const std::vector<std::string> changes = differences(previous, current);
		for (size_t i = 0; i < changes.size() && i < 5; i++)
			std::cout << "[" << timestamp() << "] change: " << changes[i] << std::endl;
		if (changes.size() > 5)
			std::cout << "[" << timestamp() << "] change: ... (" << changes.size() - 5 << " more)"
					  << std::endl;
		previous = std::move(current);

		int result = build_project(project, "Debug", false, false);
		if (result != 0) {
			std::cout << "[" << timestamp() << "] build failed; still watching" << std::endl;
			continue;
		}
		std::cout << "[" << timestamp() << "] build ok" << std::endl;
		if (run) run_project(project);
	}
}

} // namespace bcli
