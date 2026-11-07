#include "../editor.h"
#include "../i18n.h"
#include "../platform_dialogs.h"

// Shared right-click context menu for the Hierarchy tree and the Viewport.
// Each panel opens a popup named "##editor_ctx" with OpenPopupContextItem /
// OpenPopup and then calls EditorContextMenu() to render the contents.

static void order_items(EditorState &state, bapi_ui_component_t comp)
{
	if (!comp) return;
	if (ImGui::MenuItem(L("Bring to Front"), "Home"))
		EditorReorderComponent(state, comp, ReorderOp::Front);
	if (ImGui::MenuItem(L("Move Up"), "PgUp"))
		EditorReorderComponent(state, comp, ReorderOp::Up);
	if (ImGui::MenuItem(L("Move Down"), "PgDn"))
		EditorReorderComponent(state, comp, ReorderOp::Down);
	if (ImGui::MenuItem(L("Send to Back"), "End"))
		EditorReorderComponent(state, comp, ReorderOp::Back);
}

void EditorContextMenu(EditorState &state, const std::vector<bapi_ui_component_t> &comps)
{
	bool has_selection = !comps.empty();
	bapi_ui_component_t primary = comps.empty() ? nullptr : comps.back();

	if (has_selection && ImGui::MenuItem(L("Copy"), "Ctrl+C")) EditorCopySelection(state);
	if (has_selection && ImGui::MenuItem(L("Duplicate"), "Ctrl+D"))
		EditorDuplicateSelection(state);
	if (has_selection && ImGui::MenuItem(L("Save as Template..."))) {
		bapi_ui_component_t primary =
			state.selection_list.empty() ? nullptr : state.selection_list.back();
		const char *base_id = primary ? EditorComponentId(primary) : nullptr;
		std::string dir	   = EditorTemplateDir(state);
		std::string default_name = (base_id && base_id[0]) ? std::string(base_id) + ".xml"
														   : "template.xml";
		std::string path = EditorSaveFileDialog("Template (*.xml)\0*.xml\0All Files (*.*)\0*.*\0",
												default_name.c_str());
		if (!path.empty()) EditorSaveTemplate(state, path.c_str());
	}
	if (has_selection && ImGui::MenuItem(L("Delete"), "Del"))
		EditorRemoveComponents(state, comps);
	ImGui::Separator();
	if (has_selection && ImGui::MenuItem(L("Group"), "Ctrl+G")) EditorGroupSelection(state);
	if (has_selection && ImGui::MenuItem(L("Ungroup"), "Ctrl+Shift+G"))
		EditorUngroupSelection(state);
	ImGui::Separator();

	if (has_selection && primary) {
		if (ImGui::BeginMenu(L("Order"))) {
			order_items(state, primary);
			ImGui::EndMenu();
		}
		if (comps.size() >= 2 && ImGui::BeginMenu(L("Align"))) {
			if (ImGui::MenuItem(L("Align Left"))) EditorAlignSelection(state, "left");
			if (ImGui::MenuItem(L("Align Center H"))) EditorAlignSelection(state, "center");
			if (ImGui::MenuItem(L("Align Right"))) EditorAlignSelection(state, "right");
			ImGui::Separator();
			if (ImGui::MenuItem(L("Align Top"))) EditorAlignSelection(state, "top");
			if (ImGui::MenuItem(L("Align Middle V"))) EditorAlignSelection(state, "middle");
			if (ImGui::MenuItem(L("Align Bottom"))) EditorAlignSelection(state, "bottom");
			ImGui::EndMenu();
		}
		if (comps.size() >= 2 && ImGui::BeginMenu(L("Distribute"))) {
			if (ImGui::MenuItem(L("Distribute Horizontal"))) EditorDistributeSelection(state, "h");
			if (ImGui::MenuItem(L("Distribute Vertical"))) EditorDistributeSelection(state, "v");
			ImGui::EndMenu();
		}
		if (comps.size() >= 2 && ImGui::BeginMenu(L("Make Same Size"))) {
			if (ImGui::MenuItem(L("Same Width"))) EditorMakeSameSize(state, "w");
			if (ImGui::MenuItem(L("Same Height"))) EditorMakeSameSize(state, "h");
			if (ImGui::MenuItem(L("Same Width && Height"))) EditorMakeSameSize(state, "wh");
			ImGui::EndMenu();
		}
		ImGui::Separator();
	}

	if (has_selection) {
		if (ImGui::MenuItem(L("Lock"))) EditorToggleLocked(state, comps);
		if (ImGui::MenuItem(L("Hide In UI Kit"))) EditorToggleEditorHidden(state, comps);
		ImGui::Separator();
	}

	if (ImGui::MenuItem(L("Select All"), "Ctrl+A", false, state.ui != nullptr))
		EditorSelectAll(state);
}
