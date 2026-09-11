#include "editor.h"
#include "i18n.h"

#include <SDL3/SDL.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

// Custom title bar: no OS frame; ImGui draws the bar with min/max/close buttons
// and drag-to-move. Edge resizing comes from SDL_SetWindowHitTest.

static SDL_Window *g_titlebar_window = nullptr;

static const int kTitleBarHeight = 30;
static const int kTitleBarHitTest = 6;
// Three window-control buttons at the right edge of the bar (min/max/close);
// clicking them must reach ImGui, not start a window drag.
static const int kTitleButtonsWidth = 3 * 46;

static SDL_HitTestResult SDLCALL titlebar_hit_test(SDL_Window *win, const SDL_Point *area,
												   void *data)
{
	(void)win;
	(void)data;
	int w = 0, h = 0;
	SDL_GetWindowSize(win, &w, &h);
	const int x = area->x;
	const int y = area->y;

	const int left	 = x < kTitleBarHitTest;
	const int right	 = x >= w - kTitleBarHitTest;
	const int top	 = y < kTitleBarHitTest;
	const int bottom = y >= h - kTitleBarHitTest;

	if (top && left) return SDL_HITTEST_RESIZE_TOPLEFT;
	if (top && right) return SDL_HITTEST_RESIZE_TOPRIGHT;
	if (bottom && left) return SDL_HITTEST_RESIZE_BOTTOMLEFT;
	if (bottom && right) return SDL_HITTEST_RESIZE_BOTTOMRIGHT;
	if (top) return SDL_HITTEST_RESIZE_TOP;
	if (bottom) return SDL_HITTEST_RESIZE_BOTTOM;
	if (left) return SDL_HITTEST_RESIZE_LEFT;
	if (right) return SDL_HITTEST_RESIZE_RIGHT;

	// The bar itself drags the window (double-click maximize comes for free
	// from the OS on Windows via HTCAPTION).
	if (y < kTitleBarHeight && x < w - kTitleButtonsWidth) return SDL_HITTEST_DRAGGABLE;
	return SDL_HITTEST_NORMAL;
}

#ifdef _WIN32
// Borderless maximize covers the taskbar by default; clamp to the work area.
static WNDPROC g_prev_wndproc = nullptr;

static LRESULT CALLBACK titlebar_wndproc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	LRESULT result = g_prev_wndproc ? CallWindowProc(g_prev_wndproc, hwnd, msg, wParam, lParam)
									: DefWindowProc(hwnd, msg, wParam, lParam);
	if (msg == WM_GETMINMAXINFO) {
		MINMAXINFO *mmi = (MINMAXINFO *)lParam;
		RECT		work;
		if (SystemParametersInfo(SPI_GETWORKAREA, 0, &work, 0)) {
			mmi->ptMaxPosition.x = work.left;
			mmi->ptMaxPosition.y = work.top;
			mmi->ptMaxSize.x	 = work.right - work.left;
			mmi->ptMaxSize.y	 = work.bottom - work.top;
		}
	}
	return result;
}
#endif

