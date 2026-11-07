#include "../editor.h"
#include "../i18n.h"

#include "internal/scene/ui_internal.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>

// Live WYSIWYG preview of the ACTIVE document's UI. The engine UI is rendered
// into an offscreen SDL texture (same pattern as the viewport) at an
// auto-fit scale, and ImGui mouse/keyboard events over the image are forwarded
// to bapi_ui_update so the document is actually interactive. Interactions that
// change persisted state (value/checked/text/activation) mark the document
// dirty; hover/focus do not. Preview clicks never change the editor selection.

extern "C" {
extern SDL_Renderer *g_editor_renderer;
}

static SDL_Texture *g_preview_texture = nullptr;
static int			g_preview_tex_w = 0;
static int			g_preview_tex_h = 0;

static void preview_ensure_texture(SDL_Renderer *renderer, int w, int h)
{
	if (w <= 0 || h <= 0) {
		if (g_preview_texture) {
			SDL_DestroyTexture(g_preview_texture);
			g_preview_texture = nullptr;
		}
		g_preview_tex_w = 0;
		g_preview_tex_h = 0;
		return;
	}
	if (g_preview_texture && g_preview_tex_w == w && g_preview_tex_h == h) return;
	if (g_preview_texture) {
		SDL_DestroyTexture(g_preview_texture);
		g_preview_texture = nullptr;
	}
	g_preview_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
										  SDL_TEXTUREACCESS_TARGET, w, h);
	g_preview_tex_w = w;
	g_preview_tex_h = h;
}

static void utf8_encode(ImWchar c, char *out, int *length)
{
	if (c < 0x80) {
		out[0] = (char)c;
		*length = 1;
	} else if (c < 0x800) {
		out[0] = (char)(0xC0 | (c >> 6));
		out[1] = (char)(0x80 | (c & 0x3F));
		*length = 2;
	} else if (c < 0x10000) {
		out[0] = (char)(0xE0 | (c >> 12));
		out[1] = (char)(0x80 | ((c >> 6) & 0x3F));
		out[2] = (char)(0x80 | (c & 0x3F));
		*length = 3;
	} else {
		out[0] = (char)(0xF0 | (c >> 18));
		out[1] = (char)(0x80 | ((c >> 12) & 0x3F));
		out[2] = (char)(0x80 | ((c >> 6) & 0x3F));
		out[3] = (char)(0x80 | (c & 0x3F));
		*length = 4;
	}
}

