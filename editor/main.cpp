#include "editor.h"
#include "i18n.h"
#include "platform_dialogs.h"
#include "project_file.h"

static const char *kUiFileFilter = "UI Documents (*.xml)\0*.xml\0All Files (*.*)\0*.*\0";

#include "internal/bapi_internal.h"

#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include <imgui_internal.h>
#include <SDL3/SDL.h>

#include <cstdio>

// defined in panels/viewport.cpp
extern "C" {
extern SDL_Window	*g_editor_window;
extern SDL_Renderer *g_editor_renderer;
}

// ---------------------------------------------------------------------------
// Default docking layout. The layout version is persisted inside imgui.ini
// (as a custom settings section). When the saved version is older than the
// current one -- first run, or after an upgrade that rearranged the panels --
// the built-in default layout is applied once and the marker bumped; afterwards
// the user's own layout is kept and never overwritten.
// ---------------------------------------------------------------------------
static const int kLayoutVersion = 6;
static int		 g_layout_version = 0;

static void *layout_settings_read_open(ImGuiContext *, ImGuiSettingsHandler *, const char *)
{
	g_layout_version = 0;
	return NULL;
}

static void layout_settings_read_line(ImGuiContext *, ImGuiSettingsHandler *, void *,
									  const char *line)
{
	int version = 0;
	if (sscanf(line, "Version=%d", &version) == 1) g_layout_version = version;
}

static void layout_settings_apply_all(ImGuiContext *, ImGuiSettingsHandler *) {}

static void layout_settings_write_all(ImGuiContext *, ImGuiSettingsHandler *, ImGuiTextBuffer *buf)
{
	buf->appendf("[EditorLayout][Default]\n");
	buf->appendf("Version=%d\n\n", g_layout_version);
}

static void apply_default_layout(ImGuiID dockspace_id)
{
	ImGui::DockBuilderRemoveNode(dockspace_id);
	ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
	ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->WorkSize);

	// Documents as a thin tab strip on top; body = left / center / right columns.
	ImGuiID dock_docs, dock_body;
	ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Up, 0.06f, &dock_docs, &dock_body);

	ImGuiID dock_bottom, dock_main;
	ImGui::DockBuilderSplitNode(dock_body, ImGuiDir_Down, 0.22f, &dock_bottom, &dock_main);

	ImGuiID dock_left, dock_center, dock_right;
	ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Left, 0.20f, &dock_left, &dock_center);
	ImGui::DockBuilderSplitNode(dock_center, ImGuiDir_Right, 0.30f, &dock_right, &dock_center);

	ImGuiID dock_tree, dock_left_rest;
	ImGui::DockBuilderSplitNode(dock_left, ImGuiDir_Up, 0.45f, &dock_tree, &dock_left_rest);

	ImGuiID dock_palette, dock_scenes;
	ImGui::DockBuilderSplitNode(dock_left_rest, ImGuiDir_Up, 0.5f, &dock_palette, &dock_scenes);

	ImGuiID dock_props, dock_preview;
	ImGui::DockBuilderSplitNode(dock_right, ImGuiDir_Up, 0.55f, &dock_props, &dock_preview);

	ImGui::DockBuilderDockWindow(L("Documents"), dock_docs);
	ImGui::DockBuilderDockWindow(L("Hierarchy"), dock_tree);
	ImGui::DockBuilderDockWindow(L("Palette"), dock_palette);
	ImGui::DockBuilderDockWindow(L("Scenes"), dock_scenes);
	ImGui::DockBuilderDockWindow(L("Viewport"), dock_center);
	ImGui::DockBuilderDockWindow(L("Properties"), dock_props);
	ImGui::DockBuilderDockWindow(L("Preview"), dock_preview);
	ImGui::DockBuilderDockWindow(L("Build Output"), dock_bottom);
	ImGui::DockBuilderDockWindow(L("History"), dock_bottom);

	ImGui::DockBuilderFinish(dockspace_id);
}

