# BridgeEngine 内部架构

BridgeEngine 保持公开 BAPI 稳定，并将平台相关实现隔离在 `src/internal/platform/platform.h` 之后。

## 平台层

平台适配器按能力分组：

- `core`：初始化、关闭、计时和日志。
- `window`：窗口、事件和鼠标状态。
- `renderer`：渲染命令。
- `texture`：纹理与图像加载。
- `text`：字体与文字渲染。
- `audio`：音频设备、流和 WAV 加载。
- `sync`：互斥锁。
- `io`：只读文件流。

`plat_get()` 是唯一的平台内部入口。引擎 module 直接使用相应能力组，`plat_interface_t` 不再提供扁平的兼容函数指针。

`plat_interface_t.capabilities` 是内部能力位图，`plat_supports()` 用于查询。SDL3 声明音频和视频能力；XJ380 两者均不声明。运行时始终请求窗口/图形初始化，仅在声明音频能力时追加 `PLAT_INIT_AUDIO`。XJ380 的音频 vtable 保持为 `NULL` 槽位，渲染和内置文字能力不受影响。

不支持的媒体通过既有 BAPI 返回值显式失败：音频/视频初始化返回非零，声音/视频加载返回 `NULL`；每种媒体首次失败写入一条平台 warning。没有返回值的媒体控制和清理函数仍为安全无操作。这是内部 adapter 契约，不新增公开 capability 查询接口。

资源包读取（`src/pack.c`）不经过平台层，直接封装 `thirdparty/rzip/rz_lib.c`（C99，仅依赖标准库），仅桌面端编译；XJ380 使用 `src/pack_stub.c`，所有 `bapi_pack_*` 返回失败并记录一次 warning（rz_lib 依赖 POSIX `sys/stat.h`/`fseeko` 等，XJ380 freestanding 环境不可用）。

桌面资源包在打开时建立两个只读索引：保持归档顺序的节点数组，以及按名字、原始序号排序的查询数组。
按序号枚举为 O(1)，按名称查询为 O(log n)，打开时额外付出 O(n log n) 的排序和 O(n) 内存。
排序使用原始序号打破同名条目的平局，保证查找、大小、整文件读取和流式读取仍选择第一个同名条目。
索引只借用 RZip 节点，不复制文件名或资源数据；关闭时先释放索引，再关闭归档。
索引分配失败时退回链表查询，资源包仍可正常打开。

## 媒体加载（两段式）

媒体加载统一为两段式流程：先把字节从磁盘（`bapi_file_read_alloc`，经平台 `io` 能力组）或资源包（`bapi_pack_read_file_alloc` / `bapi_pack_stream_*`）取到内存，再从内存解码。平台 vtable 为此新增两个槽位：`plat_texture_api_t.load_image_mem` 与 `plat_audio_api_t.load_wav_mem`（SDL3 经 `SDL_IOFromConstMem` 实现；XJ380 保持 `NULL` 槽位即不支持）。

- `bapi_sound_load` / `bapi_texture_load` 在平台同时提供 `io` 与内存解码槽位时走两段式路径，否则回退到原路径加载（保证 XJ380 延迟 GPU 图片加载等旧行为不变）。
- 视频通过引擎内部的 `video_source_t`（read/seek/size/close 回调）把三种字节源喂给 FFmpeg AVIO：磁盘文件（平台 `plat_io_t`）、内存拷贝（`bapi_video_load_from_memory`，视频自持）、包条目流（`bapi_video_load_from_pack_stream`，随用随读，不全量缓冲）。`bapi_video_load` 保持磁盘流式语义。
- 两套接口并存：全量缓冲套（`*_load_from_pack`、`*_load_from_memory`，`src/resource.c` 组合）与流式套（`bapi_pack_stream_*` + `bapi_video_load_from_pack_stream`），由调用方按内存/性能取舍选择。

## 文字后端

桌面 SDL3 后端通过 SDL3_ttf 和 BAPI 文字函数加载、绘制字体。运行时优先从 `assets/text/font.ttf` 加载字体；从源码树直接运行示例时，会回退到 `examples/assets/text/font.ttf`，最后兼容旧项目的 `text/font.ttf`。

