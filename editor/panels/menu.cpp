#include "../editor.h"
#include "../build_run.h"
#include "../i18n.h"
#include "../platform_dialogs.h"
#include "../project_file.h"
#include "../project_templates.h"

#include <SDL3/SDL.h>

#ifndef BRIDGEENGINE_SOURCE_DIR
#define BRIDGEENGINE_SOURCE_DIR ""
#endif

#include <cstdio>
#include <cstring>

static const char *kUiFileFilter = "UI Documents (*.xml)\0*.xml\0All Files (*.*)\0*.*\0";
static const char *kProjectFileFilter =
	"BridgeEngine Projects (*.bep)\0*.bep\0All Files (*.*)\0*.*\0";

static void open_project(EditorState &state, const std::string &path)
{
	if (path.empty()) return;
	if (state.dirty) {
		state.pending_action = PendingAction::OpenProject;
		state.pending_path = path;
		return;
	}
	std::string error;
	EditorOpenProject(state, path.c_str(), error);
}

static void file_menu(EditorState &state)
{
	if (!ImGui::BeginMenu(L("File"))) return;

	if (ImGui::MenuItem(L("New"), "Ctrl+N")) {
		if (state.dirty)
			state.pending_action = PendingAction::NewDocument;
		else {
			EditorNewDocument(state);
			state.show_welcome = false;
		}
	}
	if (ImGui::MenuItem(L("New Project..."))) {
		state.new_project_open = true;
		state.new_project_ok	= false;
		state.new_project_message[0] = '\0';
	}
	if (ImGui::MenuItem(L("Open Project...")))
		open_project(state, EditorOpenFileDialog(kProjectFileFilter, ""));
	if (ImGui::BeginMenu(L("Recent Projects"), !state.recent_projects.empty())) {
		for (const std::string &path : state.recent_projects) {
			if (ImGui::MenuItem(path.c_str())) open_project(state, path);
		}
		ImGui::EndMenu();
	}
	if (ImGui::MenuItem(L("Open..."), "Ctrl+O")) {
		std::string path = EditorOpenFileDialog(kUiFileFilter, "assets/ui/demo.xml");
		if (!path.empty()) {
			if (state.dirty) {
				state.pending_action = PendingAction::OpenFile;
				state.pending_path	 = path;
			} else {
				EditorLoadFile(state, path.c_str());
			}
		}
	}
	if (ImGui::BeginMenu(L("Recent Files"), !state.recent_files.empty())) {
		for (const std::string &path : state.recent_files) {
			if (ImGui::MenuItem(path.c_str())) {
				if (state.dirty) {
					state.pending_action = PendingAction::OpenFile;
					state.pending_path	 = path;
				} else {
					EditorLoadFile(state, path.c_str());
				}
			}
		}
		ImGui::EndMenu();
	}
	if (ImGui::MenuItem(L("Save"), "Ctrl+S")) {
		if (state.filepath.empty()) {
			std::string path = EditorSaveFileDialog(kUiFileFilter, "editor_out.xml");
			if (!path.empty()) EditorSaveFile(state, path.c_str());
		} else {
			EditorSaveFile(state, state.filepath.c_str());
		}
	}
	if (ImGui::MenuItem(L("Save As..."), "Ctrl+Shift+S")) {
		std::string path = EditorSaveFileDialog(kUiFileFilter, state.filepath.c_str());
		if (!path.empty()) EditorSaveFile(state, path.c_str());
	}
	ImGui::Separator();
	if (ImGui::MenuItem(L("Save Project"), "Ctrl+Shift+P", false, !state.project_path.empty())) {
		std::string error;
		if (!EditorSaveProject(state, error)) {
			std::lock_guard<std::mutex> lock(state.build_log_mutex);
			state.build_log += "save project: " + error + "\n";
		}
	}
	if (ImGui::MenuItem(L("UI Encryption Key..."))) {
		state.key_dialog_open = true;
		// keep the current key so the dialog shows it on reopen
		strncpy(state.key_dialog_buf, state.ui_key, sizeof(state.key_dialog_buf) - 1);
		state.key_dialog_buf[sizeof(state.key_dialog_buf) - 1] = '\0';
	}
	ImGui::Separator();
	if (ImGui::MenuItem(L("Close"), "Ctrl+W", false, state.active_doc >= 0)) {
		if (state.dirty) {
			state.pending_action	 = PendingAction::CloseDoc;
			state.pending_doc_index	 = state.active_doc;
		} else {
			EditorCloseDocument(state, state.active_doc);
		}
	}
	if (ImGui::MenuItem(L("Close All"))) {
		int first_dirty = -1;
		for (int i = 0; i < (int)state.documents.size(); i++) {
			if (EditorDocumentIsDirty(state, i)) {
				first_dirty = i;
				break;
			}
		}
		if (first_dirty >= 0) {
			EditorActivateDocument(state, first_dirty);
			state.pending_action	 = PendingAction::CloseDoc;
			state.pending_doc_index	 = first_dirty;
		} else {
			for (int i = (int)state.documents.size() - 1; i >= 0; i--)
				EditorCloseDocument(state, i);
		}
	}
	ImGui::Separator();
	if (ImGui::MenuItem(L("Quit"), "Alt+F4")) {
		if (state.dirty)
			state.pending_action = PendingAction::Quit;
		else
			state.wants_quit = true;
	}
	ImGui::EndMenu();
}