static void setup_imgui_fonts(void)
{
	ImGuiIO &io = ImGui::GetIO();
	// Prefer a CJK-capable system font so Chinese UI strings render; fall back
	// to the embedded default font (Latin only) when none is available.
#ifdef _WIN32
	const char *candidates[] = {
		"C:\\Windows\\Fonts\\msyh.ttc",  // Microsoft YaHei
		"C:\\Windows\\Fonts\\msyh.ttf",
		"C:\\Windows\\Fonts\\simhei.ttf", // SimHei
		"C:\\Windows\\Fonts\\simsun.ttc", // SimSun
		"C:\\Windows\\Fonts\\msyhl.ttc",
	};
#elif defined(__APPLE__)
	const char *candidates[] = {
		"/System/Library/Fonts/PingFang.ttc",
		"/System/Library/Fonts/Hiragino Sans GB.ttc",
		"/System/Library/Fonts/STHeiti Light.ttc",
		"/System/Library/Fonts/STHeiti Medium.ttc",
	};
#else
	const char *candidates[] = {
		"/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
		"/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc",
		"/usr/share/fonts/wenquanyi/wqy-microhei/wqy-microhei.ttc",
		"/usr/share/fonts/truetype/wqy/wqy-microhei.ttc",
	};
#endif
	for (const char *path : candidates) {
		if (io.Fonts->AddFontFromFileTTF(path, 16.0f, nullptr,
										 io.Fonts->GetGlyphRangesChineseSimplifiedCommon()))
			return;
	}
	io.Fonts->AddFontDefault();
}

static void setup_imgui_style(void)
{
	ImGuiIO &io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	ImGuiStyle &style = ImGui::GetStyle();
	style.WindowRounding	= 2.0f;
	style.FrameRounding		= 2.0f;
	style.WindowBorderSize	= 1.0f;
	style.FramePadding		= ImVec2(6, 4);

	// Black theme. Base on the dark palette, then push the surfaces to near-black.
	ImGui::StyleColorsDark(&style);
	ImVec4 *colors			 = style.Colors;
	colors[ImGuiCol_WindowBg]				= ImVec4(0.04f, 0.04f, 0.05f, 1.00f);
	colors[ImGuiCol_ChildBg]				= ImVec4(0.03f, 0.03f, 0.04f, 1.00f);
	colors[ImGuiCol_PopupBg]				= ImVec4(0.05f, 0.05f, 0.06f, 0.98f);
	colors[ImGuiCol_FrameBg]				= ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
	colors[ImGuiCol_FrameBgHovered]			= ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
	colors[ImGuiCol_FrameBgActive]			= ImVec4(0.15f, 0.15f, 0.17f, 1.00f);
	colors[ImGuiCol_TitleBg]				= ImVec4(0.06f, 0.06f, 0.07f, 1.00f);
	colors[ImGuiCol_TitleBgActive]			= ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
	colors[ImGuiCol_MenuBarBg]				= ImVec4(0.06f, 0.06f, 0.07f, 1.00f);
	colors[ImGuiCol_Tab]					= ImVec4(0.06f, 0.06f, 0.07f, 1.00f);
	colors[ImGuiCol_TabHovered]				= ImVec4(0.13f, 0.13f, 0.15f, 1.00f);
	colors[ImGuiCol_TabActive]				= ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
	colors[ImGuiCol_TabUnfocused]			= ImVec4(0.05f, 0.05f, 0.06f, 1.00f);
	colors[ImGuiCol_TabUnfocusedActive]		= ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
	colors[ImGuiCol_CheckMark]				= ImVec4(0.40f, 0.55f, 0.95f, 1.00f);
	colors[ImGuiCol_Header]					= ImVec4(0.13f, 0.13f, 0.15f, 1.00f);
	colors[ImGuiCol_HeaderHovered]			= ImVec4(0.18f, 0.18f, 0.21f, 1.00f);
	colors[ImGuiCol_HeaderActive]			= ImVec4(0.20f, 0.20f, 0.23f, 1.00f);
	colors[ImGuiCol_Button]					= ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
	colors[ImGuiCol_ButtonHovered]			= ImVec4(0.18f, 0.18f, 0.21f, 1.00f);
	colors[ImGuiCol_ButtonActive]			= ImVec4(0.22f, 0.22f, 0.25f, 1.00f);
	colors[ImGuiCol_Separator]				= ImVec4(0.15f, 0.15f, 0.17f, 1.00f);
	colors[ImGuiCol_Text]					= ImVec4(0.88f, 0.88f, 0.90f, 1.00f);
	colors[ImGuiCol_TextDisabled]			= ImVec4(0.45f, 0.45f, 0.48f, 1.00f);
	colors[ImGuiCol_Border]					= ImVec4(0.14f, 0.14f, 0.16f, 1.00f);
}

// Heights of the custom title bar + menu bar + toolbar (px). The dockspace
// starts below them. Must match titlebar.cpp / menu.cpp / toolbar.cpp.
static const float kTitleBarH = 30.0f;
static const float kMenuBarH  = 28.0f;
static const float kToolbarH  = 36.0f;
static const float kTopChromeH = kTitleBarH + kMenuBarH + kToolbarH;

