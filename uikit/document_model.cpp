#include "editor.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------
// multi-document management (M3-C): the active document's fields live directly
// on EditorState; parked documents are swapped in/out of the state.
// ---------------------------------------------------------------------------

static void apply_smart_snap(EditorState &state, float *io_dx, float *io_dy);

static void swap_doc_state(Document &doc, EditorState &state)
{
	std::swap(doc.ui, state.ui);
	std::swap(doc.filepath, state.filepath);
	std::swap(doc.dirty, state.dirty);
	std::swap(doc.selection, state.selection);
	std::swap(doc.selection_list, state.selection_list);
	std::swap(doc.view_scale, state.view_scale);
	std::swap(doc.view_offset_x, state.view_offset_x);
	std::swap(doc.view_offset_y, state.view_offset_y);
	std::swap(doc.undo_stack, state.undo_stack);
	std::swap(doc.redo_stack, state.redo_stack);
	std::swap(doc.drag_component, state.drag_component);
	std::swap(doc.drag_start_rect, state.drag_start_rect);
	std::swap(doc.drag_start_rects, state.drag_start_rects);
	std::swap(doc.drag_start_mouse_x, state.drag_start_mouse_x);
	std::swap(doc.drag_start_mouse_y, state.drag_start_mouse_y);
	std::swap(doc.dragging, state.dragging);
	std::swap(doc.resize_handle, state.resize_handle);
	std::swap(doc.resizing, state.resizing);
	std::swap(doc.marquee_active, state.marquee_active);
	std::swap(doc.marquee_start_x, state.marquee_start_x);
	std::swap(doc.marquee_start_y, state.marquee_start_y);
	std::swap(doc.marquee_cur_x, state.marquee_cur_x);
	std::swap(doc.marquee_cur_y, state.marquee_cur_y);
	std::swap(doc.owner_registry, state.owner_registry);
	std::swap(doc.active_scene, state.active_scene);
	std::swap(doc.persistent_roots, state.persistent_roots);
	std::swap(doc.locked, state.locked);
	std::swap(doc.editor_hidden, state.editor_hidden);
}

static void reset_active_doc(EditorState &state)
{
	state.ui = nullptr;
	state.filepath.clear();
	state.dirty = false;
	state.selection = nullptr;
	state.selection_list.clear();
	state.view_scale = 1.0f;
	state.view_offset_x = 0.0f;
	state.view_offset_y = 0.0f;
	state.undo_stack.clear();
	state.redo_stack.clear();
	state.drag_component = nullptr;
	state.drag_start_rect = {0.0f, 0.0f, 0.0f, 0.0f};
	state.drag_start_rects.clear();
	state.drag_start_mouse_x = 0.0f;
	state.drag_start_mouse_y = 0.0f;
	state.dragging = false;
	state.resize_handle = -1;
	state.resizing = false;
	state.marquee_active = false;
	state.marquee_start_x = 0.0f;
	state.marquee_start_y = 0.0f;
	state.marquee_cur_x = 0.0f;
	state.marquee_cur_y = 0.0f;
	state.owner_registry.clear();
	state.active_scene = nullptr;
	state.persistent_roots.clear();
	state.locked.clear();
	state.editor_hidden.clear();
}

static bool has_doc(const EditorState &state)
{
	return state.active_doc >= 0 && state.active_doc < (int)state.documents.size();
}

// The active document's fields live directly on EditorState, so its parked
// copy inside documents[active_doc] is stale and must not be read. These
// accessors centralize that rule.
bool EditorDocumentIsDirty(const EditorState &state, int index)
{
	if (index < 0 || index >= (int)state.documents.size()) return false;
	if (index == state.active_doc) return state.dirty;
	return state.documents[index]->dirty;
}

const std::string &EditorDocumentPath(const EditorState &state, int index)
{
	static const std::string kEmpty;
	if (index < 0 || index >= (int)state.documents.size()) return kEmpty;
	if (index == state.active_doc) return state.filepath;
	return state.documents[index]->filepath;
}