`src/text.c` 在字体缓存之上维护有界 LRU 文字纹理缓存。键包括字体实例（字号继续按整数像素量化）、
自持的文字字节和完整 RGBA；位置不进入键，同一标签移动时可复用纹理。命中后只提交纹理绘制，
省去光栅化、纹理创建和文字测量；`bapi_get_text_size` 也复用已绘制文字的尺寸。
绘制目标尺寸仍来自文字测量，预算估算使用实际纹理尺寸，避免把布局尺寸误当纹理存储大小。

桌面默认最多缓存 128 条，RGBA 纹理估算大小与文字键合计不超过 4 MiB；XJ380 为 32 条、256 KiB。
这些预算不含固定缓存元数据、字体、驱动额外开销或一次绘制的临时 surface/texture。
超预算、无法查询纹理大小或键分配失败时仍直接绘制，只是不保留纹理。
字体淘汰时释放依赖它的文字纹理；文字清理和引擎退出时先清理缓存，再关闭字体、TTF 与 renderer，
重新初始化不会复用旧 renderer 的纹理。公开 BAPI 和平台 vtable 无需变更。

XJ380 后端使用 XAPI 内置文字绘制。当前 XJ380 头文件中的 `WSTR` 实际为 `char *`，因此 BAPI 将文字按字节串传给 XAPI，不做 UTF-16 或 `wchar_t` 转换。

XJ380 尚未实现字体文件加载；若 XAPI 要求非 UTF-8 编码，应在独立适配器中处理编码兼容性。

## 引擎生命周期

单实例运行时状态由 `src/core/render_context.c` 管理。该 module 负责平台、窗口、renderer 和 TTF 的启动、失败回滚与关闭顺序。

关闭时先释放视频、音频、纹理和文字资源，再关闭 TTF、renderer、窗口和平台。这不会增加多窗口或多引擎支持，但能避免生命周期状态分散在多个文件级全局变量中。

## 事件

BAPI 自己定义事件枚举和事件结构。平台适配器产生 `plat_event_t`，`src/core/init.c` 将其转换为 `bapi_event_t` 后再由 `bapi_poll_event()` 返回。M4 新增的两个事件 `BAPI_EVENT_MOUSE_WHEEL` 与 `BAPI_EVENT_TEXT_INPUT` **不经过平台适配层**：SDL 后端不映射滚轮、桌面没有独立文本输入队列，它们由编辑器 Preview 面板等消费方直接构造并喂给 `bapi_ui_update`。XJ380 无这些输入来源。

## UI 编辑器（桌面 only）

`editor/` 是一个 C++/Dear ImGui 的独立桌面工具，构建目标为 `bridgeengine_editor`（`BRIDGEENGINE_BUILD_EDITOR`）。它**不**链接引擎运行时作为编辑器框架，而是直接使用公开 BAPI 和 `src/internal/bapi_internal.h` 的 native handle getter 拿到 SDL window/renderer。

- `editor/editor.h`：`EditorState`（全局：snap/grid、viewport 尺寸、多文档容器、recent files、pending 确认状态）和 `Command` 命令基类。每个打开的场景是一个 `Document` 结构（ui、文件路径、dirty、选择、视图相机、undo/redo 栈、拖拽/缩放/框选中间态、owner 注册表、剪贴板）。
- `editor/document_model.cpp`：新建/加载/保存文档（走 `bapi_ui_load_from_xml`/`save_to_xml`）、命中测试、视图坐标换算，以及多文档管理（`EditorActivateDocument` / `EditorCloseDocument`）。
- `editor/commands.cpp`：Add/Remove/SetRect/SetText/SetId/SetColor/SetTextSize/Reparent/Clone/Reorder/MultiRemove/MultiRect/SetSrc 命令 + undo/redo 栈。组件所有权由 `ComponentOwner`（shared_ptr）管理，Add 与 Remove 命令通过 `state.owner_registry` 共享同一 owner，保证 detached 组件恰好释放一次。`MultiRemoveCmd` / `MultiRectCmd` 把一次删除或多拖合并成单步 undo。
- `editor/panels/`：menu、documents（标签页 + 未保存确认弹窗）、toolbar、viewport、preview、palette、tree、properties 面板。