static void build_dockspace(EditorState &state)
{
	ImGuiID dockspace_id = ImGui::GetID("DockSpace");

	// Dock host spans the whole viewport below the top chrome (title bar,
	// menu bar, toolbar).
	ImGuiViewport *viewport = ImGui::GetMainViewport();
	ImVec2		   pos(viewport->Pos.x, viewport->Pos.y + kTopChromeH);
	ImVec2		   size(viewport->Size.x, viewport->Size.y - kTopChromeH);

	ImGui::SetNextWindowPos(pos);
	ImGui::SetNextWindowSize(size);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
	ImGui::Begin("##DockHost", nullptr,
				 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
					 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
					 ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBringToFrontOnFocus |
					 ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoSavedSettings);
	ImGui::DockSpace(dockspace_id, ImVec2(0, 0), ImGuiDockNodeFlags_PassthruCentralNode);
	ImGui::End();
	ImGui::PopStyleColor();
	ImGui::PopStyleVar(3);

	// Apply the default layout once when the saved layout predates the current
	// version (first run / after an upgrade) or when the user asks to reset it.
	if (state.reset_layout_requested || g_layout_version < kLayoutVersion) {
		apply_default_layout(dockspace_id);
		g_layout_version = kLayoutVersion;
		state.reset_layout_requested = false;
		ImGui::MarkIniSettingsDirty();
	}
}