static void project_menu(EditorState &state)
{
	if (!ImGui::BeginMenu(L("Project"))) return;

	if (ImGui::MenuItem(L("Build"), "Ctrl+Shift+B", false, EditorCanBuild(state)))
		EditorBuildProject(state);
	if (ImGui::MenuItem(L("Run"), "Ctrl+F5", false, EditorCanRun(state)))
		EditorRunProject(state);
	if (ImGui::MenuItem(L("Stop"), "Shift+F5", false, EditorCanStopRun(state)))
		EditorStopRunProject(state);
	ImGui::Separator();
	if (ImGui::BeginMenu(L("Build Configuration"))) {
		static const char *kConfigs[] = {"Debug", "Release", "RelWithDebInfo", "MinSizeRel"};
		for (const char *cfg : kConfigs) {
			if (ImGui::MenuItem(cfg, nullptr, state.build_config == cfg)) {
				state.build_config = cfg;
			}
		}
		ImGui::EndMenu();
	}

	ImGui::EndMenu();
}

static void edit_menu(EditorState &state)
{
	if (!ImGui::BeginMenu(L("Edit"))) return;

	if (ImGui::MenuItem(L("Undo"), "Ctrl+Z", false, EditorCanUndo(state))) EditorUndo(state);
	if (ImGui::MenuItem(L("Redo"), "Ctrl+Y", false, EditorCanRedo(state))) EditorRedo(state);
	ImGui::Separator();
	if (ImGui::MenuItem(L("Copy"), "Ctrl+C", false, !state.selection_list.empty()))
		EditorCopySelection(state);
	if (ImGui::MenuItem(L("Cut"), "Ctrl+X", false, !state.selection_list.empty()))
		EditorCutSelection(state);
	if (ImGui::MenuItem(L("Paste"), "Ctrl+V", false, !state.clipboard.empty()))
		EditorPasteClipboard(state);
	if (ImGui::MenuItem(L("Duplicate"), "Ctrl+D", false, !state.selection_list.empty()))
		EditorDuplicateSelection(state);
	ImGui::Separator();
	if (ImGui::MenuItem(L("Group"), "Ctrl+G", false, state.selection_list.size() >= 2))
		EditorGroupSelection(state);
	if (ImGui::MenuItem(L("Ungroup"), "Ctrl+Shift+G", false, !state.selection_list.empty()))
		EditorUngroupSelection(state);
	ImGui::Separator();
	if (ImGui::MenuItem(L("Select All"), "Ctrl+A", false, state.ui != nullptr))
		EditorSelectAll(state);
	if (ImGui::MenuItem(L("Delete Selection"), "Del", false, !state.selection_list.empty()))
		EditorRemoveComponents(state, state.selection_list);
	ImGui::Separator();
	if (ImGui::BeginMenu(L("Order"), !state.selection_list.empty())) {
		if (ImGui::MenuItem(L("Bring to Front"), "Home"))
			EditorReorderComponent(state, state.selection, ReorderOp::Front);
		if (ImGui::MenuItem(L("Move Up"), "PgUp"))
			EditorReorderComponent(state, state.selection, ReorderOp::Up);
		if (ImGui::MenuItem(L("Move Down"), "PgDn"))
			EditorReorderComponent(state, state.selection, ReorderOp::Down);
		if (ImGui::MenuItem(L("Send to Back"), "End"))
			EditorReorderComponent(state, state.selection, ReorderOp::Back);
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu(L("Align"), state.selection_list.size() >= 2)) {
		if (ImGui::MenuItem(L("Align Left"))) EditorAlignSelection(state, "left");
		if (ImGui::MenuItem(L("Align Center H"))) EditorAlignSelection(state, "center");
		if (ImGui::MenuItem(L("Align Right"))) EditorAlignSelection(state, "right");
		ImGui::Separator();
		if (ImGui::MenuItem(L("Align Top"))) EditorAlignSelection(state, "top");
		if (ImGui::MenuItem(L("Align Middle V"))) EditorAlignSelection(state, "middle");
		if (ImGui::MenuItem(L("Align Bottom"))) EditorAlignSelection(state, "bottom");
	}
	if (ImGui::BeginMenu(L("Distribute"), state.selection_list.size() >= 3)) {
		if (ImGui::MenuItem(L("Distribute Horizontal"))) EditorDistributeSelection(state, "h");
		if (ImGui::MenuItem(L("Distribute Vertical"))) EditorDistributeSelection(state, "v");
	}
	if (ImGui::BeginMenu(L("Make Same Size"), state.selection_list.size() >= 2)) {
		if (ImGui::MenuItem(L("Same Width"))) EditorMakeSameSize(state, "w");
		if (ImGui::MenuItem(L("Same Height"))) EditorMakeSameSize(state, "h");
		if (ImGui::MenuItem(L("Same Width && Height"))) EditorMakeSameSize(state, "wh");
	}
	ImGui::EndMenu();
}

