#include "build_run.h"

#include "editor.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dirent.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>

// Path separator and "does it exist" probe for the platform's native syntax.
#ifdef _WIN32
static const char kSep = '\\';

static bool path_exists(const std::string &path)
{
	return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}
#else
static const char kSep = '/';

static bool path_exists(const std::string &path)
{
	struct stat st;
	return ::stat(path.c_str(), &st) == 0;
}
#endif

static void append_log(EditorState *state, const std::string &text)
{
	std::lock_guard<std::mutex> lock(state->build_log_mutex);
	state->build_log += text;
}

static int run_command(EditorState *state, const char *command)
{
	append_log(state, "> ");
	append_log(state, command);
	append_log(state, "\n");

	FILE *pipe =
#ifdef _WIN32
		_popen(command, "r");
#else
		::popen(command, "r");
#endif
	if (!pipe) {
		append_log(state, "failed to start process\n");
		return -1;
	}
	char buffer[1024];
	while (std::fgets(buffer, sizeof(buffer), pipe)) append_log(state, buffer);
#ifdef _WIN32
	int exit_code = _pclose(pipe);
#else
	// pclose returns the raw wait status; normalize it like a shell would.
	int status = ::pclose(pipe);
	int exit_code;
	if (status == -1) {
		exit_code = -1;
	} else if (WIFEXITED(status)) {
		exit_code = WEXITSTATUS(status);
	} else if (WIFSIGNALED(status)) {
		exit_code = 128 + WTERMSIG(status);
	} else {
		exit_code = status;
	}
#endif

	char tail[64];
	std::snprintf(tail, sizeof(tail), "\n[exit code %d]\n", exit_code);
	append_log(state, tail);
	return exit_code;
}

static std::string project_root(const EditorState &state)
{
	if (state.project_path.empty()) return "";
	size_t slash = state.project_path.find_last_of("\\/");
	return slash == std::string::npos ? state.project_path : state.project_path.substr(0, slash);
}

static std::string find_exe_recursive(const std::string &dir, const std::string &exe_name)
{
#ifdef _WIN32
	WIN32_FIND_DATAA fd;
	std::string pattern = dir + "\\*";
	HANDLE handle		= FindFirstFileA(pattern.c_str(), &fd);
	if (handle == INVALID_HANDLE_VALUE) return "";
	do {
		std::string path = dir + "\\" + fd.cFileName;
		if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			if (fd.cFileName[0] == '.') continue;
			std::string sub = find_exe_recursive(path, exe_name);
			if (!sub.empty()) {
				FindClose(handle);
				return sub;
			}
		} else if (_stricmp(fd.cFileName, exe_name.c_str()) == 0) {
			FindClose(handle);
			return path;
		}
	} while (FindNextFileA(handle, &fd) != 0);
	FindClose(handle);
#else
	DIR *d = ::opendir(dir.c_str());
	if (!d) return "";
	struct dirent *ent;
	while ((ent = ::readdir(d)) != NULL) {
		if (std::strcmp(ent->d_name, ".") == 0 || std::strcmp(ent->d_name, "..") == 0) continue;
		std::string path = dir + "/" + ent->d_name;
		bool is_dir;
#ifdef DT_DIR
		if (ent->d_type == DT_DIR) {
			is_dir = true;
		} else if (ent->d_type != DT_UNKNOWN) {
			is_dir = false;
		} else
#endif
		{
			// Only directories need the stat fallback; for anything else a
			// failed stat just means "not the exe".
			struct stat st;
			is_dir = ::stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
		}
		if (is_dir) {
			if (ent->d_name[0] == '.') continue;
			std::string sub = find_exe_recursive(path, exe_name);
			if (!sub.empty()) {
				::closedir(d);
				return sub;
			}
		} else if (std::strcmp(ent->d_name, exe_name.c_str()) == 0) {
			::closedir(d);
			return path;
		}
	}
	::closedir(d);
#endif
	return "";
}

// Relative path of the project's build directory under its root.
static const char *kBuildDirName = "build";

static void build_worker(EditorState *state, std::string root, std::string config)
{
	append_log(state, "=== configure ===\n");
	std::string cache_path = root + kSep + kBuildDirName + kSep + "CMakeCache.txt";
	if (!path_exists(cache_path)) {
		std::string command = "cmake -S \"" + root + "\" -B \"" + root + kSep + kBuildDirName + "\"";
#ifdef _WIN32
		const char *vcpkg_root = std::getenv("VCPKG_ROOT");
		if (vcpkg_root && vcpkg_root[0]) {
			command += " -DCMAKE_TOOLCHAIN_FILE=";
			command += vcpkg_root;
			command += "/scripts/buildsystems/vcpkg.cmake";
			command += " -DVCPKG_TARGET_TRIPLET=x64-windows";
		}
#endif
		command += " 2>&1";
		run_command(state, command.c_str());
	}

	append_log(state, "=== build ===\n");
	std::string command =
		"cmake --build \"" + root + kSep + kBuildDirName + "\" --config " + config + " 2>&1";
	int build_exit			= run_command(state, command.c_str());
	state->build_succeeded = build_exit == 0;
	state->build_running   = false;
}