std::string EditorDocumentTitle(const EditorState &state, int index)
{
	const std::string &path = EditorDocumentPath(state, index);
	if (path.empty()) return "Untitled";
	size_t slash = path.find_last_of("/\\");
	std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
	if (EditorDocumentIsDirty(state, index)) name += " *";
	return name;
}

void EditorActivateDocument(EditorState &state, int index)
{
	if (index < 0 || index >= (int)state.documents.size() || index == state.active_doc) return;
	if (has_doc(state)) swap_doc_state(*state.documents[state.active_doc], state);
	state.active_doc = index;
	swap_doc_state(*state.documents[index], state);
	state.tab_select_request = index;
}

void EditorCloseDocument(EditorState &state, int index)
{
	if (index < 0 || index >= (int)state.documents.size()) return;
	if (index == state.active_doc) {
		if (has_doc(state)) swap_doc_state(*state.documents[index], state);
		state.documents.erase(state.documents.begin() + index);
		if (state.documents.empty()) {
			state.active_doc = -1;
			reset_active_doc(state);
			EditorNewDocument(state);
			return;
		}
		int next = index < (int)state.documents.size() ? index : (int)state.documents.size() - 1;
		state.active_doc = next;
		swap_doc_state(*state.documents[next], state);
		state.tab_select_request = next;
	} else {
		state.documents.erase(state.documents.begin() + index);
		if (index < state.active_doc) {
			state.active_doc--;
			state.tab_select_request = state.active_doc;
		}
	}
}

static const char *kRecentFile = "recent_files.txt";

void EditorPushRecentFile(EditorState &state, const char *path)
{
	if (!path || !path[0]) return;
	auto it = std::find(state.recent_files.begin(), state.recent_files.end(), path);
	if (it != state.recent_files.end()) state.recent_files.erase(it);
	state.recent_files.insert(state.recent_files.begin(), path);
	if (state.recent_files.size() > 8) state.recent_files.resize(8);
	EditorSaveRecentFiles(state);
}

void EditorLoadRecentFiles(EditorState &state)
{
	FILE *f = fopen(kRecentFile, "r");
	if (!f) return;
	char buf[1024];
	while (fgets(buf, sizeof(buf), f)) {
		size_t len = strlen(buf);
		while (len && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) buf[--len] = 0;
		if (len) state.recent_files.push_back(buf);
	}
	fclose(f);
}

void EditorSaveRecentFiles(EditorState &state)
{
	FILE *f = fopen(kRecentFile, "w");
	if (!f) return;
	for (const std::string &path : state.recent_files) fprintf(f, "%s\n", path.c_str());
	fclose(f);
}

bapi_ui_component_t EditorFindByPath(EditorState &state, const char *id)
{
	return id ? bapi_ui_find(state.ui, id) : nullptr;
}

const char *EditorComponentId(bapi_ui_component_t component)
{
	return component ? bapi_ui_component_get_id(component) : nullptr;
}

