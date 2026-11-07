#include "../editor.h"
#include "../i18n.h"
#include "../platform_dialogs.h"

#include <cstdio>
#include <cstring>

static const char *kMediaFileFilter =
	"Media Files\0*.png;*.jpg;*.jpeg;*.bmp;*.mp4;*.webp\0Images\0*.png;*.jpg;*.jpeg;*.bmp\0"
	"Videos\0*.mp4\0All Files (*.*)\0*.*\0";

static bool is_src_type(bapi_ui_component_type_t type)
{
	switch (type) {
	case BAPI_UI_COMPONENT_IMAGE:
	case BAPI_UI_COMPONENT_VIDEO:
	case BAPI_UI_COMPONENT_NINE_PATCH:
	case BAPI_UI_COMPONENT_ANIMATION:
		return true;
	default:
		return false;
	}
}

static void src_editor(EditorState &state, bapi_ui_component_t comp)
{
	const char *src = bapi_ui_component_get_src(comp);
	ImGui::TextUnformatted(L("Src"));
	ImGui::SameLine();
	if (src && src[0])
		ImGui::TextWrapped("%s", src);
	else
		ImGui::TextDisabled("%s", L("(none)"));
	if (ImGui::Button(L("Browse..."))) {
		std::string path = EditorOpenFileDialog(kMediaFileFilter, src && src[0] ? src : "assets/");
		if (!path.empty()) EditorSetComponentSrc(state, comp, path.c_str());
	}
}

static void color_role_editor(EditorState &state, bapi_ui_component_t comp, const char *label,
							  bapi_ui_color_role_t role)
{
	bapi_color_t color;
	if (bapi_ui_component_get_color(comp, role, &color) != 0) return;
	float c[4] = {color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f};
	if (ImGui::ColorEdit4(label, c,
						  ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar)) {
		bapi_color_t nc;
		nc.r = (uint8_t)(c[0] * 255.0f + 0.5f);
		nc.g = (uint8_t)(c[1] * 255.0f + 0.5f);
		nc.b = (uint8_t)(c[2] * 255.0f + 0.5f);
		nc.a = (uint8_t)(c[3] * 255.0f + 0.5f);
		EditorSetComponentColor(state, comp, role, nc);
	}
}

static void rect_editor(EditorState &state, bapi_ui_component_t comp)
{
	bapi_rect_t rect;
	bapi_ui_component_get_rect(comp, &rect);
	bool changed = false;
	bapi_rect_t next = rect;

	if (ImGui::DragFloat(L("X"), &next.x, 1.0f)) changed = true;
	if (ImGui::DragFloat(L("Y"), &next.y, 1.0f)) changed = true;
	if (ImGui::DragFloat(L("W"), &next.w, 1.0f)) changed = true;
	if (ImGui::DragFloat(L("H"), &next.h, 1.0f)) changed = true;

	if (changed && (next.x != rect.x || next.y != rect.y || next.w != rect.w || next.h != rect.h)) {
		EditorSetComponentRect(state, comp, next);
	}
}