static void view_menu(EditorState &state)
{
	if (!ImGui::BeginMenu(L("View"))) return;

	if (ImGui::MenuItem(L("Zoom In"), "Ctrl+="))
		EditorSetViewCamera(state, state.view_scale * 1.25f, state.view_offset_x,
							state.view_offset_y);
	if (ImGui::MenuItem(L("Zoom Out"), "Ctrl+-"))
		EditorSetViewCamera(state, state.view_scale * 0.8f, state.view_offset_x,
							state.view_offset_y);
	if (ImGui::MenuItem(L("Reset View"), "Ctrl+0"))
		EditorSetViewCamera(state, 1.0f, 0.0f, 0.0f);
	ImGui::Separator();
	if (ImGui::MenuItem(L("Reset Layout"))) state.reset_layout_requested = true;
	ImGui::Separator();
	if (ImGui::BeginMenu(L("Language"))) {
		EditorLang current = EditorGetLanguage();
		if (ImGui::MenuItem(L("简体中文"), nullptr, current == EditorLang::Chinese)) {
			if (current != EditorLang::Chinese) {
				EditorSetLanguage(EditorLang::Chinese);
				state.reset_layout_requested = true;
			}
		}
		if (ImGui::MenuItem(L("English"), nullptr, current == EditorLang::English)) {
			if (current != EditorLang::English) {
				EditorSetLanguage(EditorLang::English);
				state.reset_layout_requested = true;
			}
		}
		ImGui::EndMenu();
	}
	ImGui::EndMenu();
}

