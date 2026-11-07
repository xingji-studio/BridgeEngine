#include "cli.h"

#include <cstdio>
#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <direct.h>
#include <process.h>
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace bcli
{

#ifdef _WIN32

namespace
{

std::string windows_quote(const std::string &value)
{
	if (value.find_first_of(" \t\"") == std::string::npos) return value;
	std::string out = "\"";
	for (char c : value) {
		if (c == '"')
			out += "\\\"";
		else
			out += c;
	}
	out += '"';
	return out;
}

std::string command_line(const std::vector<std::string> &args)
{
	std::string command;
	for (const auto &arg : args) {
		if (!command.empty()) command += ' ';
		command += windows_quote(arg);
	}
	return command;
}

} // namespace

ProcessResult run_process(const std::vector<std::string> &args, const fs::path &cwd,
						  bool capture_output)
{
	ProcessResult result;
	if (args.empty()) return result;

	if (!capture_output) {
		char previous[4096];
		if (!_getcwd(previous, sizeof(previous))) previous[0] = '\0';
		if (!cwd.empty()) _chdir(cwd.string().c_str());
		std::vector<const char *> argv;
		for (const auto &arg : args) argv.push_back(arg.c_str());
		argv.push_back(nullptr);
		result.started	 = true;
		result.exit_code = _spawnvp(_P_WAIT, argv[0], argv.data());
		if (previous[0]) _chdir(previous);
		return result;
	}

	const std::string command = "cd /d \"" + cwd.string() + "\" && " + command_line(args) + " 2>&1";
	FILE			 *pipe	  = _popen(command.c_str(), "r");
	if (!pipe) return result;
	result.started = true;
	char buffer[4096];
	while (std::fgets(buffer, sizeof(buffer), pipe)) result.output += buffer;
	result.exit_code = _pclose(pipe);
	return result;
}

#else

ProcessResult run_process(const std::vector<std::string> &args, const fs::path &cwd,
						  bool capture_output)
{
	ProcessResult result;
	if (args.empty()) return result;

	int pipefd[2] = {-1, -1};
	if (capture_output && pipe(pipefd) != 0) return result;

	const pid_t pid = fork();
	if (pid < 0) {
		if (capture_output) {
			close(pipefd[0]);
			close(pipefd[1]);
		}
		return result;
	}

	if (pid == 0) {
		if (!cwd.empty()) {
			if (chdir(cwd.string().c_str()) != 0) _exit(127);
		}
		if (capture_output) {
			dup2(pipefd[1], STDOUT_FILENO);
			dup2(pipefd[1], STDERR_FILENO);
			close(pipefd[0]);
			close(pipefd[1]);
		}
		std::vector<char *> argv;
		for (const auto &arg : args) argv.push_back(const_cast<char *>(arg.c_str()));
		argv.push_back(nullptr);
		execvp(argv[0], argv.data());
		_exit(127);
	}

	result.started = true;
	if (capture_output) {
		close(pipefd[1]);
		char	buffer[4096];
		ssize_t count;
		while ((count = read(pipefd[0], buffer, sizeof(buffer))) > 0)
			result.output.append(buffer, static_cast<size_t>(count));
		close(pipefd[0]);
	}
	int status = 0;
	if (waitpid(pid, &status, 0) < 0) {
		result.exit_code = -1;
	} else if (WIFEXITED(status)) {
		result.exit_code = WEXITSTATUS(status);
	} else if (WIFSIGNALED(status)) {
		result.exit_code = 128 + WTERMSIG(status);
	}
	return result;
}

#endif

fs::path find_built_executable(const Project &project, const std::string &configuration)
{
	std::error_code ec;
	const fs::path	build  = project.root / "build";
	const char	   *suffix = "";
#ifdef _WIN32
	suffix = ".exe";
#endif
	const std::vector<fs::path> candidates = {
		build / (project.name + suffix),
		build / configuration / (project.name + suffix),
		build / "Release" / (project.name + suffix),
		build / "RelWithDebInfo" / (project.name + suffix),
		build / "bin" / (project.name + suffix),
		build / "bin" / configuration / (project.name + suffix),
	};
	for (const auto &candidate : candidates) {
		if (fs::is_regular_file(candidate, ec)) return candidate;
	}
	return {};
}

} // namespace bcli