const char *EditorComponentTypeName(bapi_ui_component_type_t type)
{
	switch (type) {
	case BAPI_UI_COMPONENT_RECT:
		return "Rect";
	case BAPI_UI_COMPONENT_LABEL:
		return "Label";
	case BAPI_UI_COMPONENT_BUTTON:
		return "Button";
	case BAPI_UI_COMPONENT_IMAGE:
		return "Image";
	case BAPI_UI_COMPONENT_LINE:
		return "Line";
	case BAPI_UI_COMPONENT_CIRCLE:
		return "Circle";
	case BAPI_UI_COMPONENT_POLYGON:
		return "Polygon";
	case BAPI_UI_COMPONENT_BORDER:
		return "Border";
	case BAPI_UI_COMPONENT_PROGRESS:
		return "Progress";
	case BAPI_UI_COMPONENT_SEPARATOR:
		return "Separator";
	case BAPI_UI_COMPONENT_PANEL:
		return "Panel";
	case BAPI_UI_COMPONENT_CONTAINER:
		return "Container";
	case BAPI_UI_COMPONENT_ROW:
		return "Row";
	case BAPI_UI_COMPONENT_COLUMN:
		return "Column";
	case BAPI_UI_COMPONENT_GRID:
		return "Grid";
	case BAPI_UI_COMPONENT_CHECKBOX:
		return "Checkbox";
	case BAPI_UI_COMPONENT_RADIO:
		return "Radio";
	case BAPI_UI_COMPONENT_TOGGLE:
		return "Toggle";
	case BAPI_UI_COMPONENT_SLIDER:
		return "Slider";
	case BAPI_UI_COMPONENT_INPUT:
		return "Input";
	case BAPI_UI_COMPONENT_SELECT:
		return "Select";
	case BAPI_UI_COMPONENT_LIST:
		return "List";
	case BAPI_UI_COMPONENT_SCROLL:
		return "Scroll";
	case BAPI_UI_COMPONENT_TAB:
		return "Tab";
	case BAPI_UI_COMPONENT_VIDEO:
		return "Video";
	case BAPI_UI_COMPONENT_CANVAS:
		return "Canvas";
	case BAPI_UI_COMPONENT_NINE_PATCH:
		return "NinePatch";
	case BAPI_UI_COMPONENT_ANIMATION:
		return "Animation";
	case BAPI_UI_COMPONENT_TOOLTIP:
		return "Tooltip";
	case BAPI_UI_COMPONENT_MODAL:
		return "Modal";
	case BAPI_UI_COMPONENT_POPUP:
		return "Popup";
	}
	return "Unknown";
}

bool EditorIsContainerType(bapi_ui_component_type_t type)
{
	switch (type) {
	case BAPI_UI_COMPONENT_PANEL:
	case BAPI_UI_COMPONENT_CONTAINER:
	case BAPI_UI_COMPONENT_ROW:
	case BAPI_UI_COMPONENT_COLUMN:
	case BAPI_UI_COMPONENT_GRID:
	case BAPI_UI_COMPONENT_SCROLL:
	case BAPI_UI_COMPONENT_CANVAS:
	case BAPI_UI_COMPONENT_MODAL:
	case BAPI_UI_COMPONENT_POPUP:
		return true;
	default:
		return false;
	}
}

void EditorMarkDirty(EditorState &state)
{
	state.dirty = true;
}

void EditorNewDocument(EditorState &state)
{
	if (has_doc(state)) swap_doc_state(*state.documents[state.active_doc], state);
	state.active_doc = (int)state.documents.size();
	state.documents.push_back(std::make_unique<Document>());
	reset_active_doc(state);
	state.ui = bapi_ui_create();
	EditorSetViewCamera(state, 1.0f, 0.0f, 0.0f);
	state.tab_select_request = state.active_doc;
}

bool EditorLoadFile(EditorState &state, const char *path)
{
	if (!path || !path[0]) return false;
	// load_from_file: plain XML when ui_key is empty, encrypted .uix otherwise
	const char *key = state.ui_key[0] ? state.ui_key : "";
	bapi_ui_t ui = bapi_ui_load_from_file(path, key);
	if (!ui) return false;

	if (has_doc(state)) swap_doc_state(*state.documents[state.active_doc], state);
	state.active_doc = (int)state.documents.size();
	state.documents.push_back(std::make_unique<Document>());
	reset_active_doc(state);

	state.ui		 = ui;
	state.filepath	 = path;
	state.dirty		 = false;
	state.show_welcome = false;
	EditorSetViewCamera(state, 1.0f, 0.0f, 0.0f);
	EditorPushRecentFile(state, path);
	state.tab_select_request = state.active_doc;
	return true;
}

bool EditorSaveFile(EditorState &state, const char *path)
{
	if (!state.ui || !path || !path[0]) return false;
	// Documents are ALWAYS saved as plain XML: the source tree stays editable
	// and diffable. Encryption happens at build time (project CMake reads
	// .uixkey and encrypts ui/*.xml into the output dir), never in the editor.
	if (bapi_ui_save_to_xml(state.ui, path) != 0) return false;
	state.filepath = path;
	state.dirty	   = false;
	EditorPushRecentFile(state, path);
	return true;
}