static void prefill_new_project(EditorState &state)
{
	if (state.new_project_name[0] == '\0')
		std::strncpy(state.new_project_name, "MyGame", sizeof(state.new_project_name));
	if (state.new_project_dir[0] == '\0') {
		char *cwd = SDL_GetCurrentDirectory();
		if (cwd) {
			std::snprintf(state.new_project_dir, sizeof(state.new_project_dir), "%s", cwd);
			SDL_free(cwd);
		}
	}
	if (state.new_project_engine[0] == '\0')
		std::snprintf(state.new_project_engine, sizeof(state.new_project_engine), "%s",
					  BRIDGEENGINE_SOURCE_DIR);
}

void EditorNewProjectDialog(EditorState &state)
{
	if (!state.new_project_open) return;
	prefill_new_project(state);

	// OpenPopup must run under the same window context that BeginPopupModal
	// uses, otherwise the popup ID does not match (ImGui mixes in the current
	// window ID). Both calls therefore happen here, not in the welcome screen
	// or the menu bar. We pass a null p_open: when the popup is not open yet
	// BeginPopupModal resets a non-null *p_open to false, which would drop the
	// request before the popup ever renders.
	if (!ImGui::IsPopupOpen(L("New Project")))
		ImGui::OpenPopup(L("New Project"));

	ImGui::SetNextWindowSize(ImVec2(560, 0), ImGuiCond_Appearing);
	if (!ImGui::BeginPopupModal(L("New Project"), nullptr,
								ImGuiWindowFlags_AlwaysAutoResize))
		return;

	// Esc closes the popup without telling us; sync the state so the popup is
	// not re-opened on the next frame.
	if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
		state.new_project_open = false;
		ImGui::CloseCurrentPopup();
	}

	ImGui::TextWrapped("%s", L("wizard.hint"));
	ImGui::Separator();

	ImGui::Text("%s", L("wizard.step1"));
	ImGui::InputText(L("Project Name"), state.new_project_name, sizeof(state.new_project_name));
	ImGui::SetItemTooltip("%s", L("wizard.name_tooltip"));
	ImGui::Spacing();

	ImGui::Text("%s", L("wizard.step2"));
	ImGui::InputText(L("Parent Directory"), state.new_project_dir,
					 sizeof(state.new_project_dir));
	ImGui::SameLine();
	if (ImGui::Button(L("Browse..."))) {
		std::string dir = EditorBrowseForFolder(L("wizard.open_parent"));
		if (!dir.empty())
			std::snprintf(state.new_project_dir, sizeof(state.new_project_dir), "%s", dir.c_str());
	}
	char preview[1024];
	std::snprintf(preview, sizeof(preview), "%s\\%s", state.new_project_dir,
				  state.new_project_name);
	ImGui::TextDisabled("%s", preview);
	ImGui::Spacing();

	ImGui::Text("%s", L("wizard.step3"));
	ImGui::InputText(L("Engine Source (optional)"), state.new_project_engine,
					 sizeof(state.new_project_engine));
	ImGui::SameLine();
	std::string engine_browse = std::string(L("Browse...")) + "##engine";
	if (ImGui::Button(engine_browse.c_str())) {
		std::string dir = EditorBrowseForFolder(L("wizard.open_engine"));
		if (!dir.empty())
			std::snprintf(state.new_project_engine, sizeof(state.new_project_engine), "%s",
						  dir.c_str());
	}
	ImGui::SetItemTooltip("%s", L("wizard.engine_tooltip"));
	ImGui::Spacing();

	ImGui::TextDisabled("%s", L("wizard.files"));
	ImGui::BulletText("CMakeLists.txt");
	ImGui::BulletText("main.c");
	ImGui::BulletText("assets/text/font.ttf");
	ImGui::BulletText("ui/menu.xml  ui/game.xml  ui/settings.xml");
	ImGui::BulletText("%s.bep", state.new_project_name);

	if (state.new_project_message[0]) {
		ImVec4 color = state.new_project_ok ? ImVec4(0.3f, 0.9f, 0.4f, 1.0f)
											: ImVec4(0.9f, 0.3f, 0.3f, 1.0f);
		ImGui::TextColored(color, "%s", state.new_project_message);
	}

	ImGui::Separator();
	bool created = false;
	if (ImGui::Button(L("Create"), ImVec2(120, 0))) {
		std::string error;
		std::string project_path;
		if (EditorCreateProject(state.new_project_name, state.new_project_dir,
								state.new_project_engine, error, &project_path)) {
			std::string open_error;
			state.new_project_ok = EditorOpenProject(state, project_path.c_str(), open_error);
			std::snprintf(state.new_project_message, sizeof(state.new_project_message),
						  state.new_project_ok ? L("wizard.created") : L("wizard.created_fail"),
						  state.new_project_ok ? project_path.c_str() : open_error.c_str());
			created = state.new_project_ok;
		} else {
			state.new_project_ok = false;
			std::snprintf(state.new_project_message, sizeof(state.new_project_message), "%s",
						  error.c_str());
		}
	}
	ImGui::SameLine();
	if (ImGui::Button(L("Cancel"), ImVec2(120, 0)) || created) {
		state.new_project_open = false;
		ImGui::CloseCurrentPopup();
	}

	ImGui::EndPopup();
}

