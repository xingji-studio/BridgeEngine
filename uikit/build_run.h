#pragma once

struct EditorState;

// Build the current project with cmake (configure if needed, then build) in a
// background thread; output is appended to state.build_log.
bool EditorCanBuild(const EditorState &state);
void EditorBuildProject(EditorState &state);

// Run the built project executable (working directory = its folder so assets
// resolve relative to it).
bool EditorCanRun(const EditorState &state);
void EditorRunProject(EditorState &state);

// Terminate the running project process, if any.
bool EditorCanStopRun(const EditorState &state);
void EditorStopRunProject(EditorState &state);

// Build log panel.
void EditorBuildOutputPanel(EditorState &state);

// Join a finished build thread (call every frame).
void EditorUpdateBuildThread(EditorState &state);
