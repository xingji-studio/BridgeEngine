#pragma once

#include "BridgeEngine.h"
#include <imgui.h>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct SDL_Window;
struct SDL_Renderer;

struct EditorState;
class Command;

// Internal: tracks component ownership for Add/Remove commands sharing one node.
struct ComponentOwner {
	bapi_ui_component_t comp = nullptr;
	bool				attached = false;
	~ComponentOwner();
};
using ComponentOwnerPtr = std::shared_ptr<ComponentOwner>;

// A single open document (scene). EditorState keeps the ACTIVE document's
// fields live on itself for direct access; this struct holds a parked copy
// that is swapped in/out when switching tabs.
	struct Document {
		bapi_ui_t ui = nullptr;
		std::string filepath;
		bool dirty = false;

		~Document()
		{
			// A Document's ui is only non-null while parked (never while it is
			// the active document, whose ui lives on EditorState). Destroy the
			// ui here before the members below are destroyed in reverse
			// declaration order, so all ComponentOwners free detached nodes
			// first (they hold comp pointers into this ui).
			if (ui) bapi_ui_destroy(ui);
		}

	bapi_ui_component_t selection = nullptr;
	std::vector<bapi_ui_component_t> selection_list;

	float view_scale = 1.0f;
	float view_offset_x = 0.0f;
	float view_offset_y = 0.0f;

	std::vector<std::unique_ptr<Command>> undo_stack;
	std::vector<std::unique_ptr<Command>> redo_stack;

	bapi_ui_component_t drag_component = nullptr;
	bapi_rect_t drag_start_rect = {0.0f, 0.0f, 0.0f, 0.0f};
	std::unordered_map<bapi_ui_component_t, bapi_rect_t> drag_start_rects;
	float drag_start_mouse_x = 0.0f;
	float drag_start_mouse_y = 0.0f;
	bool dragging = false;

	int  resize_handle = -1;
	bool resizing = false;

	bool  marquee_active = false;
	float marquee_start_x = 0.0f;
	float marquee_start_y = 0.0f;
	float marquee_cur_x = 0.0f;
	float marquee_cur_y = 0.0f;

	std::unordered_map<bapi_ui_component_t, ComponentOwnerPtr> owner_registry;

	// scene management (M4): the root panel that is currently "the scene", and
	// roots skipped by scene switches (kept as-is, e.g. always-on HUD). Both
	// are editor-only state and are not persisted into the XML.
	bapi_ui_component_t active_scene = nullptr;
	std::unordered_set<bapi_ui_component_t> persistent_roots;

	// layer management: components that are locked (view not editable) or
	// editor-hidden. Both are editor-only and never written to the XML.
	std::unordered_set<bapi_ui_component_t> locked;
	std::unordered_set<bapi_ui_component_t> editor_hidden;
};

// Pending "save changes?" confirmation (M3-C2 dirty intercept).
enum class PendingAction {
	None,
	NewDocument,
	OpenFile,
	OpenProject,
	Quit,
	CloseDoc,
};

struct EditorState {
	bapi_ui_t ui = nullptr;
	std::string filepath;
	bool dirty = false;
	bool wants_quit = false;

	bapi_ui_component_t selection = nullptr;
	std::vector<bapi_ui_component_t> selection_list;

	bool snap_to_grid = false;
	bool show_grid = true;
	float grid_size = 20.0f;

	float view_scale = 1.0f;
	float view_offset_x = 0.0f;
	float view_offset_y = 0.0f;

	// viewport content rect in ImGui window coordinates, refreshed each frame
	ImVec2 viewport_origin = {0.0f, 0.0f};
	ImVec2 viewport_size = {0.0f, 0.0f};
	bool viewport_visible = false;

	std::vector<std::unique_ptr<Command>> undo_stack;
	std::vector<std::unique_ptr<Command>> redo_stack;

	// drag-in-progress state
	bapi_ui_component_t drag_component = nullptr;
	bapi_rect_t drag_start_rect = {0.0f, 0.0f, 0.0f, 0.0f};
	std::unordered_map<bapi_ui_component_t, bapi_rect_t> drag_start_rects;
	float drag_start_mouse_x = 0.0f;
	float drag_start_mouse_y = 0.0f;
	bool dragging = false;

	// smart-snap guide lines shown while dragging, in doc coordinates
	// (transient, cleared each frame): x/y of vertical/horizontal lines.
	std::vector<float> snap_guides_v;
	std::vector<float> snap_guides_h;