void EditorKeyDialog(EditorState &state)
{
	if (!state.key_dialog_open) return;
	ImGui::OpenPopup(L("UI Encryption Key"));
	ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_Appearing);
	if (!ImGui::BeginPopupModal(L("UI Encryption Key"), nullptr,
								ImGuiWindowFlags_AlwaysAutoResize))
		return;
	ImGui::TextWrapped(
		"%s",
		L("key.hint"));
	ImGui::InputText(L("Key"), state.key_dialog_buf, sizeof(state.key_dialog_buf));
	if (ImGui::IsItemHovered()) ImGui::SetItemTooltip("%s", L("key.tooltip"));
	ImGui::Spacing();
	if (ImGui::Button(L("Apply"), ImVec2(120, 0))) {
		// An empty key switches back to plain XML documents.
		strncpy(state.ui_key, state.key_dialog_buf, sizeof(state.ui_key) - 1);
		state.ui_key[sizeof(state.ui_key) - 1] = '\0';
		// Persist immediately so the project build picks it up without
		// requiring a Save Project first.
		EditorSyncUiKeyFile(state);
		state.key_dialog_open				   = false;
		ImGui::CloseCurrentPopup();
	}
	ImGui::SameLine();
	if (ImGui::Button(L("Clear"), ImVec2(120, 0))) {
		state.ui_key[0]		   = '\0';
		state.key_dialog_buf[0] = '\0';
		EditorSyncUiKeyFile(state);
		state.key_dialog_open   = false;
		ImGui::CloseCurrentPopup();
	}
	ImGui::SameLine();
	if (ImGui::Button(L("Cancel"), ImVec2(120, 0))) {
		state.key_dialog_open = false;
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndPopup();
}

void EditorMenuPanel(EditorState &state)
{
	// Menu bar sits below the custom title bar (30px).
	ImGuiViewport *viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + 30.0f));
	ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, 0.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::Begin("##MainMenuBar", nullptr,
				 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
					 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
					 ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings |
					 ImGuiWindowFlags_MenuBar);
	if (ImGui::BeginMenuBar()) {
		file_menu(state);
		edit_menu(state);
		view_menu(state);
		project_menu(state);
		ImGui::Separator();
		const char *title = state.filepath.empty() ? "[untitled]" : state.filepath.c_str();
		ImGui::Text("%s%s", title, state.dirty ? " *" : "");
		ImGui::EndMenuBar();
	}
	ImGui::End();
	ImGui::PopStyleVar(3);
}