static bapi_ui_component_t hit_test_node(EditorState &state, bapi_ui_component_t node, float x,
										float y)
{
	if (!node) return nullptr;
	// Invisible components (and their subtrees) are not rendered, so they must
	// not swallow clicks either -- otherwise hidden scene panels overlapping
	// visible content would steal the selection. Use the tree panel to select
	// hidden components.
	if (!bapi_ui_component_is_visible(node)) return nullptr;
	// Locked components (and their subtrees) cannot be selected in the view.
	if (state.locked.count(node)) return nullptr;
	for (int i = bapi_ui_component_get_child_count(node) - 1; i >= 0; i--) {
		bapi_ui_component_t child = bapi_ui_component_get_child(node, i);
		bapi_ui_component_t hit   = hit_test_node(state, child, x, y);
		if (hit) return hit;
	}
	bapi_rect_t rect;
	bapi_ui_component_get_rect(node, &rect);
	// Text-drawn components (labels, tooltips, inputs, ...) usually have a
	// zero rect w/h because their size comes from the font. Measure the actual
	// text bounds so clicking the rendered text still selects the component.
	if (rect.w <= 0.0f || rect.h <= 0.0f) {
		float tw = 0.0f, th = 0.0f;
		const char *text = bapi_ui_component_get_text(node);
		bapi_get_text_size(text ? text : "", bapi_ui_component_get_text_size(node), &tw, &th);
		if (rect.w <= 0.0f) rect.w = tw;
		if (rect.h <= 0.0f) rect.h = th;
	}
	if (x >= rect.x && y >= rect.y && x <= rect.x + rect.w && y <= rect.y + rect.h) return node;
	return nullptr;
}

bapi_ui_component_t EditorHitTest(EditorState &state, float doc_x, float doc_y)
{
	if (!state.ui) return nullptr;
	for (int i = bapi_ui_get_root_count(state.ui) - 1; i >= 0; i--) {
		bapi_ui_component_t root = bapi_ui_get_root(state.ui, i);
		bapi_ui_component_t hit  = hit_test_node(state, root, doc_x, doc_y);
		if (hit) return hit;
	}
	return nullptr;
}

void EditorSetViewCamera(EditorState &state, float scale, float offset_x, float offset_y)
{
	if (scale > 0.0f) state.view_scale = scale;
	state.view_offset_x = offset_x;
	state.view_offset_y = offset_y;
}

float EditorScreenToDocX(EditorState &state, float screen_x)
{
	return (screen_x - state.viewport_origin.x) / state.view_scale - state.view_offset_x;
}

float EditorScreenToDocY(EditorState &state, float screen_y)
{
	return (screen_y - state.viewport_origin.y) / state.view_scale - state.view_offset_y;
}

static void reset_interaction(EditorState &state)
{
	state.dragging = false;
	state.drag_component = nullptr;
	state.resizing = false;
	state.resize_handle = -1;
	state.marquee_active = false;
}

void EditorSelectComponent(EditorState &state, bapi_ui_component_t component)
{
	reset_interaction(state);
	state.selection_list.clear();
	state.selection = component;
	if (component) state.selection_list.push_back(component);
}

void EditorToggleSelectComponent(EditorState &state, bapi_ui_component_t component)
{
	if (!component) return;
	reset_interaction(state);
	auto it = std::find(state.selection_list.begin(), state.selection_list.end(), component);
	if (it != state.selection_list.end()) {
		state.selection_list.erase(it);
		if (state.selection == component) {
			state.selection = state.selection_list.empty() ? nullptr : state.selection_list.back();
		}
	} else {
		state.selection_list.push_back(component);
		state.selection = component;
	}
}

void EditorClearSelection(EditorState &state)
{
	reset_interaction(state);
	state.selection_list.clear();
	state.selection = nullptr;
}

static void collect_all_nodes(bapi_ui_component_t node, std::vector<bapi_ui_component_t> &out)
{
	if (!node) return;
	out.push_back(node);
	for (int i = 0; i < bapi_ui_component_get_child_count(node); i++)
		collect_all_nodes(bapi_ui_component_get_child(node, i), out);
}