编辑器支持多选（Ctrl+点击 切换、Delete 批量删除、视口批量拖动、框选）、层级树的拖拽重挂载（拖到容器或
空白区域成为子节点/根节点）、复制/粘贴与 Ctrl+D 原地复制（底层是 `bapi_ui_component_clone` 深拷贝 +
唯一 id 去重）、对齐（以主选择为参照，左/中/右/上/中/下）、z 序调整（Edit > Order 或 Home/End/PgUp/PgDn，
底层是 `bapi_ui_insert_root` / `bapi_ui_component_insert_child` 的 index 语义）。视口带文档坐标网格
（工具栏 Grid 开关），拖动可吸附到网格（Snap 开关，`grid_size = 20`）。视口支持 8 向缩放手柄、
方向键 nudge（Shift=10px、Snap 开启时按 grid）。palette 中的类型可直接拖入视口创建组件。

Properties 面板对资源型组件提供 `src` 的 Browse（原生对话框 → `SetSrcCmd`，失败回滚且不记录 undo）；
多选时显示批量区，可对全部选中组件一次性应用文本大小、可见/启用和颜色；Id 输入带实时唯一性校验，
与同级组件重名时标红并拦截提交（`EditorIdIsUnique` 校验兄弟作用域）。
- `editor/platform_dialogs.cpp`：原生文件对话框（Windows，`GetOpenFileNameA`/`GetSaveFileNameA`）。为保证 `fopen`/`bapi_ui_load_from_xml` 两侧一致，走 ANSI 变体，因此**只保证 ASCII 路径**；非 ASCII 字符可能被系统代码页转码损坏，视为已知限制。非 Windows 构建返回空串（视为取消）。

多文档：每个 `Document` 是独立的 UI 树、undo/redo 栈与视图相机。活动文档的字段直接保存在
`EditorState` 上（既有代码无感），切换标签时通过 `swap_doc_state` 把字段换入/换出，活动文档与
`EditorState` 之间用同一套 swap 保持单份活数据。关闭/替换文档时 `Document` 析构先释放 detached
组件的 owner，再释放整棵 UI 树。`Document` 的 `ui` 只在停靠（非活动）时非空，避免与 `EditorState`
的活动 `ui` 重复释放。File > New/Open/Close/Close All/Quit 与关窗都会在 `dirty` 时弹
「Unsaved Changes」确认（保存/丢弃/取消）；最近文件列表（8 条，MRU）持久化到工作目录
`recent_files.txt`。

视口渲染：引擎的 `bapi_ui_render_ex()` 先画进一张 SDL offscreen 纹理（`SDL_TEXTUREACCESS_TARGET`），再经 `ImGui::Image()` 显示，使引擎内容与 ImGui 窗口的 z 排序和裁剪由 ImGui 处理。视图相机映射为 `screen = (doc + offset) * scale`。

实时预览（M4）：`editor/panels/preview.cpp` 与视口同款 offscreen 纹理渲染活动文档，默认自动
适配窗口尺寸，Ctrl+滚轮缩放（`EditorState::preview_scale`：0=自适应，>0=手动缩放）。鼠标/滚轮/
按键/IME 文本经 ImGui IO 转换后直接构造 `bapi_event_t` 喂给 `bapi_ui_update`，预览与编辑共用
同一棵 UI 树（WYSIWYG）：点按钮、拖滑块、滚 SCROLL、敲 INPUT 都即时生效，预览点击不改变
编辑器选择。脏策略——只有会改持久化状态（checkbox/slider/select 的 `checked`/`value`/
`selected_index`、INPUT 文本、激活）的交互才调用 `EditorMarkDirty`；hover/focus 与滚动
（`scroll_offset` 不入 XML）不标脏。每次进入预览窗口都发一次 MOUSE_MOTION 刷新悬停、离开时发
远点清除，`pressed` 在预览外释放时补发 BUTTON_UP 兜底。