// Anchor presets for relative-positioned children. Computes a rect in the
// parent's local space (offset from parent origin, same size preserved unless
// "Fill" is chosen).
static void anchor_editor(EditorState &state, bapi_ui_component_t comp)
{
	bapi_ui_component_t parent = bapi_ui_component_get_parent(comp);
	if (!parent || !bapi_ui_component_get_relative(comp)) return;

	bapi_rect_t pr, cr;
	bapi_ui_component_get_rect(parent, &pr);
	bapi_ui_component_get_rect(comp, &cr);

	struct Anchor {
		const char *label;
		float		x, y, w, h;
		bool		use_wh;
	};
	const float pw = pr.w, ph = pr.h;
	const float cw = cr.w, ch = cr.h;
	Anchor anchors[] = {
		{"Fill", 0.0f, 0.0f, pw, ph, true},
		{"Top-Left", 0.0f, 0.0f, cw, ch, false},
		{"Top-Center", (pw - cw) * 0.5f, 0.0f, cw, ch, false},
		{"Top-Right", pw - cw, 0.0f, cw, ch, false},
		{"Mid-Left", 0.0f, (ph - ch) * 0.5f, cw, ch, false},
		{"Center", (pw - cw) * 0.5f, (ph - ch) * 0.5f, cw, ch, false},
		{"Mid-Right", pw - cw, (ph - ch) * 0.5f, cw, ch, false},
		{"Bottom-Left", 0.0f, ph - ch, cw, ch, false},
		{"Bottom-Center", (pw - cw) * 0.5f, ph - ch, cw, ch, false},
		{"Bottom-Right", pw - cw, ph - ch, cw, ch, false},
	};

	ImGui::TextUnformatted("Anchors (relative to parent)");
	for (int i = 0; i < (int)(sizeof(anchors) / sizeof(anchors[0])); i++) {
		if (i > 0 && i % 3 != 0) ImGui::SameLine();
		if (ImGui::Button(anchors[i].label)) {
			bapi_rect_t next;
			next.x = anchors[i].x;
			next.y = anchors[i].y;
			next.w = anchors[i].use_wh ? anchors[i].w : cr.w;
			next.h = anchors[i].use_wh ? anchors[i].h : cr.h;
			EditorSetComponentRect(state, comp, next);
		}
	}
	ImGui::TextDisabled("Offsets are relative to the parent's top-left.");
}

static void bool_editor(EditorState &state, bapi_ui_component_t comp, const char *label, BoolField field,
						bool value)
{
	bool next = value;
	if (ImGui::Checkbox(label, &next) && next != value) {
		switch (field) {
		case BoolField::Checked:
			EditorSetComponentChecked(state, comp, next);
			break;
		case BoolField::Relative:
			EditorSetComponentRelative(state, comp, next);
			break;
		case BoolField::Visible:
			EditorSetComponentVisible(state, comp, next);
			break;
		case BoolField::Enabled:
			EditorSetComponentEnabled(state, comp, next);
			break;
		}
	}
}

static void float_field(EditorState &state, bapi_ui_component_t comp, const char *label, float value,
						float speed, float min, float max, NumericField field)
{
	float next = value;
	if (ImGui::DragFloat(label, &next, speed, min, max) && next != value) {
		switch (field) {
		case NumericField::MinValue:
			EditorSetComponentMin(state, comp, next);
			break;
		case NumericField::MaxValue:
			EditorSetComponentMax(state, comp, next);
			break;
		case NumericField::Step:
			EditorSetComponentStep(state, comp, next);
			break;
		case NumericField::Radius:
			EditorSetComponentRadius(state, comp, next);
			break;
		}
	}
}

static void slider_value_editor(EditorState &state, bapi_ui_component_t comp)
{
	float value = bapi_ui_component_get_value(comp);
	float min	= bapi_ui_component_get_min_value(comp);
	float max	= bapi_ui_component_get_max_value(comp);
	float next	= value;
	if (ImGui::SliderFloat("Value", &next, min, max) && next != value) {
		EditorSetComponentValue(state, comp, next);
	}
	float_field(state, comp, "Min", min, 0.5f, -100000.0f, max, NumericField::MinValue);
	float_field(state, comp, "Max", max, 0.5f, min, 100000.0f, NumericField::MaxValue);
	float step = bapi_ui_component_get_step(comp);
	float_field(state, comp, "Step", step, 0.5f, 0.0001f, 100000.0f, NumericField::Step);
}

static void selected_index_editor(EditorState &state, bapi_ui_component_t comp)
{
	int selected = bapi_ui_component_get_selected_index(comp);
	int next	 = selected;
	if (ImGui::DragInt(L("Selected Index"), &next, 1, 0, 1024) && next != selected) {
		EditorSetComponentSelectedIndex(state, comp, next);
	}
}

static void scroll_offset_editor(EditorState &state, bapi_ui_component_t comp)
{
	float offset = bapi_ui_component_get_scroll_offset(comp);
	float next	 = offset;
	if (ImGui::DragFloat(L("Scroll Offset"), &next, 1.0f, 0.0f, 100000.0f) && next != offset) {
		EditorSetComponentScrollOffset(state, comp, next);
	}
}

