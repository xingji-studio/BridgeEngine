#pragma once

#include <string>

// Generates a new BridgeEngine project from the built-in template directory
// (templates/project in the engine source tree). The whole template is copied
// to <parent_dir>/<project_name>, with placeholder tokens substituted:
//   __PROJECT_NAME__ -> project_name   (C identifier, also the window title)
//   __EXEC_NAME__    -> project_name   (cmake target)
//   __ENGINE_DIR__   -> engine_dir     ("" = rely on find_package in the
//                                        generated CMakeLists)
//
// Returns true on success. On failure returns false and fills error_out with
// a human-readable message. On success, out_bep_path (if non-null) receives the
// absolute path of the generated <project_name>.bep project file.
bool EditorCreateProject(const std::string &project_name, const std::string &parent_dir,
						 const std::string &engine_dir, std::string &error_out,
						 std::string *out_bep_path = nullptr);
