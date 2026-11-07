#pragma once

#include <string>

// Native file dialogs (Windows only; non-Windows builds return empty strings).
// Only ASCII paths are guaranteed: the ANSI (GetOpenFileNameA) variant is used,
// so names with non-ASCII characters may be mangled. Callers treat an empty
// result as "user cancelled".

// Opens a "pick an existing file" dialog. Returns the chosen path or "" on cancel.
std::string EditorOpenFileDialog(const char *filter, const char *default_dir);

// Opens a "save as" dialog. Returns the chosen path or "" on cancel.
std::string EditorSaveFileDialog(const char *filter, const char *default_name);

// Opens a "pick a folder" dialog. Returns the chosen directory or "" on cancel.
std::string EditorBrowseForFolder(const char *title);