int main(int argc, char *argv[])
{
	if (bapi_engine_init("BridgeEngine Edit", 1440, 900) != 0) {
		std::fprintf(stderr, "[editor] bapi_engine_init failed\n");
		return 1;
	}

	g_editor_window	  = (SDL_Window *)bapi_internal_get_native_window();
	g_editor_renderer = (SDL_Renderer *)bapi_internal_get_native_renderer();
	if (!g_editor_window || !g_editor_renderer) {
		std::fprintf(stderr, "[editor] native window/renderer unavailable\n");
		bapi_engine_quit();
		return 1;
	}

	// Custom title bar: remove the OS frame, draw our own (min/max/close).
	EditorSetupCustomTitleBar(g_editor_window);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	setup_imgui_style();
	setup_imgui_fonts();

	// persist the default-layout version marker inside imgui.ini
	ImGuiSettingsHandler layout_handler;
	layout_handler.TypeName   = "EditorLayout";
	layout_handler.TypeHash   = ImHashStr("EditorLayout");
	layout_handler.ReadOpenFn = layout_settings_read_open;
	layout_handler.ReadLineFn = layout_settings_read_line;
	layout_handler.ApplyAllFn = layout_settings_apply_all;
	layout_handler.WriteAllFn = layout_settings_write_all;
	ImGui::AddSettingsHandler(&layout_handler);

	if (!ImGui_ImplSDL3_InitForSDLRenderer(g_editor_window, g_editor_renderer)) {
		std::fprintf(stderr, "[editor] ImGui_ImplSDL3 init failed\n");
		ImGui::DestroyContext();
		bapi_engine_quit();
		return 1;
	}
	if (!ImGui_ImplSDLRenderer3_Init(g_editor_renderer)) {
		std::fprintf(stderr, "[editor] ImGui_ImplSDLRenderer3 init failed\n");
		ImGui_ImplSDL3_Shutdown();
		ImGui::DestroyContext();
		bapi_engine_quit();
		return 1;
	}

	EditorState state;
	EditorLoadLanguage();
	EditorLoadRecentFiles(state);
	EditorLoadRecentProjects(state);
	EditorNewDocument(state);
	if (argc > 1) {
		EditorLoadFile(state, argv[1]);
		state.show_welcome = false;
	}

	ImVec4 clear_color = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);

	while (!state.wants_quit) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			ImGui_ImplSDL3_ProcessEvent(&event);
			if (event.type == SDL_EVENT_QUIT) {
				if (state.dirty)
					state.pending_action = PendingAction::Quit;
				else
					state.wants_quit = true;
			}
		}

		ImGui_ImplSDL3_NewFrame();
		ImGui_ImplSDLRenderer3_NewFrame();
		ImGui::NewFrame();

		// keyboard shortcuts
		ImGuiIO &io = ImGui::GetIO();
		if (io.WantCaptureKeyboard) {
			// handled by ImGui widgets
		} else if (io.KeyCtrl) {
			if (ImGui::IsKeyPressed(ImGuiKey_Z)) EditorUndo(state);
			if (ImGui::IsKeyPressed(ImGuiKey_Y)) EditorRedo(state);
			if (ImGui::IsKeyPressed(ImGuiKey_C)) EditorCopySelection(state);
			if (ImGui::IsKeyPressed(ImGuiKey_X)) EditorCutSelection(state);
			if (ImGui::IsKeyPressed(ImGuiKey_V)) EditorPasteClipboard(state);
			if (ImGui::IsKeyPressed(ImGuiKey_A)) EditorSelectAll(state);
			if (ImGui::IsKeyPressed(ImGuiKey_D)) EditorDuplicateSelection(state);
			if (ImGui::IsKeyPressed(ImGuiKey_G)) {
				if (io.KeyShift)
					EditorUngroupSelection(state);
				else
					EditorGroupSelection(state);
			}
			if (ImGui::IsKeyPressed(ImGuiKey_S)) {
				if (state.filepath.empty()) {
					std::string path = EditorSaveFileDialog(kUiFileFilter, "editor_out.xml");
					if (!path.empty()) EditorSaveFile(state, path.c_str());
				} else {
					EditorSaveFile(state, state.filepath.c_str());
				}
			}
			if (ImGui::IsKeyPressed(ImGuiKey_O)) {
				std::string path = EditorOpenFileDialog(kUiFileFilter, "assets/ui/demo.xml");
				if (!path.empty()) {
					if (state.dirty) {
						state.pending_action	= PendingAction::OpenFile;
						state.pending_path		= path;
					} else {
						EditorLoadFile(state, path.c_str());
					}
				}
			}
			if (ImGui::IsKeyPressed(ImGuiKey_N)) {
				if (state.dirty)
					state.pending_action = PendingAction::NewDocument;
				else {
					EditorNewDocument(state);
					state.show_welcome = false;
				}
			}
			if (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_P) && !state.project_path.empty()) {
				std::string error;
				EditorSaveProject(state, error);
			}
		}
		if (io.KeyShift && !io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F5))
			EditorStopRunProject(state);
		if (ImGui::IsKeyPressed(ImGuiKey_Delete) && !state.selection_list.empty())
			EditorRemoveComponents(state, state.selection_list);

		// arrow-key nudge
		if (!io.WantCaptureKeyboard) {
			if (state.selection && !io.KeyCtrl) {
				if (ImGui::IsKeyPressed(ImGuiKey_Home))
					EditorReorderComponent(state, state.selection, ReorderOp::Front);
				if (ImGui::IsKeyPressed(ImGuiKey_End))
					EditorReorderComponent(state, state.selection, ReorderOp::Back);
				if (ImGui::IsKeyPressed(ImGuiKey_PageUp))
					EditorReorderComponent(state, state.selection, ReorderOp::Up);
				if (ImGui::IsKeyPressed(ImGuiKey_PageDown))
					EditorReorderComponent(state, state.selection, ReorderOp::Down);
			}
			float step = io.KeyShift ? 10.0f : 1.0f;
			if (state.snap_to_grid) step = state.grid_size;
			float dx = 0.0f;
			float dy = 0.0f;
			if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) dx = -step;
			if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) dx = step;
			if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) dy = -step;
			if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) dy = step;
			if (dx != 0.0f || dy != 0.0f) EditorNudgeSelection(state, dx, dy);
		}

		EditorDrawTitleBar(state);
		EditorMenuPanel(state);
		if (state.show_welcome) {
			EditorWelcomePanel(state);
		} else {
			EditorDocumentsPanel(state);
			EditorToolbarPanel(state);
			build_dockspace(state);
			EditorPalettePanel(state);
			EditorTreePanel(state);
			EditorScenesPanel(state);
			EditorPropertiesPanel(state);
			EditorViewportPanel(state);
			EditorPreviewPanel(state);
			EditorUpdateBuildThread(state);
			EditorBuildOutputPanel(state);
			EditorUndoPanel(state);
		}
		// Render the modal last so it always sits on top of every window,
		// including the welcome screen.
		EditorNewProjectDialog(state);
		EditorKeyDialog(state);

		// Clear the backbuffer first, then render the engine UI into the
		// viewport region, then ImGui windows on top.
		SDL_SetRenderDrawColor(g_editor_renderer,
							   (Uint8)(clear_color.x * 255), (Uint8)(clear_color.y * 255),
							   (Uint8)(clear_color.z * 255), (Uint8)(clear_color.w * 255));
		SDL_RenderClear(g_editor_renderer);

		EditorRenderEngineView(state);

		ImGui::Render();
		ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), g_editor_renderer);
		SDL_RenderPresent(g_editor_renderer);
	}

	ImGui_ImplSDLRenderer3_Shutdown();
	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext();

	// A project process may still be running; stop it so no orphan stays behind.
	if (state.run_process_handle) EditorStopRunProject(state);

	EditorSaveRecentFiles(state);
	EditorSaveRecentProjects(state);
	EditorSaveProjectSession(state);
	if (state.ui) bapi_ui_destroy(state.ui);
	bapi_engine_quit();
	return 0;
}
