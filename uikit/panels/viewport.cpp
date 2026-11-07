#include "../editor.h"
#include "../i18n.h"

#include "internal/bapi_internal.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>

// The engine's render_ex maps: screen = (doc + offset) * scale.
// The viewport renders the engine UI into an offscreen SDL texture, then the
// texture is displayed with ImGui::Image so z-ordering with the docking layout
// is handled by ImGui.

extern "C" {
SDL_Window *g_editor_window = nullptr;
SDL_Renderer *g_editor_renderer = nullptr;
}

static float screen_to_doc_x(EditorState &state, float sx);
static float screen_to_doc_y(EditorState &state, float sy);
static float texture_to_doc_x(EditorState &state, float tx);
static float texture_to_doc_y(EditorState &state, float ty);
static float doc_to_window_x(EditorState &state, float dx);
static float doc_to_window_y(EditorState &state, float dy);
static void draw_doc_grid(SDL_Renderer *renderer, EditorState &state, int w, int h);

static SDL_Renderer *native_renderer(void)
{
	if (g_editor_renderer) return g_editor_renderer;
	g_editor_renderer = (SDL_Renderer *)bapi_internal_get_native_renderer();
	return g_editor_renderer;
}

static SDL_Texture *g_viewport_texture = nullptr;
static int			g_viewport_tex_w = 0;
static int			g_viewport_tex_h = 0;

static void ensure_viewport_texture(int w, int h)
{
	SDL_Renderer *renderer = native_renderer();
	if (!renderer) return;
	if (w <= 0 || h <= 0) {
		if (g_viewport_texture) {
			SDL_DestroyTexture(g_viewport_texture);
			g_viewport_texture = nullptr;
			g_viewport_tex_w = 0;
			g_viewport_tex_h = 0;
		}
		return;
	}
	if (g_viewport_texture && g_viewport_tex_w == w && g_viewport_tex_h == h) return;
	if (g_viewport_texture) {
		SDL_DestroyTexture(g_viewport_texture);
		g_viewport_texture = nullptr;
	}
	g_viewport_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
										   SDL_TEXTUREACCESS_TARGET, w, h);
	g_viewport_tex_w = w;
	g_viewport_tex_h = h;
}

// Called once at startup and once at shutdown to release the viewport texture.
void EditorViewportCleanup(void)
{
	if (g_viewport_texture) {
		SDL_DestroyTexture(g_viewport_texture);
		g_viewport_texture = nullptr;
	}
	g_viewport_tex_w = 0;
	g_viewport_tex_h = 0;
}

void EditorRenderEngineView(EditorState &state)
{
	if (!state.ui || !state.viewport_visible) return;
	SDL_Renderer *renderer = native_renderer();
	if (!renderer) return;

	int w = (int)state.viewport_size.x;
	int h = (int)state.viewport_size.y;
	if (w <= 0 || h <= 0) return;

	ensure_viewport_texture(w, h);
	if (!g_viewport_texture) return;

	SDL_Texture *prev_target = SDL_GetRenderTarget(renderer);
	SDL_SetRenderTarget(renderer, g_viewport_texture);

	SDL_SetRenderDrawColor(renderer, 45, 45, 50, 255);
	SDL_RenderClear(renderer);

	draw_doc_grid(renderer, state, w, h);

	// Layout first so relative-positioned children render at resolved positions.
	bapi_ui_layout(state.ui);

	bapi_ui_render_ex(state.ui, state.view_offset_x, state.view_offset_y, state.view_scale);

	SDL_SetRenderTarget(renderer, prev_target);
}