项目体系（M6）：编辑器围绕 `.bep` 项目文件组织工程。`templates/project/` 是自包含的项目模板
（CMakeLists.txt、main.c、`assets/text/font.ttf`、`ui/` 示例、`project.bep`），
`editor/project_templates.cpp` 的 `EditorCreateProject` 把模板复制到目标目录并替换
`__PROJECT_NAME__`/`__EXEC_NAME__`/`__ENGINE_DIR__` 占位符，再把 `project.bep` 重命名为
`<name>.bep`（通过 `out_bep_path` 输出实际生成路径）。`editor/project_file.cpp` 解析 `.bep`
（INI 风格，`name`/`engine`/`[documents]` 段，文档为相对路径），`EditorOpenProject` 加载项目
文档并写入最近项目列表。`editor/panels/welcome.cpp` 是启动欢迎页（最近项目/最近文件/新建项目/
打开文档），主窗口启动时默认显示；命令行传 XML 路径时直接进设计界面。新建项目向导
`editor/panels/menu.cpp` 的三步表单（名称→位置→引擎源码）默认预填当前工作目录与
`BRIDGEENGINE_SOURCE_DIR`。

i18n（M6）：`editor/i18n.cpp` 是轻量多语言层，`L(key)` 以英文源串为 key 查表。翻译表在运行时从
`editor/locale/zh_CN.txt`/`en.txt` 加载（`key = value` 行格式，`#` 注释），未命中回退英文 key，
因此局部翻译不会损坏界面。语言偏好持久化在 `editor_settings.txt`，View > 语言 切换时会同时触发
一次默认布局重置。编辑器按 `/utf-8` 编译，启动时通过 `GetGlyphRangesChineseSimplifiedCommon`
加载中文字形，保证简体中文渲染。

构建/运行（M7）：`editor/build_run.cpp` 在后台线程跑 cmake（未配置时先 `-S`/`-B` configure；
检测到 `VCPKG_ROOT` 则带 vcpkg toolchain），用 `_popen` 捕获输出追加到 `state.build_log`
（`build_log_mutex` 保护）。`EditorBuildOutputPanel`（`editor/panels/build_output.cpp`）把日志
渲染在底部 Build Output dock，可清除。Project 菜单的 Build（Ctrl+Shift+B）/ Run（Ctrl+F5）先
构建再从 build 目录递归定位 `<项目名>.exe`，以 exe 所在目录为工作目录启动，保证模板里
`assets/` 相对路径可用。模板项目 `add_subdirectory` 引擎时关闭
`BRIDGEENGINE_BUILD_SHARED/EXAMPLES/TESTS/EDITOR/XJ380`，只产出运行所需的最小静态库。

ImGui 来自 docking 分支源码。构建时优先使用 `BRIDGEENGINE_IMGUI_SOURCE_DIR` 指定的本地源码树（例如 vcpkg 的 buildtrees 拷贝）；否则通过 FetchContent 从 GitHub 拉取 `v1.92.8-docking`。imgui 自带的 CMakeLists 会 `find_package(SDL3 CONFIG REQUIRED)`，与 FetchContent 的 SDL3 冲突，因此只编译其核心源文件和 SDL3/SDL-renderer 后端，直接构建静态库。

默认布局：编辑器内置一套默认 docking 布局（顶部 Documents 标签条，左侧 Hierarchy/Palette 上下排列，中央 Viewport，右侧 Properties/Preview 上下排列）。布局版本号以自定义 `[EditorLayout]` settings 段持久化在 `imgui.ini` 里：当保存的版本早于当前版本（首次启动或升级后）自动应用一次默认布局并把版本号升到当前值，之后尊重用户自排的布局、绝不复写。`View > Reset Layout` 随时可手动恢复默认布局。

## 后续 2.5D

不要在 `src/draw.c` 中加入 2.5D 或世界坐标渲染；基础渲染 module 应保持为轻量的 2D 绘制层。

未来的 2.5D 工作应位于独立 module，例如 `world25d` 或 `render25d`，并通过窄接口连接到现有 renderer 能力。