	// resize-in-progress state
	int resize_handle = -1; // 0..7, -1 = none
	bool resizing = false;

	// marquee box-select state
	bool marquee_active = false;
	float marquee_start_x = 0.0f;
	float marquee_start_y = 0.0f;
	float marquee_cur_x = 0.0f;
	float marquee_cur_y = 0.0f;

	// ownership registry for components shared between Add/Remove commands
	std::unordered_map<bapi_ui_component_t, ComponentOwnerPtr> owner_registry;

	// clipboard holds detached master clones for Copy/Paste. It is global to
	// EditorState (not per Document) so a component copied in one document can
	// be pasted into another.
	std::vector<ComponentOwnerPtr> clipboard;

	// scene management (M4): active root panel + roots skipped on scene switch
	bapi_ui_component_t active_scene = nullptr;
	std::unordered_set<bapi_ui_component_t> persistent_roots;

	// layer management (editor-only, never persisted)
	std::unordered_set<bapi_ui_component_t> locked;
	std::unordered_set<bapi_ui_component_t> editor_hidden;

	// hierarchy panel search + reveal
	char search_query[128] = {};
	bapi_ui_component_t reveal_request = nullptr;

	// multi-document (M3-C): every open document, plus the active index
	std::vector<std::unique_ptr<Document>> documents;
	int active_doc = -1;

	// When a document is activated programmatically (open/new/project), ImGui's
	// tab bar must be told which tab to select. Holding the target index here
	// makes the Documents panel pass SetSelected to exactly that tab for one
	// frame; -1 means "no pending selection request".
	int tab_select_request = -1;

	// pending "save?" confirmation state
	PendingAction pending_action = PendingAction::None;
	int pending_doc_index = -1;
	std::string pending_path;

	// most-recently-used files, most recent first
	std::vector<std::string> recent_files;
	std::vector<std::string> recent_projects;
	std::string project_path;
	std::string project_engine;
	bool show_welcome = true;

	// optional encryption key for .uix UI documents. Empty means documents are
	// saved/loaded as plain XML. When set, saves write encrypted .uix files and
	// loads require the key.
	char ui_key[64] = {};
	bool key_dialog_open = false;
	char key_dialog_buf[64] = {};

	// preview panel camera: <= 0 = auto-fit to window, > 0 = manual zoom
	float preview_scale = 0.0f;

	// set by View > Reset Layout; the dockspace reapplies the default layout
	bool reset_layout_requested = false;

	// build/run current project (M7): a background thread runs cmake; the
	// output is appended to build_log under build_log_mutex.
	std::atomic<bool> build_running{false};
	std::atomic<bool> build_succeeded{false};
	std::mutex build_log_mutex;
	std::string build_log;
	std::unique_ptr<std::thread> build_thread;
	std::string build_exe_path;

	// selected cmake build configuration (Debug / Release / RelWithDebInfo / MinSizeRel)
	std::string build_config = "Debug";

	// the currently running project process (WIN32). Opaque so editor.h stays
	// free of <windows.h>: process handle as void*, id as uint64_t.
	void *run_process_handle = nullptr;
	uint64_t run_process_id = 0;

	// New Project dialog state (File > New Project...)
	bool new_project_open = false;
	char new_project_name[128] = {};
	char new_project_dir[512] = {};
	char new_project_engine[512] = {};
	bool new_project_ok = false;
	char new_project_message[512] = {};
};

struct Command {
	virtual ~Command() = default;
	virtual void Execute(EditorState &state) = 0;
	virtual void Undo(EditorState &state) = 0;
	virtual const char *Name() const = 0;
};

// document_model.cpp
void EditorNewDocument(EditorState &state);
bool EditorLoadFile(EditorState &state, const char *path);
bool EditorSaveFile(EditorState &state, const char *path);
void EditorActivateDocument(EditorState &state, int index);
void EditorCloseDocument(EditorState &state, int index);
void EditorPushRecentFile(EditorState &state, const char *path);
void EditorLoadRecentFiles(EditorState &state);
void EditorSaveRecentFiles(EditorState &state);
bool EditorOpenProject(EditorState &state, const char *path, std::string &error_out);
void EditorPushRecentProject(EditorState &state, const char *path);
void EditorLoadRecentProjects(EditorState &state);
void EditorSaveRecentProjects(EditorState &state);
// Write/remove <project>/.uixkey from the current UI key (build-time encryption).
void EditorSyncUiKeyFile(EditorState &state);
std::string EditorDocumentTitle(const EditorState &state, int index);
bool EditorDocumentIsDirty(const EditorState &state, int index);
const std::string &EditorDocumentPath(const EditorState &state, int index);
bapi_ui_component_t EditorHitTest(EditorState &state, float doc_x, float doc_y);
const char *EditorComponentTypeName(bapi_ui_component_type_t type);
bool EditorIsContainerType(bapi_ui_component_type_t type);
const char *EditorComponentId(bapi_ui_component_t component);
bapi_ui_component_t EditorFindByPath(EditorState &state, const char *id);

