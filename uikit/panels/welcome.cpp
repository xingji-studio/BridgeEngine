#include "../editor.h"
#include "../i18n.h"
#include "../platform_dialogs.h"

#include <cstdio>
#include <string>

static const char *kProjectFileFilter =
	"BridgeEngine Projects (*.bep)\0*.bep\0All Files (*.*)\0*.*\0";

static std::string project_title(const std::string &path)
{
	size_t slash = path.find_last_of("\\/");
	std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
	if (name.size() > 4 && name.substr(name.size() - 4) == ".bep") name.resize(name.size() - 4);
	return name;
}

static void open_welcome_project(EditorState &state, const std::string &path)
{
	std::string error;
	if (!EditorOpenProject(state, path.c_str(), error)) {
		state.new_project_ok = false;
		std::snprintf(state.new_project_message, sizeof(state.new_project_message), "%s",
					  error.c_str());
	}
}

void EditorWelcomePanel(EditorState &state)
{
	if (!state.show_welcome) return;

	ImGuiViewport *viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	// Fixed width, auto height. A full-width button (-1.0f) combined with
	// AlwaysAutoResize would feed back into the window width each frame
	// (button widens -> window widens -> button widens ...), making the
	// window jitter. Fixing the width keeps the layout stable.
	ImGui::SetNextWindowSize(ImVec2(560.0f, 0.0f), ImGuiCond_Appearing);
	if (!ImGui::Begin(L("Welcome"), &state.show_welcome)) {
		ImGui::End();
		return;
	}

	ImGui::Text("%s", L("BridgeEngine UI Kit"));
	ImGui::TextDisabled("%s", L("Open a project or start a new one."));
	ImGui::Separator();
	if (ImGui::Button(L("New Project..."), ImVec2(-1.0f, 38.0f))) {
		state.new_project_open = true;
		state.new_project_ok = false;
		state.new_project_message[0] = '\0';
	}

	ImGui::Spacing();
	ImGui::Text("%s", L("Recent Projects"));
	if (state.recent_projects.empty()) {
		ImGui::TextDisabled("%s", L("No recent projects."));
	} else {
		for (const std::string &path : state.recent_projects) {
			std::string label = project_title(path);
			if (ImGui::Selectable(label.c_str())) open_welcome_project(state, path);
			if (ImGui::IsItemHovered()) ImGui::SetItemTooltip("%s", path.c_str());
		}
	}
	if (state.new_project_message[0] && !state.new_project_ok)
		ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "%s", state.new_project_message);

	ImGui::Separator();
	if (ImGui::Button(L("Open Project..."), ImVec2(-1.0f, 32.0f))) {
		std::string path = EditorOpenFileDialog(kProjectFileFilter, "");
		if (!path.empty()) open_welcome_project(state, path);
	}
	if (ImGui::Button(L("Open UI Document..."), ImVec2(-1.0f, 32.0f))) {
		std::string path = EditorOpenFileDialog(
			"UI Documents (*.xml)\0*.xml\0All Files (*.*)\0*.*\0", "");
		if (!path.empty()) {
			state.show_welcome = false;
			if (EditorLoadFile(state, path.c_str()) && state.documents.size() > 1)
				EditorCloseDocument(state, 0);
		}
	}

	ImGui::End();
}