void EditorBuildProject(EditorState &state)
{
	if (state.build_running) return;
	std::string root = project_root(state);
	if (root.empty()) return;

	state.build_succeeded = false;
	state.build_running	  = true;
	{
		std::lock_guard<std::mutex> lock(state.build_log_mutex);
		state.build_log.clear();
	}
	std::string config = state.build_config;
	state.build_thread = std::make_unique<std::thread>(build_worker, &state, root, config);
}

void EditorUpdateBuildThread(EditorState &state)
{
	if (state.build_thread && state.build_thread->joinable() && !state.build_running) {
		state.build_thread->join();
		state.build_thread.reset();
	}
}

bool EditorCanBuild(const EditorState &state)
{
	return !state.project_path.empty() && !state.build_running;
}

bool EditorCanRun(const EditorState &state)
{
	return !state.project_path.empty() && !state.build_running && state.build_succeeded;
}

// The built executable carries no ".exe" suffix on POSIX systems.
static std::string exe_file_name(const std::string &base)
{
#ifdef _WIN32
	return base + ".exe";
#else
	return base;
#endif
}

void EditorRunProject(EditorState &state)
{
	std::string root = project_root(state);
	if (root.empty()) return;

	// Executable name = project name (cmake target / exe name).
	std::string base = state.project_path;
	size_t slash	 = base.find_last_of("\\/");
	base = slash == std::string::npos ? base : base.substr(slash + 1);
	if (base.size() > 4 && base.compare(base.size() - 4, 4, ".bep") == 0) base.resize(base.size() - 4);

	std::string exe =
		find_exe_recursive(root + kSep + kBuildDirName, exe_file_name(base));
	if (exe.empty()) {
		append_log(&state, "run: executable not found in the build directory\n");
		return;
	}
	state.build_exe_path = exe;

	// main.c loads assets relative to the working directory, so run from the
	// executable's folder.
	std::string exe_dir = exe;
	size_t exe_slash	= exe_dir.find_last_of("\\/");
	exe_dir = exe_slash == std::string::npos ? "." : exe_dir.substr(0, exe_slash);

#ifdef _WIN32
	STARTUPINFOA si;
	PROCESS_INFORMATION pi;
	std::memset(&si, 0, sizeof(si));
	std::memset(&pi, 0, sizeof(pi));
	si.cb = sizeof(si);
	if (CreateProcessA(NULL, const_cast<char *>(exe.c_str()), NULL, NULL, FALSE, 0, NULL,
					   exe_dir.c_str(), &si, &pi)) {
		// Keep the process handle so we can terminate it on demand; the thread
		// handle is not needed. The handle is closed by EditorStopRunProject or
		// on editor shutdown.
		CloseHandle(pi.hThread);
		state.run_process_handle = pi.hProcess;
		state.run_process_id	 = (uint64_t)pi.dwProcessId;
		append_log(&state, "run: started " + exe + " (pid " + std::to_string(pi.dwProcessId) +
						  ")\n");
	} else {
		append_log(&state, "run: failed to start " + exe + "\n");
	}
#else
	// The pid doubles as the opaque non-null handle marker.
	pid_t pid = ::fork();
	if (pid < 0) {
		append_log(&state, "run: failed to start " + exe + "\n");
		return;
	}
	if (pid == 0) {
		// Child: run the project from its own folder so relative asset paths
		// resolve, then exec (never returns).
		if (::chdir(exe_dir.c_str()) != 0) ::_exit(127);
		char *argv[] = {const_cast<char *>(exe.c_str()), NULL};
		::execv(exe.c_str(), argv);
		::_exit(127);
	}
	state.run_process_handle = (void *)(uintptr_t)pid;
	state.run_process_id	 = (uint64_t)pid;
	append_log(&state, "run: started " + exe + " (pid " + std::to_string(pid) + ")\n");
#endif
}

bool EditorCanStopRun(const EditorState &state)
{
	return state.run_process_handle != nullptr;
}

void EditorStopRunProject(EditorState &state)
{
	if (!state.run_process_handle) return;
#ifdef _WIN32
	HANDLE process = (HANDLE)state.run_process_handle;
	TerminateProcess(process, 1);
	WaitForSingleObject(process, 5000);
	CloseHandle(process);
#else
	pid_t pid = (pid_t)(uintptr_t)state.run_process_handle;
	::kill(pid, SIGTERM);
	// Give it 5 seconds to exit, then force-kill. WNOHANG polling also reaps
	// the child so it never lingers as a zombie.
	for (int i = 0; i < 50; i++) {
		if (::waitpid(pid, NULL, WNOHANG) == pid) goto stopped;
		::usleep(100 * 1000);
	}
	::kill(pid, SIGKILL);
	::waitpid(pid, NULL, 0);
stopped:;
#endif
	state.run_process_handle = nullptr;
	state.run_process_id	 = 0;
	append_log(&state, "run: stopped\n");
}
