#include "../editor.h"
#include "../i18n.h"

// Undo history panel: undo and redo stacks as a clickable list.

void EditorUndoPanel(EditorState &state)
{
	ImGui::Begin(L("History"));

	auto jump_to = [&](int target_depth) {
		while ((int)state.undo_stack.size() > target_depth) EditorUndo(state);
		while ((int)state.undo_stack.size() < target_depth) EditorRedo(state);
	};

	int undo_count = (int)state.undo_stack.size();
	int redo_count = (int)state.redo_stack.size();

	if (undo_count == 0 && redo_count == 0) {
		ImGui::TextDisabled("%s", L("No history yet."));
		ImGui::End();
		return;
	}

	if (ImGui::BeginChild("undo_list")) {
		for (int i = redo_count - 1; i >= 0; i--) {
			std::string label = std::string(">> ") + state.redo_stack[i]->Name();
			if (i == redo_count - 1) label += " [redo]";
			ImGui::PushID(1000 + i);
			if (ImGui::Selectable(label.c_str())) {
				int target = undo_count + (redo_count - i);
				jump_to(target);
			}
			ImGui::PopID();
		}

		ImGui::Separator();

		for (int i = undo_count - 1; i >= 0; i--) {
			std::string label = std::string(state.undo_stack[i]->Name());
			if (i == undo_count - 1) label += " (undo)";
			ImGui::PushID(2000 + i);
			if (ImGui::Selectable(label.c_str())) jump_to(i);
			ImGui::PopID();
		}
	}
	ImGui::EndChild();

	ImGui::End();
}