// commands.cpp
enum class NumericField {
	MinValue,
	MaxValue,
	Step,
	Radius,
};

enum class BoolField {
	Checked,
	Relative,
	Visible,
	Enabled,
};

void EditorPushCommand(EditorState &state, std::unique_ptr<Command> cmd);
bool EditorUndo(EditorState &state);
bool EditorRedo(EditorState &state);
bool EditorCanUndo(const EditorState &state);
bool EditorCanRedo(const EditorState &state);
void EditorClearHistory(EditorState &state);
void EditorMarkDirty(EditorState &state);
void EditorAddComponent(EditorState &state, bapi_ui_component_type_t type, const char *id,
						bapi_ui_component_t parent);
void EditorCreateComponentAt(EditorState &state, bapi_ui_component_type_t type, float doc_x,
							float doc_y);
void EditorRemoveComponent(EditorState &state, bapi_ui_component_t comp);
void EditorRemoveComponents(EditorState &state, const std::vector<bapi_ui_component_t> &comps);

// one undo step covering several rect edits at once
struct RectMove {
	bapi_ui_component_t comp;
	bapi_rect_t			old_rect;
	bapi_rect_t			new_rect;
};
void EditorCommitMultiMove(EditorState &state, const std::vector<RectMove> &moves);
void EditorSetComponentRect(EditorState &state, bapi_ui_component_t comp, bapi_rect_t new_rect);
void EditorSetComponentText(EditorState &state, bapi_ui_component_t comp, const char *new_text);
void EditorSetComponentId(EditorState &state, bapi_ui_component_t comp, const char *new_id);
bool EditorIdIsUnique(EditorState &state, bapi_ui_component_t comp, const char *new_id);
void EditorSetComponentColor(EditorState &state, bapi_ui_component_t comp,
							 bapi_ui_color_role_t role, bapi_color_t new_color);
void EditorSetComponentTextSize(EditorState &state, bapi_ui_component_t comp, float new_size);
void EditorSetComponentValue(EditorState &state, bapi_ui_component_t comp, float new_value);
void EditorSetComponentSelectedIndex(EditorState &state, bapi_ui_component_t comp, int new_value);
void EditorSetComponentScrollOffset(EditorState &state, bapi_ui_component_t comp, float new_value);
void EditorSetComponentColumns(EditorState &state, bapi_ui_component_t comp, int new_value);
void EditorSetComponentSides(EditorState &state, bapi_ui_component_t comp, int new_value);
void EditorSetComponentMin(EditorState &state, bapi_ui_component_t comp, float new_value);
void EditorSetComponentMax(EditorState &state, bapi_ui_component_t comp, float new_value);
void EditorSetComponentStep(EditorState &state, bapi_ui_component_t comp, float new_value);
void EditorSetComponentRadius(EditorState &state, bapi_ui_component_t comp, float new_value);
void EditorSetComponentChecked(EditorState &state, bapi_ui_component_t comp, bool new_value);
void EditorSetComponentRelative(EditorState &state, bapi_ui_component_t comp, bool new_value);
void EditorSetComponentVisible(EditorState &state, bapi_ui_component_t comp, bool new_value);
// editor-only layer state (not persisted to XML): lock and editor-hide
void EditorSetLocked(EditorState &state, bapi_ui_component_t comp, bool locked);
void EditorSetEditorHidden(EditorState &state, bapi_ui_component_t comp, bool hidden);
bool EditorIsLocked(const EditorState &state, bapi_ui_component_t comp);
bool EditorIsEditorHidden(const EditorState &state, bapi_ui_component_t comp);
void EditorToggleLocked(EditorState &state, const std::vector<bapi_ui_component_t> &comps);
void EditorToggleEditorHidden(EditorState &state, const std::vector<bapi_ui_component_t> &comps);void EditorSetComponentEnabled(EditorState &state, bapi_ui_component_t comp, bool new_value);
void EditorSetComponentSrc(EditorState &state, bapi_ui_component_t comp, const char *new_src);