void EditorSelectAll(EditorState &state)
{
	if (!state.ui) return;
	reset_interaction(state);
	state.selection_list.clear();
	for (int i = 0; i < bapi_ui_get_root_count(state.ui); i++)
		collect_all_nodes(bapi_ui_get_root(state.ui, i), state.selection_list);
	state.selection = state.selection_list.empty() ? nullptr : state.selection_list.back();
}

bool EditorIsAncestor(bapi_ui_component_t node, bapi_ui_component_t candidate)
{
	for (bapi_ui_component_t cur = node; cur; cur = bapi_ui_component_get_parent(cur))
		if (cur == candidate) return true;
	return false;
}

float EditorSnapValue(EditorState &state, float value)
{
	if (!state.snap_to_grid || state.grid_size <= 0.0f) return value;
	float inv = 1.0f / state.grid_size;
	return std::floor(value * inv + 0.5f) * state.grid_size;
}

void EditorBeginViewDrag(EditorState &state, float mouse_x, float mouse_y)
{
	if (!state.selection) return;
	state.drag_component	 = state.selection;
	state.drag_start_mouse_x = mouse_x;
	state.drag_start_mouse_y = mouse_y;
	bapi_ui_component_get_rect(state.selection, &state.drag_start_rect);
	state.drag_start_rects.clear();
	for (bapi_ui_component_t comp : state.selection_list) {
		bapi_rect_t rect;
		bapi_ui_component_get_rect(comp, &rect);
		state.drag_start_rects[comp] = rect;
	}
	state.dragging = true;
}

void EditorUpdateViewDrag(EditorState &state, float mouse_x, float mouse_y)
{
	if (!state.dragging || !state.drag_component) return;
	float doc_dx = EditorScreenToDocX(state, mouse_x) - EditorScreenToDocX(state, state.drag_start_mouse_x);
	float doc_dy = EditorScreenToDocY(state, mouse_y) - EditorScreenToDocY(state, state.drag_start_mouse_y);
	if (state.snap_to_grid) {
		float dx = EditorSnapValue(state, state.drag_start_rect.x + doc_dx) - state.drag_start_rect.x;
		float dy = EditorSnapValue(state, state.drag_start_rect.y + doc_dy) - state.drag_start_rect.y;
		doc_dx = dx;
		doc_dy = dy;
	}
	// smart snap (edge/center alignment) on top of grid snap
	apply_smart_snap(state, &doc_dx, &doc_dy);
	for (auto &entry : state.drag_start_rects) {
		bapi_ui_component_t comp = entry.first;
		if (!comp) continue;
		bapi_rect_t rect = entry.second;
		rect.x += doc_dx;
		rect.y += doc_dy;
		bapi_ui_component_set_rect(comp, rect);
	}
	EditorMarkDirty(state);
}

void EditorEndViewDrag(EditorState &state)
{
	state.drag_component = nullptr;
	state.drag_start_rects.clear();
	state.dragging		 = false;
	state.snap_guides_v.clear();
	state.snap_guides_h.clear();
}

// smart snap: align dragged rects' edges/centers to other components while
// dragging. Adjusts the drag delta and records guide-line positions.

static const float kSnapThreshold = 8.0f; // doc units

static void snap_axis(float edge, const std::vector<float> &targets, float *out_snap,
					  std::vector<float> *out_guides)
{
	for (float t : targets) {
		if (fabsf(edge - t) < kSnapThreshold) {
			*out_snap += (t - edge);
			out_guides->push_back(t);
			return; // first match wins (keep guides small)
		}
	}
}

static void walk_components(bapi_ui_component_t node,
							const std::function<bool(bapi_ui_component_t)> &visit)
{
	if (!node) return;
	if (!visit(node)) return; // visit returns false to prune subtree
	int n = bapi_ui_component_get_child_count(node);
	for (int i = 0; i < n; i++) walk_components(bapi_ui_component_get_child(node, i), visit);
}

