#include "platform_dialogs.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <cstdio>
#include <cstring>

static std::string run_dialog(bool is_open, const char *filter, const char *default_value)
{
	OPENFILENAMEA ofn;
	std::memset(&ofn, 0, sizeof(ofn));

	char buffer[MAX_PATH] = {};
	if (default_value) {
		std::snprintf(buffer, sizeof(buffer), "%s", default_value);
	}

	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = NULL;
	ofn.lpstrFilter = filter;
	ofn.lpstrFile = buffer;
	ofn.nMaxFile = sizeof(buffer);
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
	if (!is_open) ofn.Flags |= OFN_OVERWRITEPROMPT;
	ofn.lpstrDefExt = "xml";
	ofn.lpstrTitle = is_open ? "Open UI Document" : "Save UI Document";

	if ((is_open ? GetOpenFileNameA(&ofn) : GetSaveFileNameA(&ofn)) == 0) return "";
	return std::string(buffer);
}

std::string EditorOpenFileDialog(const char *filter, const char *default_dir)
{
	return run_dialog(true, filter, default_dir);
}

std::string EditorSaveFileDialog(const char *filter, const char *default_name)
{
	return run_dialog(false, filter, default_name);
}

std::string EditorBrowseForFolder(const char *title)
{
	BROWSEINFOA bi;
	std::memset(&bi, 0, sizeof(bi));
	bi.lpszTitle = title;
	bi.ulFlags	  = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

	LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
	if (!pidl) return "";
	char path[MAX_PATH] = {};
	if (!SHGetPathFromIDListA(pidl, path)) {
		CoTaskMemFree(pidl);
		return "";
	}
	CoTaskMemFree(pidl);
	return std::string(path);
}

#else

#include <SDL3/SDL.h>
#include <SDL3/SDL_dialog.h>

#include <atomic>
#include <cstring>
#include <string>
#include <vector>

namespace {

std::atomic<bool> g_dialog_done{false};
std::string g_dialog_result;

// SDL requires the filter array (and its strings) to stay valid until the
// callback fires. One dialog is shown at a time, so static storage is fine.
std::vector<SDL_DialogFileFilter> g_dialog_filters;
std::vector<std::string> g_dialog_patterns;

void SDLCALL dialog_callback(void *userdata, const char *const *filelist, int filter)
{
	(void)userdata;
	(void)filter;
	g_dialog_result.clear();
	// filelist is NULL on error; the first entry is NULL on cancel.
	if (filelist && filelist[0]) g_dialog_result = filelist[0];
	g_dialog_done.store(true, std::memory_order_release);
}

// Convert a Win32-style filter string ("Label (*.xml)\0*.xml\0All (*.*)\0*.*\0")
// into SDL_DialogFileFilter entries. SDL patterns are extension lists without
// wildcards ("xml"), with "*" meaning "all files".
int build_filters(const char *filter)
{
	g_dialog_filters.clear();
	g_dialog_patterns.clear();
	if (!filter) return 0;

	const char *p = filter;
	while (*p) {
		const char *label = p;
		p += std::strlen(p) + 1;
		if (!*p) break;
		const char *pattern = p;
		p += std::strlen(p) + 1;

		std::string cleaned;
		const char *q = pattern;
		while (*q) {
			const char *token = q;
			q += std::strcspn(q, ";");
			std::string ext(token, (size_t)(q - token));
			if (*q == ';') q++;
			if (ext.rfind("*.", 0) == 0) ext.erase(0, 2);
			else if (!ext.empty() && ext[0] == '.') ext.erase(0, 1);
			if (ext.empty()) continue;
			if (!cleaned.empty()) cleaned += ';';
			cleaned += ext;
		}
		if (cleaned.empty()) continue;

		g_dialog_patterns.push_back(cleaned);
		SDL_DialogFileFilter f;
		f.name	 = label;
		f.pattern = g_dialog_patterns.back().c_str();
		g_dialog_filters.push_back(f);
	}
	return (int)g_dialog_filters.size();
}

// The SDL dialogs are asynchronous; pump events until the callback fires so
// the editor keeps its synchronous API. The time bound only guards against a
// backend that never calls back.
std::string wait_for_dialog()
{
	for (int i = 0; i < 60000; i++) { // ~10 minutes
		if (g_dialog_done.load(std::memory_order_acquire)) return g_dialog_result;
		SDL_PumpEvents();
		SDL_Delay(10);
	}
	return "";
}

} // namespace

std::string EditorBrowseForFolder(const char *title)
{
	(void)title; // SDL folder dialogs have no title parameter
	g_dialog_done.store(false, std::memory_order_relaxed);
	SDL_ShowOpenFolderDialog(dialog_callback, nullptr, nullptr, nullptr, false);
	return wait_for_dialog();
}

std::string EditorOpenFileDialog(const char *filter, const char *default_dir)
{
	(void)default_dir; // SDL open dialogs have no start-location parameter
	int n = build_filters(filter);
	g_dialog_done.store(false, std::memory_order_relaxed);
	SDL_ShowOpenFileDialog(dialog_callback, nullptr, nullptr,
						   n > 0 ? g_dialog_filters.data() : nullptr, n, nullptr, false);
	return wait_for_dialog();
}

std::string EditorSaveFileDialog(const char *filter, const char *default_name)
{
	int n = build_filters(filter);
	g_dialog_done.store(false, std::memory_order_relaxed);
	SDL_ShowSaveFileDialog(dialog_callback, nullptr, nullptr,
						   n > 0 ? g_dialog_filters.data() : nullptr, n, default_name);
	return wait_for_dialog();
}

#endif