// panels
void EditorMenuPanel(EditorState &state);
void EditorKeyDialog(EditorState &state);

// custom window title bar (uikit/titlebar.cpp)
void EditorSetupCustomTitleBar(SDL_Window *window);
bool EditorDrawTitleBar(EditorState &state);
void EditorDocumentsPanel(EditorState &state);
void EditorToolbarPanel(EditorState &state);
void EditorViewportPanel(EditorState &state);
void EditorPreviewPanel(EditorState &state);
void EditorPalettePanel(EditorState &state);
void EditorTreePanel(EditorState &state);
void EditorPropertiesPanel(EditorState &state);
void EditorScenesPanel(EditorState &state);
void EditorWelcomePanel(EditorState &state);
void EditorNewProjectDialog(EditorState &state);

// shared right-click context menu; returns true if it consumed the click.
// comps is the set to operate on (may be the selection).
void EditorContextMenu(EditorState &state, const std::vector<bapi_ui_component_t> &comps);

// viewport interaction helpers
void EditorSetViewCamera(EditorState &state, float scale, float offset_x, float offset_y);
float EditorScreenToDocX(EditorState &state, float screen_x);
float EditorScreenToDocY(EditorState &state, float screen_y);
void EditorBeginViewDrag(EditorState &state, float mouse_x, float mouse_y);
void EditorUpdateViewDrag(EditorState &state, float mouse_x, float mouse_y);
void EditorEndViewDrag(EditorState &state);
void EditorSelectComponent(EditorState &state, bapi_ui_component_t component);
void EditorToggleSelectComponent(EditorState &state, bapi_ui_component_t component);
void EditorClearSelection(EditorState &state);
void EditorCommitMove(EditorState &state, bapi_ui_component_t comp, bapi_rect_t old_rect,
					  bapi_rect_t new_rect);
void EditorRenderEngineView(EditorState &state);
float EditorSnapValue(EditorState &state, float value);
void EditorBeginResize(EditorState &state, int handle, float mouse_x, float mouse_y);
void EditorUpdateResize(EditorState &state, float mouse_x, float mouse_y);
void EditorEndResize(EditorState &state);
void EditorNudgeSelection(EditorState &state, float dx, float dy);
std::vector<bapi_ui_component_t> EditorBoxSelect(EditorState &state, bapi_rect_t rect);

// tree editing helpers
void EditorReparentComponent(EditorState &state, bapi_ui_component_t comp, bapi_ui_component_t new_parent);
void EditorCloneComponent(EditorState &state, bapi_ui_component_t source, bapi_ui_component_t parent);
bapi_ui_component_t EditorInsertClone(EditorState &state, bapi_ui_component_t source,
									  bapi_ui_component_t parent, const char *preferred_id);
void EditorDuplicateSelection(EditorState &state);
void EditorAlignSelection(EditorState &state, const char *align);
void EditorDistributeSelection(EditorState &state, const char *axis);
void EditorMakeSameSize(EditorState &state, const char *mode);
void EditorGroupSelection(EditorState &state);
void EditorUngroupSelection(EditorState &state);
void EditorCopySelection(EditorState &state);
void EditorPasteClipboard(EditorState &state);
void EditorCutSelection(EditorState &state);
void EditorSelectAll(EditorState &state);

// z-order
enum class ReorderOp { Up, Down, Front, Back };
void EditorReorderComponent(EditorState &state, bapi_ui_component_t comp, ReorderOp op);

// scene management
void EditorSwitchScene(EditorState &state, bapi_ui_component_t scene_root);
void EditorToggleScenePersistent(EditorState &state, bapi_ui_component_t root);

// build/run current project
bool EditorCanBuild(const EditorState &state);
void EditorBuildProject(EditorState &state);
bool EditorCanRun(const EditorState &state);
void EditorRunProject(EditorState &state);
bool EditorCanStopRun(const EditorState &state);
void EditorStopRunProject(EditorState &state);
void EditorBuildOutputPanel(EditorState &state);
void EditorUpdateBuildThread(EditorState &state);
void EditorUndoPanel(EditorState &state);

// template / preset library
std::string EditorTemplateDir(const EditorState &state);
bool EditorSaveTemplate(EditorState &state, const char *filepath);
bool EditorLoadTemplate(EditorState &state, const char *filepath);
std::vector<std::string> EditorListTemplates(const EditorState &state);

bool EditorIsAncestor(bapi_ui_component_t node, bapi_ui_component_t candidate);