// Called during a drag: given the current (already-grid-snapped) delta, adjust
// it so the dragged rects align to nearby component geometry, and record guide
// lines for the overlay.
static void apply_smart_snap(EditorState &state, float *io_dx, float *io_dy)
{
	state.snap_guides_v.clear();
	state.snap_guides_h.clear();
	if (state.drag_start_rects.empty() || !state.ui) return;

	// Bounding box of the dragged selection at its current (pre-snap) position.
	float minx = 1e30f, miny = 1e30f, maxx = -1e30f, maxy = -1e30f;
	for (auto &entry : state.drag_start_rects) {
		const bapi_rect_t &r = entry.second;
		minx = std::min(minx, r.x + *io_dx);
		miny = std::min(miny, r.y + *io_dy);
		maxx = std::max(maxx, r.x + r.w + *io_dx);
		maxy = std::max(maxy, r.y + r.h + *io_dy);
	}
	if (minx > maxx || miny > maxy) return;

	// Candidate lines from the dragged bbox.
	float edges_x[] = {minx, (minx + maxx) * 0.5f, maxx};
	float edges_y[] = {miny, (miny + maxy) * 0.5f, maxy};

	// Collect target lines from all components not in the dragged set.
	std::vector<float> tv, th;
	auto is_dragged = [&](bapi_ui_component_t c) { return state.drag_start_rects.count(c) != 0; };
	walk_components(state.ui ? bapi_ui_get_root(state.ui, 0) : nullptr,
					[&](bapi_ui_component_t comp) {
						if (is_dragged(comp)) return true;
						bapi_rect_t r;
						bapi_ui_component_get_rect(comp, &r);
						if (r.w <= 0.0f && r.h <= 0.0f) return true;
						tv.push_back(r.x);
						tv.push_back(r.x + r.w);
						tv.push_back((r.x + r.x + r.w) * 0.5f);
						th.push_back(r.y);
						th.push_back(r.y + r.h);
						th.push_back((r.y + r.y + r.h) * 0.5f);
						return true;
					});
	// Also walk the remaining roots.
	for (int ri = 0; ri < bapi_ui_get_root_count(state.ui); ri++)
		if (ri > 0) {
			walk_components(bapi_ui_get_root(state.ui, ri),
							[&](bapi_ui_component_t comp) {
								if (is_dragged(comp)) return true;
								bapi_rect_t r;
								bapi_ui_component_get_rect(comp, &r);
								if (r.w <= 0.0f && r.h <= 0.0f) return true;
								tv.push_back(r.x);
								tv.push_back(r.x + r.w);
								tv.push_back((r.x + r.x + r.w) * 0.5f);
								th.push_back(r.y);
								th.push_back(r.y + r.h);
								th.push_back((r.y + r.y + r.h) * 0.5f);
								return true;
							});
		}

	// Snap dx using x-axis targets (guides are vertical lines), dy using y-axis.
	float snap_dx = 0.0f, snap_dy = 0.0f;
	for (float e : edges_x) snap_axis(e, tv, &snap_dx, &state.snap_guides_v);
	for (float e : edges_y) snap_axis(e, th, &snap_dy, &state.snap_guides_h);
	*io_dx += snap_dx;
	*io_dy += snap_dy;
}

static bool rects_intersect(const bapi_rect_t &a, const bapi_rect_t &b)
{
	return a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h && a.y + a.h > b.y;
}

static void box_select_node(EditorState &state, bapi_ui_component_t node, bapi_rect_t rect,
							std::vector<bapi_ui_component_t> &out)
{
	if (!node) return;
	if (!bapi_ui_component_is_visible(node)) return;
	if (state.locked.count(node)) return; // locked subtrees are not box-selectable
	bapi_rect_t r;
	bapi_ui_component_get_rect(node, &r);
	if (rects_intersect(rect, r)) out.push_back(node);
	for (int i = 0; i < bapi_ui_component_get_child_count(node); i++)
		box_select_node(state, bapi_ui_component_get_child(node, i), rect, out);
}

