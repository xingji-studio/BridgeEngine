#include "../editor.h"
#include "../i18n.h"
#include "../platform_dialogs.h"
#include "../project_file.h"

static const char *kUiFileFilter = "UI Documents (*.xml)\0*.xml\0All Files (*.*)\0*.*\0";

// Save the active document, opening a Save As dialog if it has no path yet.
// Returns false if the user cancelled the dialog or the save failed.
static bool save_active_with_dialog(EditorState &state)
{
	if (state.filepath.empty()) {
		std::string path = EditorSaveFileDialog(kUiFileFilter, "editor_out.xml");
		if (path.empty()) return false;
		return EditorSaveFile(state, path.c_str());
	}
	return EditorSaveFile(state, state.filepath.c_str());
}

// Run the pending action. If save_changes is set, save the active document
// first; a cancelled/failed save aborts the action.
static void resolve_pending(EditorState &state, bool save_changes)
{
	if (save_changes && !save_active_with_dialog(state)) {
		state.pending_action	  = PendingAction::None;
		state.pending_doc_index	  = -1;
		state.pending_path.clear();
		return;
	}
	PendingAction action	   = state.pending_action;
	int			  doc_index	   = state.pending_doc_index;
	std::string	  path		   = state.pending_path;
	state.pending_action	   = PendingAction::None;
	state.pending_doc_index	   = -1;
	state.pending_path.clear();
	switch (action) {
	case PendingAction::NewDocument:
		EditorNewDocument(state);
		break;
	case PendingAction::OpenFile:
		EditorLoadFile(state, path.c_str());
		break;
	case PendingAction::OpenProject: {
		std::string error;
		EditorOpenProject(state, path.c_str(), error);
		break;
	}
	case PendingAction::Quit:
		state.wants_quit = true;
		break;
	case PendingAction::CloseDoc:
		EditorCloseDocument(state, doc_index);
		break;
	case PendingAction::None:
		break;
	}
}

static void unsaved_modal(EditorState &state)
{
	if (state.pending_action != PendingAction::None) {
		ImGui::OpenPopup(L("Unsaved Changes"));
	}
	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	if (ImGui::BeginPopupModal(L("Unsaved Changes"), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		std::string title =
			state.filepath.empty() ? "Untitled" : EditorDocumentTitle(state, state.active_doc);
		ImGui::Text(L("Save changes to %s?"), title.c_str());
		if (ImGui::Button(L("Save"))) {
			resolve_pending(state, true);
		}
		ImGui::SameLine();
		if (ImGui::Button(L("Discard"))) {
			resolve_pending(state, false);
		}
		ImGui::SameLine();
		if (ImGui::Button(L("Cancel"))) {
			state.pending_action	 = PendingAction::None;
			state.pending_doc_index	 = -1;
			state.pending_path.clear();
		}
		ImGui::EndPopup();
	}
}

void EditorDocumentsPanel(EditorState &state)
{
	ImGui::Begin(L("Documents"));

	int close_index = -1;
	if (ImGui::BeginTabBar(
			"doc_tabs",
			ImGuiTabBarFlags_AutoSelectNewTabs | ImGuiTabBarFlags_Reorderable |
				ImGuiTabBarFlags_FittingPolicyScroll)) {
		for (int i = 0; i < (int)state.documents.size(); i++) {
			bool open			  = true;
			// Only programmatic activation (open/new/project) may force the
			// selection. Passing SetSelected every frame overrides a click on a
			// lower-index tab, so the user could never switch back. ImGui's own
			// click handling selects the clicked tab; we then follow below.
			ImGuiTabItemFlags flags =
				(state.tab_select_request == i) ? ImGuiTabItemFlags_SetSelected : 0;
			// The display title changes with the dirty flag ("*"), so a plain
			// label would be treated as a new tab on every dirty toggle. Keep a
			// stable hidden id ("##doc_<i>") so ImGui tracks one tab per index.
			std::string title = EditorDocumentTitle(state, i);
			std::string label = title + "##doc_" + std::to_string(i);
			if (ImGui::BeginTabItem(label.c_str(), &open, flags)) {
				// This tab is the one ImGui currently considers selected.
				// Sync the active document with it (user click or reorder).
				if (i != state.active_doc) EditorActivateDocument(state, i);
				ImGui::EndTabItem();
			}
			if (!open) close_index = i;
		}
		ImGui::EndTabBar();
	}
	// Consume the one-shot selection request now that it has been applied.
	state.tab_select_request = -1;

	ImGui::TextDisabled("%s", L("Drag components from the palette or use File > Open to add documents."));
	ImGui::End();

	// Handle a tab close request after the loop so activation inside the loop
	// cannot disturb iteration.
	if (close_index >= 0) {
		if (close_index == state.active_doc) {
			if (state.dirty) {
				state.pending_action	 = PendingAction::CloseDoc;
				state.pending_doc_index	 = close_index;
			} else {
				EditorCloseDocument(state, close_index);
			}
		} else {
			if (EditorDocumentIsDirty(state, close_index)) {
				EditorActivateDocument(state, close_index);
				state.pending_action	 = PendingAction::CloseDoc;
				state.pending_doc_index	 = close_index;
			} else {
				EditorCloseDocument(state, close_index);
			}
		}
	}

	unsaved_modal(state);
}