static void type_specific_editors(EditorState &state, bapi_ui_component_t comp,
								  bapi_ui_component_type_t type)
{
	switch (type) {
	case BAPI_UI_COMPONENT_SLIDER:
	case BAPI_UI_COMPONENT_PROGRESS:
		slider_value_editor(state, comp);
		break;
	case BAPI_UI_COMPONENT_ROW:
	case BAPI_UI_COMPONENT_COLUMN:
		float_field(state, comp, "Step", bapi_ui_component_get_step(comp), 0.5f, 0.0f,
					1000.0f, NumericField::Step);
		break;
	case BAPI_UI_COMPONENT_GRID:
		float_field(state, comp, "Step", bapi_ui_component_get_step(comp), 0.5f, 0.0f,
					1000.0f, NumericField::Step);
		{
			int	 columns = bapi_ui_component_get_columns(comp);
			int	 next	 = columns;
			if (ImGui::DragInt("Columns", &next, 1, 1, 64) && next != columns) {
				EditorSetComponentColumns(state, comp, next);
			}
		}
		break;
	case BAPI_UI_COMPONENT_CHECKBOX:
	case BAPI_UI_COMPONENT_TOGGLE:
	case BAPI_UI_COMPONENT_RADIO:
		bool_editor(state, comp, "Checked", BoolField::Checked,
					bapi_ui_component_is_checked(comp) != 0);
		break;
	case BAPI_UI_COMPONENT_CIRCLE:
		float_field(state, comp, "Radius", bapi_ui_component_get_radius(comp), 0.5f, 0.0f,
					10000.0f, NumericField::Radius);
		break;
	case BAPI_UI_COMPONENT_POLYGON:
		float_field(state, comp, "Radius", bapi_ui_component_get_radius(comp), 0.5f, 0.0f,
					10000.0f, NumericField::Radius);
		{
			int	 sides = bapi_ui_component_get_sides(comp);
			int	 next  = sides;
			if (ImGui::DragInt("Sides", &next, 1, 3, 64) && next != sides) {
				EditorSetComponentSides(state, comp, next);
			}
		}
		break;
	case BAPI_UI_COMPONENT_IMAGE:
	case BAPI_UI_COMPONENT_VIDEO:
	case BAPI_UI_COMPONENT_NINE_PATCH:
	case BAPI_UI_COMPONENT_ANIMATION:
		src_editor(state, comp);
		break;
	case BAPI_UI_COMPONENT_SELECT:
	case BAPI_UI_COMPONENT_LIST:
	case BAPI_UI_COMPONENT_TAB:
		selected_index_editor(state, comp);
		break;
	case BAPI_UI_COMPONENT_SCROLL:
		scroll_offset_editor(state, comp);
		break;
	default:
		break;
	}
}

static void batch_color_role_editor(EditorState &state, const char *label, bapi_ui_color_role_t role)
{
	bapi_color_t color;
	if (bapi_ui_component_get_color(state.selection, role, &color) != 0) return;
	float c[4] = {color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f};
	if (ImGui::ColorEdit4(label, c,
						  ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar)) {
		bapi_color_t nc;
		nc.r = (uint8_t)(c[0] * 255.0f + 0.5f);
		nc.g = (uint8_t)(c[1] * 255.0f + 0.5f);
		nc.b = (uint8_t)(c[2] * 255.0f + 0.5f);
		nc.a = (uint8_t)(c[3] * 255.0f + 0.5f);
		for (bapi_ui_component_t comp : state.selection_list)
			if (comp) EditorSetComponentColor(state, comp, role, nc);
	}
}

