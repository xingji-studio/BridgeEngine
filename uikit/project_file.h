#pragma once

#include <string>
#include <vector>

struct EditorState;

bool EditorOpenProject(EditorState &state, const char *path, std::string &error_out);
bool EditorSaveProject(EditorState &state, std::string &error_out);
void EditorPushRecentProject(EditorState &state, const char *path);
void EditorLoadRecentProjects(EditorState &state);
void EditorSaveRecentProjects(EditorState &state);

// Per-project session: remembers which documents were open the last time a
// project was used, so reopening the project restores the working set instead
// of every document in the .bep. Documents are stored as paths relative to the
// project root (the folder holding the .bep), matching the [documents] section.
void EditorSaveProjectSession(EditorState &state);
std::vector<std::string> EditorLoadProjectSession(EditorState &state, const std::string &project_path);