void EditorPreviewPanel(EditorState &state)
{
	ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(28, 28, 32, 255));
	ImGui::Begin(L("Preview"), nullptr,
				 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

	ImVec2	avail = ImGui::GetContentRegionAvail();
	ImVec2	pos   = ImGui::GetCursorScreenPos();
	ImGuiIO &io	  = ImGui::GetIO();
	ImVec2	mouse = io.MousePos;
	bool	window_hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);

	if (!state.ui || avail.x < 8.0f || avail.y < 8.0f) {
		ImGui::End();
		ImGui::PopStyleColor();
		return;
	}

	// doc bounds = union of root component rects
	float minx = 1.0e30f, miny = 1.0e30f, maxx = -1.0e30f, maxy = -1.0e30f;
	int	  root_count = bapi_ui_get_root_count(state.ui);
	for (int i = 0; i < root_count; i++) {
		bapi_rect_t r;
		bapi_ui_component_get_rect(bapi_ui_get_root(state.ui, i), &r);
		if (r.x < minx) minx = r.x;
		if (r.y < miny) miny = r.y;
		if (r.x + r.w > maxx) maxx = r.x + r.w;
		if (r.y + r.h > maxy) maxy = r.y + r.h;
	}
	if (root_count == 0 || maxx <= minx || maxy <= miny) {
		minx = miny = 0.0f;
		maxx = maxy = 1.0f;
	}
	float bw = maxx - minx;
	float bh = maxy - miny;

	// scale: manual zoom (preview_scale > 0) or auto-fit
	float scale = state.preview_scale;
	if (scale <= 0.0f) {
		const float margin = 16.0f;
		float		s	  = std::min((avail.x - margin) / bw, (avail.y - margin) / bh);
		if (s <= 0.01f) s = 0.01f;
		if (s > 16.0f) s = 16.0f;
		scale = s;
	}
	if (window_hovered && io.MouseWheel != 0.0f && io.KeyCtrl) {
		float factor		   = io.MouseWheel > 0.0f ? 1.1f : 1.0f / 1.1f;
		float clamped		   = scale * factor;
		if (clamped < 0.05f) clamped = 0.05f;
		if (clamped > 40.0f) clamped = 40.0f;
		state.preview_scale = clamped;
		scale				= clamped;
	}
	// never render larger than the texture cap
	float cap = std::min(4096.0f / bw, 4096.0f / bh);
	if (scale > cap) scale = cap;

	int tw = (int)std::ceil(bw * scale);
	int th = (int)std::ceil(bh * scale);
	if (tw < 1) tw = 1;
	if (th < 1) th = 1;

	SDL_Renderer *renderer = g_editor_renderer;
	if (renderer) {
		preview_ensure_texture(renderer, tw, th);
		if (g_preview_texture) {
			SDL_Texture *prev_target = SDL_GetRenderTarget(renderer);
			SDL_SetRenderTarget(renderer, g_preview_texture);
			SDL_SetRenderDrawColor(renderer, 28, 28, 32, 255);
			SDL_RenderClear(renderer);
			// engine: screen = (doc + offset) * scale  =>  doc top-left -> (0,0)
			bapi_ui_layout(state.ui);
			bapi_ui_render_ex(state.ui, -minx, -miny, scale);
			SDL_SetRenderTarget(renderer, prev_target);
		}
	}

	const float margin = 8.0f;
	ImVec2		origin(pos.x + margin, pos.y + margin);
	ImVec2		image_size((float)tw, (float)th);
	if (g_preview_texture) {
		ImGui::SetCursorScreenPos(origin);
		ImGui::Image((ImTextureID)g_preview_texture, image_size);
	}

	bool in_img = mouse.x >= origin.x && mouse.x <= origin.x + image_size.x &&
				  mouse.y >= origin.y && mouse.y <= origin.y + image_size.y;
	auto doc_x = [&](float sx) { return (sx - origin.x) / scale + minx; };
	auto doc_y = [&](float sy) { return (sy - origin.y) / scale + miny; };

	bapi_ui_internal *u = (bapi_ui_internal *)state.ui;

	// forward mouse state each frame to keep hover fresh in the doc
	if (window_hovered) {
		bapi_event_t motion = {};
		motion.type			= BAPI_EVENT_MOUSE_MOTION;
		if (in_img) {
			motion.data.motion.x = doc_x(mouse.x);
			motion.data.motion.y = doc_y(mouse.y);
		} else {
			motion.data.motion.x = -1.0e6f;
			motion.data.motion.y = -1.0e6f;
		}
		bapi_ui_update(state.ui, &motion);
	}

	if (in_img) {
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
			bapi_event_t ev = {};
			ev.type			= BAPI_EVENT_MOUSE_BUTTON_DOWN;
			ev.data.button.x = doc_x(mouse.x);
			ev.data.button.y = doc_y(mouse.y);
			bapi_ui_update(state.ui, &ev);
		}
		if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
			bool had_pressed = u->pressed != nullptr;
			bapi_event_t ev	 = {};
			ev.type			 = BAPI_EVENT_MOUSE_BUTTON_UP;
			ev.data.button.x = doc_x(mouse.x);
			ev.data.button.y = doc_y(mouse.y);
			bapi_ui_update(state.ui, &ev);
			if (had_pressed) EditorMarkDirty(state);
		}
		if (io.MouseWheel != 0.0f && !io.KeyCtrl) {
			bapi_event_t ev = {};
			ev.type					= BAPI_EVENT_MOUSE_WHEEL;
			ev.data.wheel.x			= io.MouseWheel;
			ev.data.wheel.mouse_x	= doc_x(mouse.x);
			ev.data.wheel.mouse_y	= doc_y(mouse.y);
			bapi_ui_update(state.ui, &ev);
		}
	} else if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
		// released outside the preview: end any press started inside it
		bapi_event_t ev = {};
		ev.type			= BAPI_EVENT_MOUSE_BUTTON_UP;
		ev.data.button.x = -1.0e6f;
		ev.data.button.y = -1.0e6f;
		bapi_ui_update(state.ui, &ev);
	}

	// keyboard: only when the preview window has focus and ImGui isn't using keys
	bool keyboard_active = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
						   !io.WantCaptureKeyboard;
	if (keyboard_active) {
		auto send_key = [&](uint32_t key) {
			bapi_event_t ev = {};
			ev.type			= BAPI_EVENT_KEY_DOWN;
			ev.data.key.key = key;
			bapi_ui_update(state.ui, &ev);
		};
		if (ImGui::IsKeyPressed(ImGuiKey_Tab)) send_key(KEY_TAB);
		if (ImGui::IsKeyPressed(ImGuiKey_Escape)) send_key(KEY_ESC);
		if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) {
			bool had_focus = u->focused != nullptr;
			send_key(KEY_ENTER);
			if (had_focus) EditorMarkDirty(state);
		}
		if (ImGui::IsKeyPressed(ImGuiKey_Backspace)) {
			bool had_focus = u->focused != nullptr;
			send_key(KEY_BACKSPACE);
			if (had_focus) EditorMarkDirty(state);
		}
		// printable text / IME composition, encoded to UTF-8
		if (io.InputQueueCharacters.Size > 0) {
			char buffer[32];
			int	 length = 0;
			for (int i = 0; i < io.InputQueueCharacters.Size && length < 31; i++) {
				int	 encoded_len;
				char utf8[4];
				utf8_encode(io.InputQueueCharacters[i], utf8, &encoded_len);
				if (length + encoded_len > 31) break;
				memcpy(buffer + length, utf8, (size_t)encoded_len);
				length += encoded_len;
			}
			if (length > 0) {
				buffer[length] = '\0';
				bool had_focus = u->focused != nullptr;
				bapi_event_t ev = {};
				ev.type = BAPI_EVENT_TEXT_INPUT;
				memcpy(ev.data.text.text, buffer, (size_t)length + 1);
				bapi_ui_update(state.ui, &ev);
				if (had_focus) EditorMarkDirty(state);
			}
		}
	}

	ImGui::End();
	ImGui::PopStyleColor();
}
