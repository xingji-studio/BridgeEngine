#include "i18n.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>

#ifndef BRIDGEENGINE_SOURCE_DIR
#define BRIDGEENGINE_SOURCE_DIR ""
#endif

namespace {

EditorLang g_lang = EditorLang::Chinese;
const char *kSettingsFile = "editor_settings.txt";

// Translation tables, loaded from editor/locale/<lang>.txt. Values are stable
// after loading (L() returns c_str() into these maps).
std::unordered_map<std::string, std::string> g_zh;
std::unordered_map<std::string, std::string> g_en;

std::string trim(const std::string &value)
{
	const char *spaces = " \t";
	size_t first = value.find_first_not_of(spaces);
	if (first == std::string::npos) return "";
	size_t last = value.find_last_not_of(spaces);
	return value.substr(first, last - first + 1);
}

void load_lang_file(const std::string &path, std::unordered_map<std::string, std::string> &out)
{
	FILE *file = std::fopen(path.c_str(), "rb");
	if (!file) return;
	char line[1024];
	while (std::fgets(line, sizeof(line), file)) {
		std::string text(line);
		while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) text.pop_back();
		text = trim(text);
		if (text.empty() || text[0] == '#') continue;
		size_t equals = text.find('=');
		if (equals == std::string::npos) continue;
		std::string key = trim(text.substr(0, equals));
		std::string val = trim(text.substr(equals + 1));
		if (key.empty()) continue;
		out[key] = val;
	}
	std::fclose(file);
}

std::string locale_path(const char *name)
{
	// Forward slashes work for fopen on every platform, including Windows.
	std::string base = BRIDGEENGINE_SOURCE_DIR;
	if (!base.empty() && base.back() != '/') base += '/';
	return base + "editor/locale/" + name;
}

} // namespace

void EditorSetLanguage(EditorLang lang)
{
	g_lang = lang;
	EditorSaveLanguage();
}

EditorLang EditorGetLanguage()
{
	return g_lang;
}

const char *L(const char *key)
{
	if (!key || !key[0]) return key;
	const std::unordered_map<std::string, std::string> &table =
		g_lang == EditorLang::Chinese ? g_zh : g_en;
	auto it = table.find(key);
	return it == table.end() ? key : it->second.c_str();
}

void EditorLoadLanguage()
{
	// Load the translation tables from the locale folder.
	load_lang_file(locale_path("zh_CN.txt"), g_zh);
	load_lang_file(locale_path("en.txt"), g_en);

	// Read the persisted preference.
	FILE *file = std::fopen(kSettingsFile, "rb");
	if (!file) return;
	char line[64];
	if (std::fgets(line, sizeof(line), file)) {
		if (std::strstr(line, "en") || std::strstr(line, "english")) g_lang = EditorLang::English;
		else if (std::strstr(line, "zh") || std::strstr(line, "chinese")) g_lang = EditorLang::Chinese;
	}
	std::fclose(file);
}

void EditorSaveLanguage()
{
	FILE *file = std::fopen(kSettingsFile, "wb");
	if (!file) return;
	std::fprintf(file, "language=%s\n", g_lang == EditorLang::English ? "en" : "zh");
	std::fclose(file);
}
