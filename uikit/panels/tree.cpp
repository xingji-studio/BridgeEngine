#include "../editor.h"
#include "../i18n.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

static bool is_selected(const EditorState &state, bapi_ui_component_t component)
{
	for (bapi_ui_component_t comp : state.selection_list)
		if (comp == component) return true;
	return false;
}

static void handle_drop_target(EditorState &state, bapi_ui_component_t component)
{
	if (!ImGui::BeginDragDropTarget()) return;
	if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("BAPI_UI_COMPONENT")) {
		bapi_ui_component_t dragged = nullptr;
		std::memcpy(&dragged, payload->Data, sizeof(dragged));
		if (dragged) EditorReparentComponent(state, dragged, component);
	}
	ImGui::EndDragDropTarget();
}

// Case-insensitive substring match against the component id (and type name).
static bool id_contains(const char *haystack, const std::string &needle)
{
	if (!haystack) return false;
	if (needle.empty()) return true;
	const char *h = haystack;
	while (*h) {
		size_t i = 0;
		while (i < needle.size() && h[i] &&
			   std::tolower((unsigned char)h[i]) ==
				   std::tolower((unsigned char)needle[i]))
			i++;
		if (i == needle.size()) return true;
		h++;
	}
	return false;
}

// True if this node or any descendant matches the query (used to decide
// whether to keep the node visible when the filter is active).
static bool subtree_matches(bapi_ui_component_t component, const std::string &query)
{
	if (id_contains(EditorComponentId(component), query)) return true;
	for (int i = 0; i < bapi_ui_component_get_child_count(component); i++)
		if (subtree_matches(bapi_ui_component_get_child(component, i), query)) return true;
	return false;
}

// Locked / editor-hidden nodes must still render (so the user can unlock them)
// but their children may be collapsed. Always render the node itself.
static void tree_node(EditorState &state, bapi_ui_component_t component, int depth,
					  const std::string &query)
{
	if (!component) return;
	const char *id = EditorComponentId(component);
	const char *type = EditorComponentTypeName(bapi_ui_component_get_type(component));
	int			child_count = bapi_ui_component_get_child_count(component);

	bool filtered = !query.empty() && !subtree_matches(component, query);

	// When searching, hide nodes that don't match and have no matching
	// descendants; their ancestors are kept only as expandable wrappers.
	char label[160];
	if (child_count > 0)
		snprintf(label, sizeof(label), "%s (%s) [%d]", id ? id : "?", type, child_count);
	else
		snprintf(label, sizeof(label), "%s (%s)", id ? id : "?", type);

	ImGui::PushID(component);
	bool selected = is_selected(state, component);
	bool locked   = EditorIsLocked(state, component);
	bool ehidden  = EditorIsEditorHidden(state, component);

	// Auto-expand the path to the component that asked to be revealed.
	bool reveal_here = state.reveal_request == component;
	bool reveal_in_subtree = false;
	if (state.reveal_request) {
		for (bapi_ui_component_t cur = state.reveal_request; cur;
			 cur = bapi_ui_component_get_parent(cur)) {
			if (cur == component) {
				reveal_in_subtree = true;
				break;
			}
		}
	}

	if (child_count > 0) {
		ImGuiTreeNodeFlags tflags = ImGuiTreeNodeFlags_OpenOnArrow |
									ImGuiTreeNodeFlags_OpenOnDoubleClick |
									ImGuiTreeNodeFlags_SpanAvailWidth |
									(reveal_in_subtree ? ImGuiTreeNodeFlags_DefaultOpen : 0) |
									(selected ? ImGuiTreeNodeFlags_Selected : 0);
		bool open = ImGui::TreeNodeEx(label, tflags);
		if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
			if (ImGui::GetIO().KeyCtrl)
				EditorToggleSelectComponent(state, component);
			else
				EditorSelectComponent(state, component);
		}
		if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
			ImGui::SetDragDropPayload("BAPI_UI_COMPONENT", &component, sizeof(component));
			ImGui::Text(L("Reparent %s"), id ? id : "?");
			ImGui::EndDragDropSource();
		}
		handle_drop_target(state, component);
		if (open) {
			if (!filtered) {
				for (int i = 0; i < child_count; i++) {
					bapi_ui_component_t child = bapi_ui_component_get_child(component, i);
					tree_node(state, child, depth + 1, query);
				}
			}
			ImGui::TreePop();
		}
	} else {
		if (ImGui::Selectable(label, selected)) {
			if (ImGui::GetIO().KeyCtrl)
				EditorToggleSelectComponent(state, component);
			else
				EditorSelectComponent(state, component);
		}
		if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
			ImGui::SetDragDropPayload("BAPI_UI_COMPONENT", &component, sizeof(component));
			ImGui::Text(L("Reparent %s"), id ? id : "?");
			ImGui::EndDragDropSource();
		}
		handle_drop_target(state, component);
	}

	// After the item is submitted, bring it into view if it was the target.
	if (reveal_here) ImGui::SetScrollHereY(0.5f);

	if (locked) {
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1.0f), " %s", L("(locked)"));
	}
	if (ehidden) {
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), " %s", L("(hidden)"));
	}

	// right-click: select this component (if not already) and open the menu
	if (ImGui::IsItemClicked(ImGuiMouseButton_Right) && !selected) {
		EditorSelectComponent(state, component);
	}
	if (ImGui::BeginPopupContextItem("##editor_ctx")) {
		std::vector<bapi_ui_component_t> menu_comps = state.selection_list;
		if (menu_comps.empty()) menu_comps.push_back(component);
		EditorContextMenu(state, menu_comps);
		ImGui::EndPopup();
	}
	ImGui::PopID();
}

void EditorTreePanel(EditorState &state)
{
	ImGui::Begin(L("Hierarchy"));

	// search box
	ImGui::SetNextItemWidth(-1.0f);
	ImGui::InputTextWithHint("##hier_search", L("Search components..."), state.search_query,
							 sizeof(state.search_query));
	ImGui::Separator();

	if (ImGui::BeginChild("tree")) {
		int root_count = state.ui ? bapi_ui_get_root_count(state.ui) : 0;
		if (root_count == 0) {
			ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Empty");
		} else {
			std::string query = state.search_query;
			for (int i = 0; i < root_count; i++) {
				bapi_ui_component_t root = bapi_ui_get_root(state.ui, i);
				if (!query.empty() && !subtree_matches(root, query)) continue;
				tree_node(state, root, 0, query);
			}
		}
		// drop onto empty area reparents to the UI root list
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("BAPI_UI_COMPONENT")) {
				bapi_ui_component_t dragged = nullptr;
				std::memcpy(&dragged, payload->Data, sizeof(dragged));
				if (dragged) EditorReparentComponent(state, dragged, nullptr);
			}
			ImGui::EndDragDropTarget();
		}
	}
	ImGui::EndChild();

	// The reveal request was served this frame.
	state.reveal_request = nullptr;
	ImGui::End();
}