// Draw a doc-space grid into the viewport texture, aligned to grid_size.
static void draw_doc_grid(SDL_Renderer *renderer, EditorState &state, int w, int h)
{
	if (!state.show_grid || state.grid_size <= 0.0f) return;
	float gs = state.grid_size;
	float wx0 = texture_to_doc_x(state, 0.0f);
	float wy0 = texture_to_doc_y(state, 0.0f);
	float wx1 = texture_to_doc_x(state, (float)w);
	float wy1 = texture_to_doc_y(state, (float)h);

	SDL_SetRenderDrawColor(renderer, 62, 62, 70, 255);
	for (float gx = std::floor(wx0 / gs) * gs; gx <= wx1; gx += gs) {
		float sx = (gx + state.view_offset_x) * state.view_scale;
		SDL_RenderLine(renderer, (int)sx, 0, (int)sx, h);
	}
	for (float gy = std::floor(wy0 / gs) * gs; gy <= wy1; gy += gs) {
		float sy = (gy + state.view_offset_y) * state.view_scale;
		SDL_RenderLine(renderer, 0, (int)sy, w, (int)sy);
	}
}

// Convert an ImGui mouse position (SCREEN coordinates) to doc coordinates.
// Engine: screen = (doc + offset) * scale, drawn at screen position
// state.viewport_origin =>  doc = (screen - origin)/scale - offset.
static float screen_to_doc_x(EditorState &state, float sx)
{
	return (sx - state.viewport_origin.x) / state.view_scale - state.view_offset_x;
}
static float screen_to_doc_y(EditorState &state, float sy)
{
	return (sy - state.viewport_origin.y) / state.view_scale - state.view_offset_y;
}

// Texture pixel coordinates (0..viewport size) to doc coordinates. The grid is
// rasterized into the texture, so unlike mouse coordinates these need no
// viewport_origin subtraction.
static float texture_to_doc_x(EditorState &state, float tx)
{
	return tx / state.view_scale - state.view_offset_x;
}
static float texture_to_doc_y(EditorState &state, float ty)
{
	return ty / state.view_scale - state.view_offset_y;
}

// Doc coordinates to ImGui viewport/screen coordinates for overlay drawing.
// The offscreen texture is displayed at screen position viewport_origin, so a
// doc point lands on screen at origin + (doc + offset) * scale.
// GetWindowDrawList() draws in the same coordinate space as io.MousePos and
// GetCursorScreenPos() (main viewport top-left = (0,0)), so no window-position
// offset is applied here.
static float doc_to_window_x(EditorState &state, float dx)
{
	return state.viewport_origin.x + (dx + state.view_offset_x) * state.view_scale;
}
static float doc_to_window_y(EditorState &state, float dy)
{
	return state.viewport_origin.y + (dy + state.view_offset_y) * state.view_scale;
}

static bool mouse_in_viewport(EditorState &state, const ImVec2 &mouse)
{
	return state.viewport_visible && mouse.x >= state.viewport_origin.x &&
		   mouse.y >= state.viewport_origin.y &&
		   mouse.x <= state.viewport_origin.x + state.viewport_size.x &&
		   mouse.y <= state.viewport_origin.y + state.viewport_size.y;
}

static void get_handle_positions(EditorState &state, ImVec2 out[8])
{
	bapi_rect_t rect;
	bapi_ui_component_get_rect(state.selection, &rect);
	float sx1 = doc_to_window_x(state, rect.x);
	float sy1 = doc_to_window_y(state, rect.y);
	float sx2 = doc_to_window_x(state, rect.x + rect.w);
	float sy2 = doc_to_window_y(state, rect.y + rect.h);
	float midx = (sx1 + sx2) * 0.5f;
	float midy = (sy1 + sy2) * 0.5f;
	out[0] = ImVec2(sx1, sy1);
	out[1] = ImVec2(midx, sy1);
	out[2] = ImVec2(sx2, sy1);
	out[3] = ImVec2(sx2, midy);
	out[4] = ImVec2(sx2, sy2);
	out[5] = ImVec2(midx, sy2);
	out[6] = ImVec2(sx1, sy2);
	out[7] = ImVec2(sx1, midy);
}