static void batch_editors(EditorState &state)
{
	if (state.selection_list.size() <= 1 || !state.selection) return;
	ImGui::Separator();
	ImGui::Text(L("Batch (%zu selected)"), state.selection_list.size());
	ImGui::PushID("batch");

	float next_size = bapi_ui_component_get_text_size(state.selection);
	ImGui::DragFloat(L("Text Size"), &next_size, 0.5f, 6.0f, 128.0f);
	if (ImGui::IsItemDeactivatedAfterEdit()) {
		for (bapi_ui_component_t comp : state.selection_list)
			if (comp) EditorSetComponentTextSize(state, comp, next_size);
	}

	bool visible = bapi_ui_component_is_visible(state.selection) != 0;
	if (ImGui::Checkbox(L("Visible"), &visible)) {
		for (bapi_ui_component_t comp : state.selection_list)
			if (comp) EditorSetComponentVisible(state, comp, visible);
	}
	bool enabled = bapi_ui_component_is_enabled(state.selection) != 0;
	if (ImGui::Checkbox(L("Enabled"), &enabled)) {
		for (bapi_ui_component_t comp : state.selection_list)
			if (comp) EditorSetComponentEnabled(state, comp, enabled);
	}

	ImGui::TextUnformatted(L("Apply color to all:"));
	batch_color_role_editor(state, L("Normal"), BAPI_UI_COLOR_NORMAL);
	batch_color_role_editor(state, L("Hover"), BAPI_UI_COLOR_HOVER);
	batch_color_role_editor(state, L("Click"), BAPI_UI_COLOR_CLICK);
	batch_color_role_editor(state, L("Text"), BAPI_UI_COLOR_TEXT);

	ImGui::PopID();
}

void EditorPropertiesPanel(EditorState &state)
{
	ImGui::Begin(L("Properties"));

	if (!state.selection) {
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", L("No selection"));
		ImGui::End();
		return;
	}

	batch_editors(state);

	bapi_ui_component_t comp = state.selection;
	const char *id = EditorComponentId(comp);
	const char *type = EditorComponentTypeName(bapi_ui_component_get_type(comp));

	ImGui::Text(L("Type: %s"), type);

	static char id_buf[128];
	if (!id_buf[0] || strcmp(id_buf, id) != 0) {
		snprintf(id_buf, sizeof(id_buf), "%s", id ? id : "");
	}
	bool id_unique = EditorIdIsUnique(state, comp, id_buf);
	if (ImGui::InputText(L("Id"), id_buf, sizeof(id_buf),
						 ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll) &&
		EditorIdIsUnique(state, comp, id_buf)) {
		EditorSetComponentId(state, comp, id_buf);
	}
	if (!id_unique) {
		ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "%s",
						   L("Id already in use by a sibling component"));
	}

	ImGui::Separator();
	ImGui::TextUnformatted(L("Rect (doc units)"));
	rect_editor(state, comp);

	ImGui::Separator();
	const char *text = bapi_ui_component_get_text(comp);
	if (text) {
		static char text_buf[512];
		if (strcmp(text_buf, text) != 0) {
			snprintf(text_buf, sizeof(text_buf), "%s", text ? text : "");
		}
		if (ImGui::InputText(L("Text"), text_buf, sizeof(text_buf))) {
			EditorSetComponentText(state, comp, text_buf);
		}
		float text_size = bapi_ui_component_get_text_size(comp);
		float next_size = text_size;
		if (ImGui::DragFloat(L("Text Size"), &next_size, 0.5f, 6.0f, 128.0f)) {
			EditorSetComponentTextSize(state, comp, next_size);
		}
	}

	ImGui::Separator();
	ImGui::TextUnformatted("Behavior");
	bool_editor(state, comp, L("Visible"), BoolField::Visible,
				bapi_ui_component_is_visible(comp) != 0);
	bool_editor(state, comp, L("Enabled"), BoolField::Enabled,
				bapi_ui_component_is_enabled(comp) != 0);
	bool_editor(state, comp, "Relative", BoolField::Relative,
				bapi_ui_component_get_relative(comp) != 0);
	anchor_editor(state, comp);

	ImGui::Separator();
	ImGui::TextUnformatted("Type Specific");
	type_specific_editors(state, comp, bapi_ui_component_get_type(comp));

	ImGui::Separator();
	ImGui::TextUnformatted("Colors");
	color_role_editor(state, comp, L("Normal"), BAPI_UI_COLOR_NORMAL);
	color_role_editor(state, comp, L("Hover"), BAPI_UI_COLOR_HOVER);
	color_role_editor(state, comp, L("Click"), BAPI_UI_COLOR_CLICK);
	color_role_editor(state, comp, L("Text"), BAPI_UI_COLOR_TEXT);

	ImGui::End();
}