void EditorSetupCustomTitleBar(SDL_Window *window)
{
	if (!window) return;
	g_titlebar_window = window;
	SDL_SetWindowBordered(window, false);
	SDL_SetWindowResizable(window, true);
	SDL_SetWindowMinimumSize(window, 960, 600);
	SDL_SetWindowHitTest(window, titlebar_hit_test, nullptr);

#ifdef _WIN32
	HWND hwnd = (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(window),
											 SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
	if (hwnd && !g_prev_wndproc) {
		g_prev_wndproc = (WNDPROC)SetWindowLongPtr(hwnd, GWLP_WNDPROC,
												   (LONG_PTR)titlebar_wndproc);
	}
#endif
}

// True when the window is currently maximized.
static bool window_is_maximized(void)
{
	SDL_Window *win = g_titlebar_window;
	if (!win) return false;
	SDL_WindowFlags flags = SDL_GetWindowFlags(win);
	return (flags & SDL_WINDOW_MAXIMIZED) != 0;
}

// Window-control button drawn as a vector icon (kind: 0 min, 1 max, 2 restore,
// 3 close). Returns true when pressed.
static bool titlebar_control_button(int id, int kind, float x, float w, float h, bool hover_red)
{
	ImVec2 min = ImGui::GetWindowPos();
	ImVec2 p0 = ImVec2(min.x + x, min.y);
	ImVec2 p1 = ImVec2(p0.x + w, p0.y + h);
	ImGui::SetCursorScreenPos(p0);

	ImGui::PushID(id);
	bool hovered = ImGui::IsMouseHoveringRect(p0, p1, false);
	bool pressed = ImGui::InvisibleButton("##ctl", ImVec2(w, h));
	hovered		 = hovered || ImGui::IsItemHovered();
	ImGui::PopID();

	ImDrawList *dl	  = ImGui::GetWindowDrawList();
	ImU32		bg	  = IM_COL32(255, 255, 255, 0);
	if (hovered) bg = hover_red ? IM_COL32(230, 60, 60, 220) : IM_COL32(255, 255, 255, 26);
	ImU32 fg = IM_COL32(205, 205, 210, 255);
	if (hovered) fg = IM_COL32(255, 255, 255, 255);

	if (bg != 0) dl->AddRectFilled(p0, p1, bg);

	const float cxm = (p0.x + p1.x) * 0.5f;
	const float cym = (p0.y + p1.y) * 0.5f;
	const float sz  = 8.0f;
	const float sw  = 1.4f;

	if (kind == 0) {
		dl->AddLine(ImVec2(cxm - sz * 0.5f, cym + 3.0f),
					ImVec2(cxm + sz * 0.5f, cym + 3.0f), fg, sw);
	} else if (kind == 1) {
		dl->AddRect(ImVec2(cxm - sz * 0.5f, cym - sz * 0.5f),
					ImVec2(cxm + sz * 0.5f, cym + sz * 0.5f), fg, 1.0f, 0, sw);
	} else if (kind == 2) {
		float half = sz * 0.5f;
		dl->AddRect(ImVec2(cxm - half, cym - half),
					ImVec2(cxm + half - 2.0f, cym + half - 2.0f), fg, 1.0f, 0, sw);
		dl->AddRect(ImVec2(cxm - half + 2.0f, cym - half + 2.0f),
					ImVec2(cxm + half, cym + half), fg, 1.0f, 0, sw);
	} else {
		dl->AddLine(ImVec2(cxm - sz * 0.5f, cym - sz * 0.5f),
					ImVec2(cxm + sz * 0.5f, cym + sz * 0.5f), fg, sw);
		dl->AddLine(ImVec2(cxm + sz * 0.5f, cym - sz * 0.5f),
					ImVec2(cxm - sz * 0.5f, cym + sz * 0.5f), fg, sw);
	}
	return pressed;
}

// Draw the custom title bar. Returns true if a window control was pressed.
bool EditorDrawTitleBar(EditorState &state)
{
	(void)state;
	if (!g_titlebar_window) return false;

	ImGuiViewport *viewport = ImGui::GetMainViewport();

	ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y));
	ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, kTitleBarHeight));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.06f, 0.08f, 1.00f));
	ImGui::Begin("##TitleBar", nullptr,
				 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
					 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
					 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
					 ImGuiWindowFlags_NoNavFocus);

	const float btn_w = 46.0f;
	const float btn_h = (float)kTitleBarHeight;

	bool close_pressed = false, min_pressed = false, max_pressed = false;
	const float btn0_x = ImGui::GetWindowWidth() - btn_w * 3.0f;
	if (titlebar_control_button(0, 0, btn0_x, btn_w, btn_h, false)) min_pressed = true;
	if (titlebar_control_button(1, window_is_maximized() ? 2 : 1, btn0_x + btn_w, btn_w, btn_h,
								false))
		max_pressed = true;
	if (titlebar_control_button(2, 3, btn0_x + btn_w * 2, btn_w, btn_h, true)) close_pressed = true;

	ImGui::SetCursorPos(ImVec2(10.0f, 6.0f));
	const char *title = state.filepath.empty() ? "BridgeEngine Edit"
											   : state.filepath.c_str();
	ImGui::TextUnformatted(title);

	ImGui::End();
	ImGui::PopStyleColor();
	ImGui::PopStyleVar(3);

	if (close_pressed) {
		state.wants_quit = true;
	} else if (min_pressed) {
		SDL_MinimizeWindow(g_titlebar_window);
	} else if (max_pressed) {
		if (window_is_maximized())
			SDL_RestoreWindow(g_titlebar_window);
		else
			SDL_MaximizeWindow(g_titlebar_window);
	}
	return close_pressed || min_pressed || max_pressed;
}