static int resize_handle_hit(EditorState &state, float mx, float my)
{
	if (state.selection_list.size() != 1 || !state.selection) return -1;
	ImVec2 handles[8];
	get_handle_positions(state, handles);
	const float reach = 7.0f;
	for (int i = 0; i < 8; i++) {
		if (mx >= handles[i].x - reach && mx <= handles[i].x + reach &&
			my >= handles[i].y - reach && my <= handles[i].y + reach)
			return i;
	}
	return -1;
}

void EditorViewportPanel(EditorState &state)
{
	ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(30, 30, 34, 255));
	ImGui::Begin(L("Viewport"), nullptr,
				 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

	ImVec2 avail = ImGui::GetContentRegionAvail();
	ImVec2 pos   = ImGui::GetCursorScreenPos();
	state.viewport_origin = pos;
	state.viewport_size	  = avail;
	state.viewport_visible = avail.x > 8.0f && avail.y > 8.0f;

	ImGuiIO &io	 = ImGui::GetIO();
	ImVec2	mouse = io.MousePos;

	// ----- wheel zoom around cursor -----
	if (state.viewport_visible && io.MouseWheel != 0.0f && mouse_in_viewport(state, mouse)) {
		float doc_x = screen_to_doc_x(state, mouse.x);
		float doc_y = screen_to_doc_y(state, mouse.y);
		float factor = io.MouseWheel > 0.0f ? 1.1f : 1.0f / 1.1f;
		float new_scale = state.view_scale * factor;
		if (new_scale < 0.05f) new_scale = 0.05f;
		if (new_scale > 40.0f) new_scale = 40.0f;
		// keep the doc point under the cursor fixed:
		// screen = origin + (doc + offset) * scale => offset = (mouse - origin)/scale - doc
		float new_offset_x = (mouse.x - state.viewport_origin.x) / new_scale - doc_x;
		float new_offset_y = (mouse.y - state.viewport_origin.y) / new_scale - doc_y;
		EditorSetViewCamera(state, new_scale, new_offset_x, new_offset_y);
	}

	// ----- click to select / start drag / start marquee -----
	// Only respond when the mouse is over the Viewport window, so an overlapping
	// window (e.g. Documents) keeps its own clicks instead of panning the canvas.
	bool hovered = mouse_in_viewport(state, mouse) && ImGui::IsWindowHovered();
	if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !state.dragging &&
		!state.resizing && !state.marquee_active) {
		int handle = resize_handle_hit(state, mouse.x, mouse.y);
		if (handle >= 0) {
			EditorBeginResize(state, handle, mouse.x, mouse.y);
		} else {
			float doc_x = screen_to_doc_x(state, mouse.x);
			float doc_y = screen_to_doc_y(state, mouse.y);
			bapi_ui_component_t hit = EditorHitTest(state, doc_x, doc_y);
			if (io.KeyCtrl) {
				EditorToggleSelectComponent(state, hit);
				if (hit) state.reveal_request = hit;
			} else if (hit) {
				EditorSelectComponent(state, hit);
				state.reveal_request = hit;
				EditorBeginViewDrag(state, mouse.x, mouse.y);
			} else {
				// empty space: begin marquee box-select
				state.marquee_active	= true;
				state.marquee_start_x	= doc_x;
				state.marquee_start_y	= doc_y;
				state.marquee_cur_x		= doc_x;
				state.marquee_cur_y		= doc_y;
			}
		}
	}

	// ----- right-click context menu -----
	if (state.viewport_visible && ImGui::IsWindowHovered() &&
		ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
		float doc_x = screen_to_doc_x(state, mouse.x);
		float doc_y = screen_to_doc_y(state, mouse.y);
		bapi_ui_component_t hit = EditorHitTest(state, doc_x, doc_y);
		if (hit) {
			bool already = std::find(state.selection_list.begin(), state.selection_list.end(),
									 hit) != state.selection_list.end();
			if (!already) EditorSelectComponent(state, hit);
		} else {
			EditorSelectComponent(state, nullptr);
		}
		ImGui::OpenPopup("##editor_ctx");
	}
	if (ImGui::BeginPopup("##editor_ctx")) {
		EditorContextMenu(state, state.selection_list);
		ImGui::EndPopup();
	}

	// ----- drag to move -----
	if (state.dragging && state.drag_component) {
		if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
			EditorUpdateViewDrag(state, mouse.x, mouse.y);
		} else {
			// commit every moved component as a single undo step
			std::vector<RectMove> moves;
			for (auto &entry : state.drag_start_rects) {
				bapi_ui_component_t comp = entry.first;
				if (!comp) continue;
				bapi_rect_t new_rect;
				bapi_ui_component_get_rect(comp, &new_rect);
				moves.push_back(RectMove{comp, entry.second, new_rect});
			}
			EditorCommitMultiMove(state, moves);
			EditorEndViewDrag(state);
		}
	}

	// ----- resize -----
	if (state.resizing && state.selection) {
		if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
			EditorUpdateResize(state, mouse.x, mouse.y);
		} else {
			EditorEndResize(state);
		}
	}

	// ----- marquee box-select -----
	if (state.marquee_active) {
		if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
			float doc_x = screen_to_doc_x(state, mouse.x);
			float doc_y = screen_to_doc_y(state, mouse.y);
			state.marquee_cur_x = doc_x;
			state.marquee_cur_y = doc_y;
		} else {
			state.marquee_active = false;
			float x1 = state.marquee_start_x, y1 = state.marquee_start_y;
			float x2 = state.marquee_cur_x, y2 = state.marquee_cur_y;
			bapi_rect_t sel = {x1 < x2 ? x1 : x2, y1 < y2 ? y1 : y2,
							   x1 < x2 ? x2 - x1 : x1 - x2, y1 < y2 ? y2 - y1 : y1 - y2};
			if (sel.w < 4.0f && sel.h < 4.0f) {
				// plain click on empty space clears the selection
				EditorSelectComponent(state, nullptr);
			} else {
				std::vector<bapi_ui_component_t> hits = EditorBoxSelect(state, sel);
				if (io.KeyCtrl || io.KeyShift) {
					for (bapi_ui_component_t comp : hits) {
						if (std::find(state.selection_list.begin(), state.selection_list.end(),
									  comp) == state.selection_list.end()) {
							state.selection_list.push_back(comp);
							state.selection = comp;
						}
					}
				} else {
					state.selection_list = hits;
					state.selection = state.selection_list.empty() ? nullptr
																   : state.selection_list.back();
				}
			}
		}
	}

	// ----- middle mouse pan -----
	// Must be inside the viewport, otherwise a middle-drag on another window
	// would silently pan the canvas.
	if (state.viewport_visible && mouse_in_viewport(state, mouse) &&
		ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
		ImVec2 delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Middle);
		ImGui::ResetMouseDragDelta(ImGuiMouseButton_Middle);
		state.view_offset_x += delta.x / state.view_scale;
		state.view_offset_y += delta.y / state.view_scale;
	}

	// ----- draw the engine content texture -----
	if (state.viewport_visible && g_viewport_texture) {
		ImGui::SetCursorScreenPos(pos);
		ImGui::Image((ImTextureID)g_viewport_texture, ImVec2(avail.x, avail.y));
	}

	// ----- palette drag-and-drop: drop to create at cursor -----
	if (ImGui::BeginDragDropTarget()) {
		if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("BAPI_UI_PALETTE")) {
			bapi_ui_component_type_t type = (bapi_ui_component_type_t)(*(const int *)payload->Data);
			float doc_x = screen_to_doc_x(state, mouse.x);
			float doc_y = screen_to_doc_y(state, mouse.y);
			EditorCreateComponentAt(state, type, doc_x, doc_y);
		}
		if (const ImGuiPayload *payload =
				ImGui::AcceptDragDropPayload("BAPI_UI_TEMPLATE")) {
			std::string path(static_cast<const char *>(payload->Data),
							 (size_t)payload->DataSize);
			// strip trailing NUL that SetDragDropPayload copied
			while (!path.empty() && path.back() == '\0') path.pop_back();
			if (!path.empty()) EditorLoadTemplate(state, path.c_str());
		}
		ImGui::EndDragDropTarget();
	}

	// ----- selection outline overlays -----
	for (bapi_ui_component_t comp : state.selection_list) {
		if (!comp) continue;
		bapi_rect_t rect;
		bapi_ui_component_get_rect(comp, &rect);
		float sx1 = doc_to_window_x(state, rect.x);
		float sy1 = doc_to_window_y(state, rect.y);
		float sx2 = doc_to_window_x(state, rect.x + rect.w);
		float sy2 = doc_to_window_y(state, rect.y + rect.h);
		ImDrawList *dl = ImGui::GetWindowDrawList();
		bool		primary = comp == state.selection;
		dl->AddRect(ImVec2(sx1, sy1), ImVec2(sx2, sy2),
					primary ? IM_COL32(255, 180, 40, 255) : IM_COL32(90, 160, 255, 220), 0.0f,
					ImDrawFlags_None, primary ? 2.0f : 1.0f);
	}

	// ----- resize handles on a single selection -----
	if (state.selection_list.size() == 1 && state.selection) {
		ImDrawList *dl = ImGui::GetWindowDrawList();
		ImVec2		handles[8];
		get_handle_positions(state, handles);
		for (int i = 0; i < 8; i++) {
			dl->AddRectFilled(ImVec2(handles[i].x - 3, handles[i].y - 3),
							  ImVec2(handles[i].x + 3, handles[i].y + 3),
							  IM_COL32(255, 180, 40, 255));
			dl->AddRect(ImVec2(handles[i].x - 3, handles[i].y - 3),
						ImVec2(handles[i].x + 3, handles[i].y + 3), IM_COL32(0, 0, 0, 120));
		}
	}

	// ----- marquee overlay -----
	if (state.marquee_active) {
		float x1 = doc_to_window_x(state, state.marquee_start_x);
		float y1 = doc_to_window_y(state, state.marquee_start_y);
		float x2 = doc_to_window_x(state, state.marquee_cur_x);
		float y2 = doc_to_window_y(state, state.marquee_cur_y);
		ImDrawList *dl = ImGui::GetWindowDrawList();
		ImVec2		p1 = ImVec2(x1 < x2 ? x1 : x2, y1 < y2 ? y1 : y2);
		ImVec2		p2 = ImVec2(x1 < x2 ? x2 : x1, y1 < y2 ? y2 : y1);
		dl->AddRectFilled(p1, p2, IM_COL32(90, 160, 255, 40));
		dl->AddRect(p1, p2, IM_COL32(90, 160, 255, 220));
	}

	// ----- smart-snap guide lines -----
	{
		ImDrawList *dl = ImGui::GetWindowDrawList();
		const ImU32 guide_col = IM_COL32(255, 80, 200, 220);
		// vertical guides span the full viewport height
		for (float gx : state.snap_guides_v) {
			float sx = doc_to_window_x(state, gx);
			dl->AddLine(ImVec2(sx, state.viewport_origin.y),
						ImVec2(sx, state.viewport_origin.y + state.viewport_size.y), guide_col,
						1.0f);
		}
		// horizontal guides span the full viewport width
		for (float gy : state.snap_guides_h) {
			float sy = doc_to_window_y(state, gy);
			dl->AddLine(ImVec2(state.viewport_origin.x, sy),
						ImVec2(state.viewport_origin.x + state.viewport_size.x, sy), guide_col,
						1.0f);
		}
	}

	// ----- empty state hint -----
	if (state.viewport_visible && bapi_ui_get_root_count(state.ui) == 0) {
		ImGui::SetCursorScreenPos(ImVec2(pos.x + 12, pos.y + 12));
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
						   "Empty UI. Pick a component from the Palette to start.");
	}

	ImGui::End();
	ImGui::PopStyleColor();
}