std::vector<bapi_ui_component_t> EditorBoxSelect(EditorState &state, bapi_rect_t rect)
{
	std::vector<bapi_ui_component_t> result;
	if (!state.ui) return result;
	for (int i = 0; i < bapi_ui_get_root_count(state.ui); i++)
		box_select_node(state, bapi_ui_get_root(state.ui, i), rect, result);
	return result;
}

void EditorBeginResize(EditorState &state, int handle, float mouse_x, float mouse_y)
{
	if (handle < 0 || handle > 7 || !state.selection) return;
	bapi_ui_component_get_rect(state.selection, &state.drag_start_rect);
	state.drag_start_mouse_x = mouse_x;
	state.drag_start_mouse_y = mouse_y;
	state.resize_handle		 = handle;
	state.resizing			 = true;
}

void EditorUpdateResize(EditorState &state, float mouse_x, float mouse_y)
{
	if (!state.resizing || !state.selection) return;
	float doc_x = EditorScreenToDocX(state, mouse_x);
	float doc_y = EditorScreenToDocY(state, mouse_y);
	if (state.snap_to_grid) {
		doc_x = EditorSnapValue(state, doc_x);
		doc_y = EditorSnapValue(state, doc_y);
	}
	bapi_rect_t r = state.drag_start_rect;
	const float min_w = 4.0f;
	const float min_h = 4.0f;
	int			h	= state.resize_handle;

	// handle layout:  0 NW  1 N  2 NE  3 E  4 SE  5 S  6 SW  7 W
	float left = r.x, right = r.x + r.w, top = r.y, bottom = r.y + r.h;
	if (h == 0 || h == 7 || h == 6) { // west edge
		left		= doc_x;
		r.x			= left;
		r.w			= right - left;
		if (r.w < min_w) {
			r.w = min_w;
			r.x = right - min_w;
		}
	} else if (h == 2 || h == 3 || h == 4) { // east edge
		right		= doc_x;
		r.w			= right - left;
		if (r.w < min_w) r.w = min_w;
	}
	if (h == 0 || h == 1 || h == 2) { // north edge
		top		= doc_y;
		r.y		= top;
		r.h		= bottom - top;
		if (r.h < min_h) {
			r.h = min_h;
			r.y = bottom - min_h;
		}
	} else if (h == 4 || h == 5 || h == 6) { // south edge
		bottom		= doc_y;
		r.h			= bottom - top;
		if (r.h < min_h) r.h = min_h;
	}
	bapi_ui_component_set_rect(state.selection, r);
	EditorMarkDirty(state);
}

void EditorEndResize(EditorState &state)
{
	if (!state.resizing || !state.selection) {
		state.resizing = false;
		state.resize_handle = -1;
		return;
	}
	bapi_rect_t new_rect;
	bapi_ui_component_get_rect(state.selection, &new_rect);
	EditorCommitMove(state, state.selection, state.drag_start_rect, new_rect);
	state.resizing = false;
	state.resize_handle = -1;
}

void EditorNudgeSelection(EditorState &state, float dx, float dy)
{
	if (state.selection_list.empty()) return;
	std::vector<RectMove> moves;
	for (bapi_ui_component_t comp : state.selection_list) {
		if (!comp) continue;
		if (state.locked.count(comp)) continue;
		bapi_rect_t old_rect;
		bapi_ui_component_get_rect(comp, &old_rect);
		bapi_rect_t new_rect = old_rect;
		if (state.snap_to_grid) {
			new_rect.x = EditorSnapValue(state, old_rect.x + dx);
			new_rect.y = EditorSnapValue(state, old_rect.y + dy);
			if (new_rect.x == old_rect.x && new_rect.y == old_rect.y) continue;
		} else {
			new_rect.x += dx;
			new_rect.y += dy;
		}
		moves.push_back(RectMove{comp, old_rect, new_rect});
	}
	EditorCommitMultiMove(state, moves);
}

void EditorClearHistory(EditorState &state)
{
	state.undo_stack.clear();
	state.redo_stack.clear();
	state.owner_registry.clear();
}
