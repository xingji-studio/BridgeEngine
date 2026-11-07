#pragma once

// Minimal i18n layer for the editor UI. Source strings are English; each key is
// looked up in a built-in table and returns the current-language text. Keys that
// are not yet translated fall back to the key itself (English), so partial
// translations degrade gracefully.

enum class EditorLang { Chinese, English };

// Set the current language (persisted to editor_settings.txt on change).
void EditorSetLanguage(EditorLang lang);
EditorLang EditorGetLanguage();

// Current-language text for an (English) source string key.
const char *L(const char *key);

// Load/save the persisted language preference.
void EditorLoadLanguage();
void EditorSaveLanguage();
