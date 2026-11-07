#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace bcli
{

namespace fs = std::filesystem;

struct Project {
	fs::path				 root;
	fs::path				 bep_path;
	std::string				 name;
	std::string				 engine;
	std::vector<std::string> documents;
};

struct ProcessResult {
	bool		started	  = false;
	int			exit_code = -1;
	std::string output;
};

bool valid_project_name(const std::string &name);

/* Find the nearest project by walking up from start. */
bool find_project(const fs::path &start, Project &project, std::string &error);
bool load_project_file(const fs::path &bep_path, Project &project, std::string &error);
bool save_project(Project &project, std::string &error);

bool normalize_document_path(const std::string &input, std::string &normalized, std::string &error);
bool add_document(Project &project, const std::string &path, const std::string &error_prefix,
				  std::string &error);
bool remove_document(Project &project, const std::string &path, std::string &error);
bool set_engine(Project &project, const fs::path &engine, std::string &error);

fs::path	default_engine_dir();
fs::path	template_dir();
bool		create_project(const fs::path &root, const std::string &name, const fs::path &engine,
						   bool init_mode, std::string &error);
std::string replace_tokens(std::string text, const std::string &project_name,
						   const std::string &engine_dir);

ProcessResult run_process(const std::vector<std::string> &args, const fs::path &cwd,
						  bool capture_output);

fs::path find_built_executable(const Project &project, const std::string &configuration);

/* Build a project through CMake. Shared by build, run and watch. */
int build_project(Project &project, const std::string &configuration, bool clean, bool reconfigure);

/* Validate one UI XML document. Issues are already formatted as
 * "<path>:<line>: <message>". src="..." paths resolve relative to the
 * document. Returns true when the document is clean. */
bool validate_ui_text(const std::string &text, const fs::path &document,
					  std::vector<std::string> &issues);

/* Each command returns a process exit code. */
int cmd_new(const std::vector<std::string> &args);
int cmd_init(const std::vector<std::string> &args);
int cmd_info(const std::vector<std::string> &args);
int cmd_build(const std::vector<std::string> &args);
int cmd_run(const std::vector<std::string> &args);
int cmd_clean(const std::vector<std::string> &args);
int cmd_add_document(const std::vector<std::string> &args);
int cmd_remove_document(const std::vector<std::string> &args);
int cmd_list_documents(const std::vector<std::string> &args);
int cmd_set_engine(const std::vector<std::string> &args);
int cmd_doctor(const std::vector<std::string> &args);
int cmd_pack(const std::vector<std::string> &args);
int cmd_ui(const std::vector<std::string> &args);
int cmd_watch(const std::vector<std::string> &args);
int cmd_self_test(const std::vector<std::string> &args);

void print_help();
void print_version();

} // namespace bcli
