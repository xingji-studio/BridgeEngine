#include "../editor.h"
#include "../i18n.h"

#include <cstdio>

static bapi_ui_component_type_t g_palette_types[] = {
	BAPI_UI_COMPONENT_RECT,	   BAPI_UI_COMPONENT_LABEL,	 BAPI_UI_COMPONENT_BUTTON,
	BAPI_UI_COMPONENT_IMAGE,   BAPI_UI_COMPONENT_LINE,	 BAPI_UI_COMPONENT_CIRCLE,
	BAPI_UI_COMPONENT_POLYGON, BAPI_UI_COMPONENT_BORDER, BAPI_UI_COMPONENT_PROGRESS,
	BAPI_UI_COMPONENT_SEPARATOR, BAPI_UI_COMPONENT_PANEL, BAPI_UI_COMPONENT_CONTAINER,
	BAPI_UI_COMPONENT_ROW,	   BAPI_UI_COMPONENT_COLUMN, BAPI_UI_COMPONENT_GRID,
	BAPI_UI_COMPONENT_CHECKBOX, BAPI_UI_COMPONENT_RADIO, BAPI_UI_COMPONENT_TOGGLE,
	BAPI_UI_COMPONENT_SLIDER,  BAPI_UI_COMPONENT_INPUT,	 BAPI_UI_COMPONENT_SELECT,
	BAPI_UI_COMPONENT_LIST,	   BAPI_UI_COMPONENT_SCROLL, BAPI_UI_COMPONENT_TAB,
	BAPI_UI_COMPONENT_CANVAS,  BAPI_UI_COMPONENT_NINE_PATCH, BAPI_UI_COMPONENT_TOOLTIP,
	BAPI_UI_COMPONENT_MODAL,   BAPI_UI_COMPONENT_POPUP,
};

static int unique_suffix(bapi_ui_t ui, const char *base)
{
	int		suffix = 1;
	char	id[128];
	for (;;) {
		snprintf(id, sizeof(id), "%s_%d", base, suffix);
		if (bapi_ui_find(ui, id) == nullptr) return suffix;
		suffix++;
	}
}

static void palette_templates_section(EditorState &state)
{
	std::vector<std::string> templates = EditorListTemplates(state);
	if (templates.empty()) return;
	ImGui::Separator();
	ImGui::TextUnformatted("Templates");
	if (ImGui::BeginChild("palette_templates")) {
		for (const std::string &path : templates) {
			std::string name = path;
			size_t slash = name.find_last_of("/\\");
			if (slash != std::string::npos) name = name.substr(slash + 1);
			ImGui::PushID(path.c_str());
			if (ImGui::Selectable(name.c_str(), false)) {
				EditorLoadTemplate(state, path.c_str());
			}
			if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
				ImGui::SetDragDropPayload("BAPI_UI_TEMPLATE", path.c_str(), path.size() + 1);
				ImGui::Text("%s", name.c_str());
				ImGui::EndDragDropSource();
			}
			ImGui::PopID();
		}
	}
	ImGui::EndChild();
}

void EditorPalettePanel(EditorState &state)
{
	ImGui::Begin(L("Palette"));

	ImGui::TextUnformatted("Click to add to:");
	if (state.selection && EditorIsContainerType(bapi_ui_component_get_type(state.selection))) {
		const char *id	= EditorComponentId(state.selection);
		ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "  %s (selected)", id ? id : "?");
	} else {
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "  root level");
	}
	ImGui::Separator();

	if (ImGui::BeginChild("palette_list")) {
		for (size_t i = 0; i < sizeof(g_palette_types) / sizeof(g_palette_types[0]); i++) {
			bapi_ui_component_type_t type = g_palette_types[i];
			const char *name = EditorComponentTypeName(type);
			ImGui::PushID((int)i);
			if (ImGui::Selectable(name, false)) {
				char id[128];
				snprintf(id, sizeof(id), "%s_%d", name, unique_suffix(state.ui, name));
				bapi_ui_component_t parent =
					state.selection &&
							EditorIsContainerType(bapi_ui_component_get_type(state.selection))
						? state.selection
						: nullptr;
				EditorAddComponent(state, type, id, parent);
			}
			if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
				int payload_type = (int)type;
				ImGui::SetDragDropPayload("BAPI_UI_PALETTE", &payload_type, sizeof(payload_type));
				ImGui::Text(L("Add %s"), name);
				ImGui::EndDragDropSource();
			}
			ImGui::PopID();
		}
	}
	ImGui::EndChild();

	palette_templates_section(state);
	ImGui::End();
}
