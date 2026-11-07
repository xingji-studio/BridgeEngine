#include "../editor.h"
#include "../i18n.h"

#include <cstdio>

// Scenes panel: manages the root-level components of the active document as a
// collection of switchable "scenes". A scene is any root component that is not
// marked persistent. Switching scenes hides every non-persistent root except
// the target (one undo step); persistent roots (e.g. HUD, always-on panels)
// keep their own visibility state. Persistent marking is editor-only state and
// is not written to the XML.

void EditorScenesPanel(EditorState &state)
{
	ImGui::Begin(L("Scenes"));

	if (!state.ui) {
		ImGui::TextDisabled("%s", L("No document open."));
		ImGui::End();
		return;
	}

	if (ImGui::Button(L("+ Add Scene"))) {
		char id[64];
		int	 suffix = 1;
		for (;;) {
			snprintf(id, sizeof(id), "scene_%d", suffix);
			if (!bapi_ui_find(state.ui, id)) break;
			suffix++;
		}
		EditorAddComponent(state, BAPI_UI_COMPONENT_PANEL, id, nullptr);
		bapi_ui_component_t comp = bapi_ui_find(state.ui, id);
		if (comp) {
			bapi_rect_t rect = {0.0f, 0.0f, 1024.0f, 768.0f};
			EditorSetComponentRect(state, comp, rect);
			EditorSelectComponent(state, comp);
		}
	}
	ImGui::SameLine();
	ImGui::TextDisabled("%s", L("Click the radio to switch scenes"));

	ImGui::Separator();

	int root_count = bapi_ui_get_root_count(state.ui);
	if (root_count == 0) {
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", L("No root components."));
		ImGui::End();
		return;
	}

	for (int i = 0; i < root_count; i++) {
		bapi_ui_component_t root = bapi_ui_get_root(state.ui, i);
		if (!root) continue;
		bool is_scene = !state.persistent_roots.count(root);
		bool active	 = state.active_scene == root;
		const char *id = EditorComponentId(root);

		ImGui::PushID(root);

		// radio: make this root the active scene (switch)
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, is_scene ? 1.0f : 0.35f);
		bool toggled = ImGui::RadioButton("##scene", active);
		ImGui::PopStyleVar();
		if (toggled && is_scene) EditorSwitchScene(state, root);
		ImGui::SameLine();

		// persistent toggle: keep visible regardless of scene switches
		bool persist = !is_scene;
		if (ImGui::Checkbox("P##persist", &persist)) EditorToggleScenePersistent(state, root);
		ImGui::SameLine();

		// visibility toggle (direct component edit, undoable)
		bool visible = bapi_ui_component_is_visible(root) != 0;
		if (ImGui::Checkbox("V##visible", &visible)) EditorSetComponentVisible(state, root, visible);
		ImGui::SameLine();

		// clicking the label selects the component in the hierarchy
		char label[160];
		const char *type = EditorComponentTypeName(bapi_ui_component_get_type(root));
		snprintf(label, sizeof(label), "%s (%s)", id ? id : "(unnamed)", type);
		if (ImGui::Selectable(label, active)) EditorSelectComponent(state, root);

		ImGui::PopID();
	}

	ImGui::End();
}
