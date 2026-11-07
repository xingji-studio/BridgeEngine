# BridgeEngine API 文档

`include/BridgeEngine.h` 是唯一公开 API 和类型契约来源。本文不重复维护声明副本，而是描述每个
公开函数的行为语义、返回值、前置条件和资源所有权。签名以头文件为准，本文件描述的语义以当前
实现为准。发现不一致时以头文件 + 实现为准并回报。

## 全局约定

### 生命周期

1. 先调用 `bapi_engine_init(title, width, height)`。成功返回 `0`，失败返回非零。
2. 初始化成功后，应用可轮询事件、绘制、使用媒体和输入子系统。
3. 退出调用 `bapi_engine_quit()`。它可重复调用；第一次调用按 视频 → 音频 → 纹理 → 文字 → TTF →
   renderer → window → 平台 的顺序停止运行时，并关闭日志。

退出会释放引擎仍追踪的资源。所有 `bapi_texture_t`、`bapi_sound_t`、`bapi_video_t`，以及
`bapi_engine_get_window()` / `bapi_engine_get_renderer()` 返回的借用句柄，退出后全部失效，不得再
使用或释放。

初始化前和退出后：
- `bapi_poll_event()` 返回 `0`，若传入事件则写入 `BAPI_EVENT_UNKNOWN`
- `bapi_engine_get_window()` / `bapi_engine_get_renderer()` 返回 `NULL`
- 基础绘制、`bapi_delay()`、`bapi_get_ticks()` 安全返回（绘制为 no-op，delay 直接返回，ticks 为 0）
- 事件查询函数接受 `NULL` 并返回零值或 `BAPI_EVENT_UNKNOWN`

### 类型与返回约定

不透明资源类型：`bapi_window_t`、`bapi_renderer_t`、`bapi_texture_t`、`bapi_sound_t`、
`bapi_video_t`、`bapi_scene_t`、`bapi_scene_manager_t`、`bapi_level_t`、`bapi_level_manager_t`、
`bapi_file_t`。内部布局定义在 `src/internal/`，公开头不可见。

颜色、矩形、事件、摄像机、向量、圆、按钮、日志配置以及场景/关卡回调结构的精确布局都定义在
`BridgeEngine.h`。

- 创建/加载类函数：返回 `NULL` 表示失败
- 状态/操作类函数：通常 `0` 成功、非零失败
- 无返回值的函数是安全的命令式操作，任何状态下调用都不会崩溃
- 指针参数可为 `NULL` 的具体情况在各函数条目中说明

### 平台媒体能力

桌面 SDL3 后端支持 WAV 音频和 FFmpeg 视频。XJ380 保留窗口、基础绘制和内置文字，不支持音频或
视频：`bapi_audio_init()` 和 `bapi_video_init()` 返回非零，`bapi_sound_load()` 和 `bapi_video_load()`
返回 `NULL`，首次失败通过平台 warning 记录原因。无返回值的媒体控制与清理函数可安全调用。公开
API 不提供 capability 查询；调用方检查既有返回值即可。

### 资源所有权

调用方可在运行时用 `bapi_texture_destroy`、`bapi_sound_free`、`bapi_video_free` 提前释放资源；
不释放则引擎退出统一回收。场景、关卡及其 manager 由调用方创建并持有，`*_manager_destroy` 会
级联释放其管理的 scene/level。

## 引擎、事件与渲染

### 引擎生命周期

```c
typedef enum {
    BAPI_RENDERER_AUTO   = 0,
    BAPI_RENDERER_SDL    = 1,
    BAPI_RENDERER_VULKAN = 2,
} bapi_renderer_backend_t;

int bapi_engine_init(const char *title, int width, int height);
int bapi_engine_init_ex(const char *title, int width, int height,
                        bapi_renderer_backend_t backend);
bapi_renderer_backend_t bapi_engine_get_renderer_backend(void);
```
创建窗口、renderer 并初始化文字后端。`title` 可为 `NULL`。成功返回 0，任一环节失败返回非零，
失败时内部已做回滚。重复调用（已初始化）直接返回 0，不重建。

`bapi_engine_init` 等价于 `BAPI_RENDERER_AUTO`：优先尝试 Vulkan（SDL_GPU 的 `"vulkan"` 驱动），
失败时自动回退 SDL_Renderer。`BAPI_RENDERER_VULKAN` 强制 Vulkan，不可用时初始化失败；
`BAPI_RENDERER_SDL` 强制 SDL_Renderer。`bapi_engine_get_renderer_backend()` 返回实际生效的后端。
UI Kit 内部固定使用 `BAPI_RENDERER_SDL`，因为预览面板依赖 SDL_Renderer 原生句柄。

```c
void bapi_engine_quit(void);
```
停止运行时并关闭日志。可重复调用，幂等。

```c
bapi_window_t bapi_engine_get_window(void);
bapi_renderer_t bapi_engine_get_renderer(void);
```
返回借用句柄，仅初始化期间有效，退出后返回 `NULL`。调用方不拥有，无对应销毁函数。

### 事件

```c
int bapi_poll_event(bapi_event_t *event);
```
从队列取一个事件。取到返回非零并填充 `event`；无事件返回 0。`event` 传 `NULL` 时不消费事件，
只返回"是否有事件可读"。

```c
int bapi_event_get_type(const bapi_event_t *event);
```
返回 `event->type`。`event` 为 `NULL` 时返回 `BAPI_EVENT_UNKNOWN`。

```c
uint8_t bapi_event_get_key_code(const bapi_event_t *event);
int bapi_event_get_mouse_x(const bapi_event_t *event);
int bapi_event_get_mouse_y(const bapi_event_t *event);
int bapi_event_get_mouse_button(const bapi_event_t *event);
int bapi_event_get_motion_x(const bapi_event_t *event);
int bapi_event_get_motion_y(const bapi_event_t *event);
```
各 getter 从 `event` 中取对应字段；`event` 为 `NULL` 时返回 0。鼠标位置返回整型像素坐标（内部
为 float，取整）。仅当事件类型匹配对应 union 分支时字段才有意义。

```c
int bapi_event_is_mouse_button_down(const bapi_event_t *event);
int bapi_event_is_mouse_button_up(const bapi_event_t *event);
int bapi_event_is_mouse_motion(const bapi_event_t *event);
int bapi_event_get_window_width(const bapi_event_t *event);
int bapi_event_get_window_height(const bapi_event_t *event);
```
类型断言：`event` 为 `NULL` 或类型不匹配时返回 0。窗口尺寸 getter 仅在
`BAPI_EVENT_WINDOW_RESIZED` 上有意义，`event` 为 `NULL` 时返回 0。

事件类型 `bapi_event_type_t`：`BAPI_EVENT_QUIT`、`BAPI_EVENT_KEY_DOWN`、`BAPI_EVENT_KEY_UP`、
`BAPI_EVENT_MOUSE_BUTTON_DOWN`、`BAPI_EVENT_MOUSE_BUTTON_UP`、`BAPI_EVENT_MOUSE_MOTION`、
`BAPI_EVENT_MOUSE_WHEEL`、`BAPI_EVENT_TEXT_INPUT`、`BAPI_EVENT_WINDOW_RESIZED`、
`BAPI_EVENT_UNKNOWN`。按键码 0-127 为 ASCII 码，128 起为 `KEY_*` 特殊键（ESC、F1-F12 等）。
`BAPI_BUTTON_LEFT` 为左键（值为 1）。

`BAPI_EVENT_WINDOW_RESIZED` 携带 `data.window { width, height }`，在桌面窗口尺寸变化时产生。

`BAPI_EVENT_MOUSE_WHEEL` 携带 `data.wheel { x, y, mouse_x, mouse_y }`：`x`/`y` 为滚动量，
正值向上（远离用户）、负值向下；`mouse_x`/`mouse_y` 为事件发生时的光标坐标（float）。
`BAPI_EVENT_TEXT_INPUT`
携带 `data.text { text[32] }`：UTF-8 编码的输入文本，最大 31 字节加 NUL。这两个事件不经过
SDL 平台的按键/鼠标映射，由 UI 系统直接消费，桌面端由 UI Kit 预览面板或输入层构造，XJ380
上不可用。

### 渲染控制

```c
void bapi_render_clear(void);
```
将 renderer 清为黑色（0,0,0,255）。

```c
void bapi_render_present(void);
```
把当前帧提交到屏幕。绘制调用必须介于一次 `bapi_render_clear()` 和一次 `bapi_render_present()`
之间。

```c
void bapi_set_render_color(bapi_color_t color);
```
设置后续绘制使用的颜色。带颜色参数的绘制函数会覆盖此设置。

```c
void bapi_delay(uint32_t ms);
```
睡眠 `ms` 毫秒。用于简单帧率控制。

### 帧循环与时间

```c
float bapi_frame_begin(void);
void  bapi_frame_end(void);
```
可选的游戏主循环辅助。`bapi_frame_begin()` 在每帧开头调用，更新 delta 与累计时间并返回本帧
秒数（首帧返回 0，未初始化时返回 0）。`bapi_frame_end()` 提交当前帧，并按 `bapi_set_target_fps()`
的帧率上限补足睡眠。仍可继续手工调用 `bapi_render_present()`；不要在同帧混用两者。

```c
float    bapi_get_delta_time(void);
float    bapi_get_fps(void);
double   bapi_get_elapsed_time(void);
uint64_t bapi_get_frame_count(void);
```
分别返回上一帧的缩放后 delta、平滑 FPS、游戏累计时间（秒，受时间缩放/暂停影响）和已开始的帧数。

```c
void bapi_set_target_fps(int fps);
void bapi_set_time_scale(float scale);
void bapi_set_frame_paused(int paused);
int  bapi_frame_is_paused(void);
```
`target_fps` 为 0 时不限制，范围钳制到 0-1000。`time_scale` 为 delta 与 elapsed 的倍率
（0-100，默认 1）。暂停时 delta 为 0、elapsed 不前进，但帧计数和 FPS 继续更新。这些设置在
`bapi_engine_init()` 之后调用；init 会把帧状态重置为默认值。

```c
int   bapi_frame_steps(float fixed_delta, int max_steps);
float bapi_frame_step_alpha(void);
```
固定步长辅助：把本帧 delta 累积后返回应执行的固定步数（每帧最多 `max_steps` 步，超出上限时
丢弃积压以避免死亡螺旋）。`bapi_frame_step_alpha()` 返回剩余时间占一个固定步长的比例，供渲染
插值使用。每帧只应调用一次 `bapi_frame_steps()`。

### 窗口与显示

```c
void bapi_set_vsync(int enabled);
int  bapi_get_vsync(void);
void bapi_set_fullscreen(int enabled);
int  bapi_get_fullscreen(void);
```
开启/关闭垂直同步与全屏。平台没有对应能力时调用为 no-op，getter 返回当前已生效状态。全屏
和 vsync 在下次 `bapi_engine_init()` 后重置。

```c
void bapi_set_logical_size(int width, int height);
void bapi_get_logical_size(int *width, int *height);
void bapi_get_window_size(int *width, int *height);
```
设置逻辑分辨率（桌面端映射为等比缩放 + 黑边），传 `0,0` 关闭。设置后鼠标事件坐标会自动从
窗口坐标转换到逻辑坐标。`bapi_get_window_size` 返回实际窗口像素尺寸；输出参数可为 `NULL`。

### 图元绘制

所有绘制函数需在初始化后调用，坐标以像素为单位，颜色为 `bapi_color_t`。圆和多边形用线段逼近。
绘制前可通过 `bapi_push_transform()` 设置平移/旋转/缩放，所有图元、纹理和文字都会应用当前变换。

```c
void bapi_draw_pixel(float x, float y, bapi_color_t color);
void bapi_draw_line(float x1, float y1, float x2, float y2, bapi_color_t color);
void bapi_draw_rect(float x, float y, float w, float h, bapi_color_t color);
void bapi_fill_rect(float x, float y, float w, float h, bapi_color_t color);
void bapi_draw_triangle(float x1, float y1, float x2, float y2, float x3, float y3, bapi_color_t color);
void bapi_fill_triangle(float x1, float y1, float x2, float y2, float x3, float y3, bapi_color_t color);
void bapi_fill_circle(float cx, float cy, float radius, bapi_color_t color);
```
`fill_rect`/`fill_circle`/`fill_polygon` 填充实心区域。`draw_triangle`/`draw_circle` 为描边，
`fill_triangle` 为实心三角形；`fill_circle`/`fill_polygon` 使用三角扇形。平台没有
`render_fill_triangle` 槽位时自动退化为逐扫描线填充，保证结果正确。

```c
void bapi_draw_circle(float cx, float cy, float radius, bapi_color_t color);
void bapi_draw_polygon(float cx, float cy, float radius, int sides, bapi_color_t color);
void bapi_fill_polygon(float cx, float cy, float radius, int sides, bapi_color_t color);
```
正多边形，中心 `(cx,cy)`、外接圆半径 `radius`、`sides` 条边。`sides < 3` 时直接返回。第一个顶点
在正上方，顺时针排列。

```c
void bapi_draw_image(const char *filepath, float x, float y, float w, float h);
```
按文件路径缓存纹理并绘制到 `(x,y,w,h)`。首次调用加载，后续命中缓存；绘制结束后释放本次引用，
缓存继续持有纹理。需要完整控制源矩形、旋转、翻转和 tint 时用 `bapi_texture_draw()`。
`filepath` 为 `NULL` 或加载失败时 no-op。

### 颜色与计时

```c
bapi_color_t bapi_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
bapi_color_t bapi_color_from_hex(uint32_t hex_color);
```
`bapi_color` 直接构造颜色。`bapi_color_from_hex` 按 `0xAARRGGBB` 字节序解析（高位 alpha）。
`bapi_color_t` 为 `{r,g,b,a}` 四字节结构。

```c
uint32_t bapi_get_ticks(void);
```
自初始化以来的毫秒数。用于帧耗时测量与计时；游戏主循环优先使用 `bapi_frame_begin()` 系列。

### 2D 变换栈

```c
void bapi_reset_transform(void);
void bapi_push_transform(bapi_vec2_t translation, float rotation, bapi_vec2_t scale);
void bapi_pop_transform(void);
void bapi_apply_camera(const bapi_camera_t *camera);
bapi_vec2_t bapi_transform_point(bapi_vec2_t point);
bapi_vec2_t bapi_transform_vector(bapi_vec2_t vector);
float bapi_transform_scale_factor(void);
```
初始为 identity。`bapi_push_transform` 依次应用 scale、rotation（弧度）、translation，并与当前
变换串联；之后所有图元/纹理/文字绘制都使用栈顶变换。`bapi_apply_camera` 等价于压入摄像机的
world→screen 变换（含平移、旋转、缩放、视口居中）。`bapi_pop_transform` 恢复上一层。
`bapi_transform_point` 变换坐标，`bapi_transform_vector` 只应用线性部分（不含平移），
`bapi_transform_scale_factor` 返回面积缩放因子（均匀缩放下等于缩放倍数）。变换栈最多 32 层，
超出时 push 被忽略。文字只应用位置和缩放，不旋转。

## 文字与鼠标绘图

### 文字

文字基于字体文件，运行时按优先级在 `assets/text/font.ttf`、`examples/assets/text/font.ttf`、
`text/font.ttf` 中查找，按 `size` 缓存最多 8 个字号。

```c
void bapi_text_init(void);
void bapi_text_cleanup(void);
```
`bapi_text_init` 重置文字状态；`bapi_draw_text` / `bapi_get_text_size` 内部会自动调用，一般无需
手动调用。`bapi_text_cleanup` 释放缓存的字体，由引擎退出时调用。

```c
void bapi_draw_text(const char *text, float x, float y, float size, bapi_color_t color);
```
以 `size` 字号绘制文字，左上角在 `(x,y)`。`text` 为 `NULL` 或空串时 no-op。字体加载失败时 no-op。

```c
void bapi_get_text_size(const char *text, float size, float *width, float *height);
```
测量文字在 `size` 字号下的像素尺寸，写入输出参数。输出参数可为 `NULL`（跳过该项）。`text` 为空
或测量失败时输出 0。

### 鼠标绘图

轻量画板：按住左键拖动画线，内部记录最近最多 1000 条线。

```c
void bapi_mouse_init(void);
void bapi_mouse_handle_event(const bapi_event_t *event);
void bapi_mouse_render(void);
void bapi_mouse_clear(void);
void bapi_mouse_cleanup(void);
```
把轮询到的事件喂给 `bapi_mouse_handle_event` 以开始/继续/结束画线；每帧在 `bapi_render_present`
前调用 `bapi_mouse_render` 重绘已记录的线条；`bapi_mouse_clear` 清空记录；`bapi_mouse_cleanup`
由引擎退出时调用。

```c
void bapi_mouse_draw_line(float x1, float y1, float x2, float y2, bapi_color_t color);
```
直接绘制一条线，等价于 `bapi_draw_line`。

## 媒体

### 纹理

```c
bapi_texture_t bapi_texture_load(const char *filepath);
```
从文件加载纹理，返回新句柄（引用计数 1）。失败返回 `NULL`。**不经过缓存**，每次都是新对象。
平台同时支持内存解码时走两段式流程：先把整个文件读入内存缓冲，再从内存解码。

```c
bapi_texture_t bapi_texture_load_from_memory(const void *data, size_t size);
```
从内存中的图片字节解码纹理，语义同 `bapi_texture_load`。输入缓冲仅在调用期间被读取，不被持有；
`data` 为 `NULL`、`size` 为 0 或平台不支持内存解码时返回 `NULL`。不经过缓存。

`.bctex` 是块压缩纹理容器（BC1/BC3，magic `BCTX`）。`bapi_texture_load` /
`bapi_texture_load_from_memory` 会自动识别：Vulkan/SDL_GPU 后端直接把 BC1/BC3 上传为 GPU 压缩
纹理；SDL_Renderer 或其他不支持压缩格式的后端会 CPU 解码成 RGBA 后上传。构建期用
`bridgeengine_bctex_pack` 生成：

```bash
bridgeengine_bctex_pack -f bc1 -o out/assets/game.bctex out/assets/game.bmp
bridgeengine_bctex_pack -f bc3 -o out/assets/ui.bctex out/assets/ui.bmp
```

`bc1` 适合无 alpha 的图集页，`bc3` 保留 alpha。编码器是简单的 4x4 端点 + 最近调色板索引，
适合 UI/图集，不面向高质量法线贴图。

```c
void bapi_texture_destroy(bapi_texture_t texture);
```
释放一次引用；引用计数归零时销毁底层纹理。`texture` 为 `NULL` 时 no-op。

```c
void bapi_texture_render(bapi_texture_t texture, float x, float y);
void bapi_texture_render_ex(bapi_texture_t texture, float x, float y, float w, float h);
```
绘制纹理。前者按原始尺寸绘制到 `(x,y)`，后者拉伸到 `(w,h)`。`texture` 为 `NULL` 时 no-op。

```c
void bapi_texture_get_size(bapi_texture_t texture, int *w, int *h);
```
返回纹理像素宽高。输出参数可为 `NULL`。`texture` 为 `NULL` 时输出 0。

```c
bapi_texture_t bapi_texture_from_file(const char *filepath, int *out_w, int *out_h);
```
从文件加载纹理并**按文件路径缓存**（最多 `BAPI_MAX_CACHED_TEXTURES` 个槽位）。命中缓存时返回
同一句柄并增加引用计数；未命中则加载并写入缓存。缓存满时淘汰最早入缓存的条目（被淘汰的纹理
在仍有外部引用时保持可用，只是不再命中缓存）。`out_w`/`out_h` 可为 `NULL`，返回纹理尺寸。
`filepath` 为空或加载失败返回 `NULL`。

```c
void bapi_texture_cache_clear(void);
```
释放缓存中所有纹理引用。只移除缓存引用，不销毁仍被调用方持有的纹理（引用计数保护）。

```c
bapi_texture_draw_t bapi_texture_draw_default(bapi_rect_t dst);
void bapi_texture_draw(bapi_texture_t texture, const bapi_texture_draw_t *params);
```
一次绘制支持源矩形、目标矩形、归一化 pivot、旋转（弧度）、翻转和 tint。`default` 返回
pivot=(0.5,0.5)、无翻转、白色 tint 的参数；`src` 的宽或高为 0 表示整张纹理。平台没有
`render_texture_ex` 槽位时退化为变换后 AABB 的普通纹理绘制，忽略旋转和 tint。

```c
bapi_atlas_t bapi_atlas_load(const char *manifest_path);
bapi_atlas_t bapi_atlas_load_with_texture(const char *manifest_path, bapi_texture_t page);
bapi_atlas_t bapi_atlas_load_from_memory(const void *manifest, size_t size, bapi_texture_t page);
void         bapi_atlas_destroy(bapi_atlas_t atlas);
```
从文本清单加载图集。清单格式：

```text
BE_ATLAS 1
page atlas.png
size 1024 512
region player 0 0 32 32
region enemy 32 0 32 32
```

`page` 路径相对于清单文件；`region` 名字不能含空白。`bapi_atlas_load()` 自己加载 page 纹理并在
销毁时释放；`bapi_atlas_load_with_texture()` 使用调用方已加载的 page，不取得所有权，调用方需
保证 page 在 atlas 生命周期内有效。`bapi_atlas_load_from_memory()` 直接解析内存中的清单，适合
从 `.rz` 包读取清单与 page 后组装图集。加载失败返回 `NULL`，错误文本可用
`bapi_get_last_error()` 读取。atlas 应在 `bapi_engine_quit()` 之前销毁。

```c
bapi_texture_t             bapi_atlas_get_texture(bapi_atlas_t atlas);
int                        bapi_atlas_get_region_count(bapi_atlas_t atlas);
const bapi_atlas_region_t *bapi_atlas_get_region(bapi_atlas_t atlas, int index);
const bapi_atlas_region_t *bapi_atlas_find_region(bapi_atlas_t atlas, const char *name);
```
查询 page 纹理和 region。name 指针由 atlas 持有，销毁后失效。索引越界返回 `NULL`。

```c
void bapi_atlas_draw(bapi_atlas_t atlas, const char *name, bapi_rect_t dst, int flip,
                     bapi_color_t tint);
```
按 region 绘制到 `dst`，pivot 居中，支持翻转和 tint。找不到 region 时 no-op 并写入错误文本。
需要旋转或自定义 pivot 时，用 `bapi_atlas_find_region()` + `bapi_texture_draw()`。

图集页由构建期工具 `bridgeengine_atlas_pack` 生成：

```bash
bridgeengine_atlas_pack -o out/assets -n game --max-width 2048 \
    --format png --pack out/assets/game.rz res/hero.png res/enemy.png
```

输出 `game.png`（或 `--format bmp`）、`game.atlas`，`--pack` 还会把两者以相对名字写入一个
`.rz` 资源包。`--max-width` 控制页宽，条目之间保留 1px 透明间隔，页尺寸取 2 的幂。当前输出
PNG 为无损重编码；GPU 块压缩（BC/ETC/ASTC）不在本轮范围。

任意烘焙产物可以用通用打包器写入 `.rz`：

```bash
bridgeengine_rz_pack -C out/assets -o out/assets/game.rz game.atlas game.png font.ttf
```

`-C` 先进入资源目录，因此归档内条目名保持相对路径；不传 `-C` 时按当前工作目录解析文件。

从 `.rz` 组装图集的典型流程：

```c
uint8_t *manifest = bapi_pack_read_file_alloc(pack, "game.atlas", &size);
bapi_texture_t page = bapi_texture_load_from_pack(pack, "game.bmp");
bapi_atlas_t atlas = bapi_atlas_load_from_memory(manifest, size, page);
free(manifest);
/* atlas 借用 page；page 需保持存活。 */
```

```c
void bapi_sprite_batch_begin(void);
void bapi_sprite_batch_draw(bapi_texture_t texture, const bapi_texture_draw_t *params);
void bapi_sprite_batch_end(void);
```
精灵批处理：`begin()` 后连续调用 `draw()`，`end()` 时把同纹理的四边形合并为一次带纹理几何
提交。切换纹理或达到 1024 个四边形时自动 flush。适合把同一图集里的 sprite 用 `src` 子矩形
批量绘制。批处理期间不要插入其他 draw/text 调用，否则会破坏绘制顺序。平台没有
`render_geometry_textured` 槽位时退化为逐 sprite 的 `bapi_texture_draw()`，语义不变。
`bapi_sprite_batch_draw()` 在批处理外调用等价于一次普通 `bapi_texture_draw()`。

### 音频

```c
int bapi_audio_init(void);
void bapi_audio_cleanup(void);
```
`bapi_audio_init` 打开音频设备（双声道、44100 Hz）。成功返回 0；平台不支持或设备打开失败返回
非零。`bapi_audio_cleanup` 释放全部声音并关闭设备，引擎退出时调用。

```c
bapi_sound_t bapi_sound_load(const char *filepath);
```
加载 WAV 文件。成功返回句柄（初始音量 1.0、不循环、未播放）；失败或平台不支持返回 `NULL`。
平台同时支持内存解码时走两段式流程：先把整个文件读入内存缓冲，再从内存解码。

```c
bapi_sound_t bapi_sound_load_from_memory(const void *data, size_t size);
```
从内存中的 WAV 字节解码声音，语义同 `bapi_sound_load`。输入缓冲仅在调用期间被读取，不被持有
（解码出的 PCM 归声音句柄所有）；`data` 为 `NULL`、`size` 为 0 或平台不支持内存解码时返回
`NULL`。

```c
int bapi_sound_play(bapi_sound_t sound);
```
播放（或重播）声音。需要先成功 `bapi_audio_init`。成功返回 0，否则非零。重复调用会重新从头播放。

```c
void bapi_sound_set_volume(bapi_sound_t sound, float volume);
void bapi_sound_set_loop(bapi_sound_t sound, int loop);
void bapi_sound_stop(bapi_sound_t sound);
```
音量钳制在 0.0-1.0。`loop` 非零时循环。`stop` 停止播放并清空已排队数据。`sound` 为 `NULL` 时
no-op。

```c
void bapi_sound_update(void);
```
推进所有播放中声音的状态（检测播放结束、非循环声音播完自动停止、循环声音补充数据）。**必须每
帧调用**，否则声音不会正常结束/循环。

```c
void bapi_sound_free(bapi_sound_t sound);
```
停止并释放声音及其资源。`sound` 为 `NULL` 时 no-op。

```c
void  bapi_audio_set_master_volume(float volume);
float bapi_audio_get_master_volume(void);
void  bapi_audio_stop_all(void);
int   bapi_audio_get_active_sound_count(void);
```
`master_volume` 钳制在 0.0-1.0，立即作用于所有已创建声音流和流式音乐；默认 1.0。
`bapi_audio_stop_all` 停止所有播放中的音效，`bapi_audio_get_active_sound_count` 返回当前活动
音效数量（流式音乐不计入）。

### 流式音乐

```c
typedef struct bapi_music_internal *bapi_music_t;

bapi_music_t bapi_music_load(const char *filepath);
bapi_music_t bapi_music_load_from_memory(const void *data, size_t size);
```
加载压缩音乐（MP3/OGG/FLAC/WAV 等 FFmpeg 支持的格式）。`load_from_memory` 会复制输入字节，
调用后即可释放调用方缓冲。需要先成功 `bapi_audio_init()`，否则返回 `NULL`。音乐以 44.1kHz
双声道 F32 流式喂给音频设备，不会把整首解码进内存。

```c
void bapi_music_free(bapi_music_t music);
int  bapi_music_play(bapi_music_t music);
void bapi_music_pause(bapi_music_t music);
void bapi_music_resume(bapi_music_t music);
void bapi_music_stop(bapi_music_t music);
void bapi_music_set_volume(bapi_music_t music, float volume);
void bapi_music_set_loop(bapi_music_t music, int loop);
int  bapi_music_is_playing(bapi_music_t music);
double bapi_music_get_duration(bapi_music_t music);
```
`play` 从开头播放；`pause`/`resume` 暂停与继续；`stop` 停止并回到开头。音量与 master volume
相乘。`is_playing` 在暂停或播完/停止后返回 0。`get_duration` 返回秒数，未知时为 0。
`bapi_music_free` 停止并释放音乐。

```c
void bapi_music_update(void);
```
推进解码：在队列低于约 0.75 秒时继续解码并排队。播放期间必须每帧调用，否则音乐会在缓冲区
耗尽后停顿。循环音乐在 EOF 时自动 seek 回开头。

```c
int bapi_video_init(void);
void bapi_video_cleanup(void);
```
`bapi_video_init` 检查平台能力，成功返回 0，不支持返回非零。`bapi_video_cleanup` 释放全部视频，
引擎退出时调用。

```c
bapi_video_t bapi_video_load(const char *filepath);
```
加载 MP4/AVI/MKV 等 FFmpeg 支持的容器。成功返回句柄（未播放、音量 1.0、不循环）；失败或平台不
支持返回 `NULL`。加载时若音轨可解码则同时初始化音频流。磁盘文件走流式读取（按需 seek/read，
不整文件入内存）。

```c
bapi_video_t bapi_video_load_from_memory(const void *data, size_t size);
```
从内存中的视频字节加载，语义同 `bapi_video_load`。**视频持有输入数据的内部拷贝**，由
`bapi_video_free` 释放，调用方缓冲可在返回后立即释放。`data` 为 `NULL` 或 `size` 为 0 返回
`NULL`。

```c
bapi_video_t bapi_video_load_from_pack_stream(bapi_pack_t pack, const char *name);
```
以流式方式从资源包条目加载视频：不开全量内存缓冲，解码时按需从包内读取。视频持有并最终关闭
该流；**包必须在视频释放前保持打开**。条目不存在或 `name` 为 `NULL` 返回 `NULL`。

```c
void bapi_video_free(bapi_video_t video);
```
停止并释放视频及其解码器、纹理和音频流。`video` 为 `NULL` 时 no-op。退出后句柄失效，不应再次
调用。

```c
int bapi_video_play(bapi_video_t video);
void bapi_video_pause(bapi_video_t video);
void bapi_video_stop(bapi_video_t video);
```
`play` 开始播放（若已停止则从头），成功返回 0，`video` 为 `NULL` 返回 1。`pause` 切换暂停/继续。
`stop` 停止并回到开头，清空音频数据。

```c
void bapi_video_render(bapi_video_t video, int x, int y, int w, int h);
void bapi_video_render_fit(bapi_video_t video, int area_x, int area_y, int area_w, int area_h);
void bapi_video_render_center(bapi_video_t video, int window_w, int window_h);
```
`render` 拉伸到 `(x,y,w,h)`。`render_fit` 在指定区域内等比缩放并居中（letterbox）。`render_center`
在整个窗口中等比缩放并居中。

```c
void bapi_video_set_loop(bapi_video_t video, int loop);
void bapi_video_set_volume(bapi_video_t video, float volume);
int bapi_video_is_playing(bapi_video_t video);
void bapi_video_get_size(bapi_video_t video, int *w, int *h);
```
`loop` 非零循环播放。音量钳制 0.0-1.0。`is_playing` 返回是否正在播放（非暂停）。`get_size` 返回
源视频像素尺寸，输出参数可为 `NULL`。

```c
void bapi_video_update(void);
```
按视频帧率推进当前播放的视频：解码下一帧、更新纹理、播放关联音频。**必须每帧调用**，否则视频
不会前进。播放到结尾时自动停止；`loop` 开启则从头重播。

## UI、输入与摄像机

### 按钮

`bapi_button_t` 是公开结构体，字段在头文件可见。

```c
bapi_button_t *bapi_create_button(float x, float y, float w, float h, const char *text,
                                  bapi_color_t normal_color, bapi_color_t hover_color,
                                  bapi_color_t click_color, bapi_color_t text_color,
                                  float text_size);
void bapi_destroy_button(bapi_button_t *button);
```
创建/销毁按钮。`text` 可为 `NULL`（纯色按钮）。创建时用当前文字后端测量文本尺寸用于居中。

```c
int bapi_button_update(bapi_button_t *button, const bapi_event_t *event);
```
每帧传入事件更新按钮状态。返回非零表示本次发生了一次完整的点击（按下+在按钮内松开）；
`button` 或 `event` 为 `NULL` 返回 0。悬停由鼠标位置决定。

```c
void bapi_button_render(bapi_button_t *button);
```
绘制按钮：按状态（点击 > 悬停 > 常态）填充颜色，画黑色边框，文本居中。

```c
int bapi_button_is_clicked(bapi_button_t *button);
int bapi_button_is_hovered(bapi_button_t *button);
```
状态查询，`button` 为 `NULL` 返回 0。

### 输入状态

```c
void bapi_input_init(void);
void bapi_input_cleanup(void);
```
`bapi_input_init` 清零输入状态并标记初始化。`bapi_input_cleanup` 清零，引擎退出时调用。

```c
void bapi_input_handle_event(const bapi_event_t *event);
void bapi_input_update(void);
```
把轮询到的事件喂给 `handle_event` 更新按键/鼠标状态。`bapi_input_update` **每帧调用一次**：保存
上一帧状态（供 pressed/released 判断）并刷新鼠标位置。

```c
int bapi_is_key_down(uint8_t key);
int bapi_is_key_pressed(uint8_t key);
int bapi_is_key_released(uint8_t key);
```
`down`：当前按住。`pressed`：本帧新按下（上一帧未按下）。`released`：本帧松开。`key` 为 ASCII 码
或 `KEY_*` 特殊键。需先 `bapi_input_handle_event` + `bapi_input_update`。

```c
int bapi_is_mouse_button_down(int button);
int bapi_is_mouse_button_pressed(int button);
int bapi_is_mouse_button_released(int button);
```
与按键语义一致。`button` 用 `BAPI_BUTTON_LEFT`（值为 1）等。鼠标按键状态按位独立跟踪，
多个按键同时按下时分别上报。

```c
float bapi_get_mouse_x(void);
float bapi_get_mouse_y(void);
void bapi_get_mouse_position(float *x, float *y);
```
当前鼠标位置。`get_mouse_position` 输出参数可为 `NULL`。

### 摄像机

`bapi_camera_t` 是公开结构体，保存位置、缩放、旋转和视口尺寸。

```c
void bapi_camera_init(bapi_camera_t *cam, float viewport_w, float viewport_h);
void bapi_camera_set_position(bapi_camera_t *cam, float x, float y);
void bapi_camera_move(bapi_camera_t *cam, float dx, float dy);
void bapi_camera_set_zoom(bapi_camera_t *cam, float zoom);
void bapi_camera_set_rotation(bapi_camera_t *cam, float angle_rad);
void bapi_camera_set_viewport(bapi_camera_t *cam, float w, float h);
```
`init` 把摄像机放在原点、缩放 1、无旋转。`move` 是相对移动，`set_position` 是绝对定位。旋转单位
为弧度。所有函数要求 `cam` 非 `NULL`（不检查）。`zoom` 必须为正：`set_zoom` 会拒绝非正值（保持
原值不变），若直接修改字段导致 `zoom <= 0`，`screen_to_world` 返回摄像机位置、
`get_view_rect` 返回零矩形，避免除零产生 inf/NaN。

```c
void bapi_camera_world_to_screen(bapi_camera_t *cam, float wx, float wy, float *sx, float *sy);
void bapi_camera_screen_to_world(bapi_camera_t *cam, float sx, float sy, float *wx, float *wy);
bapi_vec2_t bapi_camera_world_to_screen_v(bapi_camera_t *cam, bapi_vec2_t world);
bapi_vec2_t bapi_camera_screen_to_world_v(bapi_camera_t *cam, bapi_vec2_t screen);
void bapi_camera_get_view_rect(bapi_camera_t *cam, bapi_rect_t *out_rect);
```
坐标转换：世界到屏幕 = 世界坐标减去摄像机位置 → 绕摄像机旋转 → 乘缩放 → 平移视口中心。
`screen_to_world` 为其逆变换。`*_v` 版本返回向量。`get_view_rect` 输出摄像机可见的世界范围
矩形（`out_rect` 不得为 `NULL`）。

## 场景实体

```c
typedef struct bapi_entity_internal *bapi_entity_t;
typedef void (*bapi_entity_update_fn)(bapi_entity_t entity, float delta_time);

bapi_entity_t bapi_entity_create(const char *name);
void          bapi_entity_destroy(bapi_entity_t entity);
```
轻量场景实体：局部 transform（位置/旋转/缩放）、父子层级、可选 sprite、更新回调。父实体
拥有子实体，`bapi_entity_destroy()` 会递归销毁整棵子树；`bapi_entity_remove_child()` 只解除
挂载，不销毁子实体。`name` 超过 63 字节会被截断。

```c
void        bapi_entity_set_position(bapi_entity_t entity, bapi_vec2_t position);
bapi_vec2_t bapi_entity_get_position(bapi_entity_t entity);
void        bapi_entity_set_rotation(bapi_entity_t entity, float radians);
float       bapi_entity_get_rotation(bapi_entity_t entity);
void        bapi_entity_set_scale(bapi_entity_t entity, bapi_vec2_t scale);
bapi_vec2_t bapi_entity_get_scale(bapi_entity_t entity);

bapi_vec2_t bapi_entity_get_world_position(bapi_entity_t entity);
float       bapi_entity_get_world_rotation(bapi_entity_t entity);
bapi_vec2_t bapi_entity_get_world_scale(bapi_entity_t entity);
```
局部变换顺序为 scale → rotation → translation。world 查询按父链累积；旋转在父级有非均匀缩放时
是近似值（把父链旋转直接相加），需要精确矩阵时用 `bapi_entity_draw()` 内部的变换栈。

```c
int           bapi_entity_add_child(bapi_entity_t parent, bapi_entity_t child);
int           bapi_entity_remove_child(bapi_entity_t parent, bapi_entity_t child);
bapi_entity_t bapi_entity_get_parent(bapi_entity_t entity);
int           bapi_entity_get_child_count(bapi_entity_t entity);
bapi_entity_t bapi_entity_get_child(bapi_entity_t entity, int index);
bapi_entity_t bapi_entity_find_child(bapi_entity_t entity, const char *name);
```
`add_child` 拒绝重复挂载、自引用和成环。`find_child` 只查直接子节点。

```c
void bapi_entity_set_visible(bapi_entity_t entity, int visible);
int  bapi_entity_is_visible(bapi_entity_t entity);
void bapi_entity_set_texture(bapi_entity_t entity, bapi_texture_t texture, bapi_rect_t source);
void bapi_entity_set_tint(bapi_entity_t entity, bapi_color_t tint);
void bapi_entity_set_update(bapi_entity_t entity, bapi_entity_update_fn update);
void bapi_entity_update(bapi_entity_t entity, float delta_time);
void bapi_entity_draw(bapi_entity_t entity);
```
`source` 的宽或高为 0 时使用整张纹理。带 sprite 的实体以自身位置作为 sprite 中心（pivot 0.5,
0.5）。`draw` 会压入实体变换、绘制 sprite、递归绘制子实体，再弹出变换；可见性对子树生效。
纹理是借用句柄，实体不负责释放。`update` 先调用自身回调，再递归更新子实体（不受可见性影响）。

## 世界、输入动作与 Tilemap

### 世界运行时

```c
bapi_world_t  world = bapi_world_create();
bapi_entity_t player = bapi_world_spawn(world, "player");
bapi_world_update(world, dt);
bapi_world_draw(world);
bapi_world_destroy(world);
```

- 世界拥有根实体，按加入顺序更新/绘制
- `bapi_world_despawn()` 会立即从可见列表移除，真正销毁延迟到当前 update 结束
- `bapi_world_set_paused()` 只跳过 update，不影响 draw
- `bapi_world_draw()` 使用世界摄像机，绘制后重置变换栈

### 输入动作

```c
bapi_input_action_clear();
bapi_input_action_bind("left", 'a');
bapi_input_action_bind("left", 'j');
bapi_input_action_bind_mouse("fire", 0);

if (bapi_input_action_pressed("fire")) shoot();
```

一个动作可绑定多个键/鼠标按钮；`down`/`pressed`/`released` 对所有绑定取 OR。动作名是复制存储的，
调用方字符串可释放。`bapi_input_action_clear()` 会清空全部绑定。

### Tilemap

```c
bapi_tilemap_t map = bapi_tilemap_create(40, 24, 32, 32);
bapi_tilemap_set_tile(map, 4, 5, 2, 1);       /* tile 2, solid */
bapi_rect_t body = {x, y, 16, 16};
bapi_tilemap_move_rect(map, &body, (bapi_vec2_t){dx, dy});
bapi_tilemap_draw(map);
```

- 行优先网格，`tile_id = -1` 表示空
- 无 tileset 时用确定性颜色填充；有 tileset 时按 columns 计算源矩形
- `bapi_tilemap_move_rect()` 先 X 后 Y 解算，返回是否被阻挡；会分块推进避免高速穿墙
- 网格外视为空，不是墙

### 精灵动画状态机

```c
bapi_animator_t anim = bapi_animator_create();
bapi_animator_add_clip(anim, "idle", sheet, &idle_frame, &idle_time, 1, 1);
bapi_animator_add_clip(anim, "run", sheet, run_frames, run_times, 4, 1);
bapi_animator_add_transition(anim, "idle", "run", "moving", 1);
bapi_animator_add_transition(anim, "run", "idle", "moving", 0);
bapi_animator_play(anim, "idle");
bapi_animator_set_bool(anim, "moving", 1);
bapi_animator_update(anim, dt);
bapi_entity_set_texture(player, bapi_animator_get_texture(anim),
                        bapi_animator_get_frame(anim));
```

- clip 复制名称、帧数组和时长；纹理句柄是借用的，不销毁
- transition 在推进帧之前按插入顺序求值，第一条匹配的 `from + bool 参数` 生效
- 非循环 clip 到末帧后 `bapi_animator_is_finished()` 返回 1

### 碰撞世界

```c
bapi_collision_world_t cw = bapi_collision_world_create();
bapi_collider_t body = bapi_collision_world_add(
    cw, (bapi_rect_t){x, y, 16, 16}, 1, 1, 1);
int hit = 0;
bapi_collision_world_move(cw, body, (bapi_vec2_t){dx, dy}, &hit);
```

- AABB + layer/mask 过滤；solid 参与移动阻挡
- `query` 返回总命中数，`out` 最多写 `max` 个
- `move` 先 X 后 Y，带扫掠夹紧，避免高速穿墙
- 碰撞过滤：`(A.mask & B.layer) && (B.mask & A.layer)`

### 摄像机控制器

```c
bapi_camera_controller_t cam =
    bapi_camera_controller_create(960, 540);
bapi_camera_controller_set_bounds(cam, (bapi_rect_t){0, 0, map_w, map_h});
bapi_camera_controller_set_deadzone(cam, 140, 90);
bapi_camera_controller_set_smoothing(cam, 8.0f);
bapi_camera_controller_update(cam, dt, player_x, player_y);
bapi_camera_t view;
bapi_camera_controller_get_camera(cam, &view);
```

死区、世界边界夹紧、平滑跟随、`snap`、短促 shake。`update` 的 target 参数优先；传 NaN 时复用上一次目标。

### Tween / easing

```c
bapi_tween_manager_t tweens = bapi_tween_manager_create();
bapi_tween_t pulse = bapi_tween_create(
    tweens, 0.8f, 1.2f, 0.6f, BAPI_EASE_IN_OUT_QUAD, 1);
bapi_tween_manager_update(tweens, dt);
float scale = bapi_tween_value(pulse);
```

8 种 easing，manager 持有 tween，`loop` 自动回绕，非循环到时长后 `finished`。

### 粒子

```c
bapi_particle_system_t dust = bapi_particle_system_create(256);
bapi_particle_config_t cfg = {
    .velocity_min = {-40, -20}, .velocity_max = {40, 10},
    .color_start = {220, 210, 190, 190}, .color_end = {120, 110, 100, 0},
    .life_min = 0.25f, .life_max = 0.55f,
    .size_min = 2, .size_max = 5, .gravity = 60,
};
bapi_particle_system_set_config(dust, &cfg);
bapi_particle_system_emit(dust, 8);
bapi_particle_system_update(dust, dt);
bapi_particle_system_draw(dust);
```

固定容量池，确定性伪随机，颜色按生命周期插值；无纹理，直接画彩色方块。

### 网格寻路

```c
bapi_path_t path = bapi_path_find(map, sx, sy, gx, gy, 0);
for (int i = 0; i < bapi_path_count(path); i++) {
    int x, y;
    bapi_path_get(path, i, &x, &y);
}
bapi_path_destroy(path);
```

A* over `bapi_tilemap_t`，solid tile 阻挡；路径不含起点、包含终点。4 邻接，`allow_diagonal` 开启 8 邻接。

### 定时器

```c
static void on_spawn(void *data) { *(int *)data += 1; }
bapi_timer_manager_t timers = bapi_timer_manager_create();
bapi_timer_after(timers, 1.5f, on_spawn, &counter);
bapi_timer_every(timers, 0.5f, on_spawn, &counter);
bapi_timer_manager_update(timers, dt);
```

帧驱动的一次性/重复定时器；回调内可以安全 cancel，包括 cancel 自己。

### 项目文件扩展 API

```c
bapi_project_t project = bapi_project_create("game", engine_dir);
bapi_project_add_document(project, "ui/menu.xml");
bapi_project_save(project, "game.bep");
bapi_project_destroy(project);
```

- `bapi_project_save()` 使用临时文件 + rename
- 文档路径必须是项目内相对路径；空路径、绝对路径和 `..` 会被拒绝
- UI Kit 和 `bcli` 的 `.bep` 读写都走这套共享 API

## 场景与关卡

### 场景

```c
bapi_scene_t bapi_scene_create(const char *name, bapi_scene_callbacks_t callbacks);
void bapi_scene_destroy(bapi_scene_t scene);
```
创建/销毁场景。`name` 不能为 `NULL`，用于按名字查找和 XML 持久化。`callbacks` 含
`on_enter`/`on_exit`/`on_update`/`on_render` 四个可选回调和一个 `user_data`（非空时会被写入场景）。
`bapi_scene_destroy` 只释放场景本身，不释放 manager。**所有权**：场景加入 manager
（`bapi_scene_manager_add_scene`）后归 manager 所有，`bapi_scene_manager_destroy` 会统一释放；
不要在已注册后直接调用 `bapi_scene_destroy`，否则 manager 中留下悬垂指针并导致双重释放。

```c
const char *bapi_scene_get_name(bapi_scene_t scene);
void *bapi_scene_get_user_data(bapi_scene_t scene);
void bapi_scene_set_user_data(bapi_scene_t scene, void *user_data);
```
查询/设置场景的用户数据。`scene` 为 `NULL` 时 `get_*` 返回 `NULL`，`set_*` 为 no-op。

```c
bapi_scene_manager_t bapi_scene_manager_create(void);
void bapi_scene_manager_destroy(bapi_scene_manager_t manager);
```
创建空 manager。`destroy` 级联销毁全部已添加场景（不重复销毁，添加后归 manager 所有）。最大
`MAX_SCENES`（64）个场景。

```c
int bapi_scene_manager_add_scene(bapi_scene_manager_t manager, bapi_scene_t scene);
```
添加场景。返回 0 成功；`-1` 参数非法；`-2` 已满。

```c
int bapi_scene_manager_switch_scene(bapi_scene_manager_t manager, const char *name);
```
切换到指定名字的场景。若当前有场景则触发其 `on_exit`，再触发新场景 `on_enter`。返回 0 成功；
`-1` 参数非法；`-2` 未找到。

```c
bapi_scene_t bapi_scene_manager_get_current_scene(bapi_scene_manager_t manager);
bapi_scene_t bapi_scene_manager_get_scene(bapi_scene_manager_t manager, const char *name);
```
查询当前场景 / 按名字查找。未找到返回 `NULL`。

```c
void bapi_scene_manager_update(bapi_scene_manager_t manager, float delta_time);
void bapi_scene_manager_render(bapi_scene_manager_t manager);
```
把更新/渲染派发给当前场景的回调。无当前场景或回调未设置时 no-op。

### 关卡

```c
bapi_level_t bapi_level_create(const char *name, int index, bapi_level_callbacks_t callbacks);
void bapi_level_destroy(bapi_level_t level);
```
创建/销毁关卡。`name` 不能为 `NULL`。`index` 是逻辑关卡序号，用于按序号导航（0-255 内有效）。
`callbacks` 结构与场景一致（`on_load`/`on_unload`/`on_update`/`on_render` + `user_data`）。
**所有权**：与场景相同，关卡加入 manager 后归 manager 所有，不要在已注册后直接调用
`bapi_level_destroy`。

```c
const char *bapi_level_get_name(bapi_level_t level);
int bapi_level_get_index(bapi_level_t level);
void *bapi_level_get_user_data(bapi_level_t level);
void bapi_level_set_user_data(bapi_level_t level, void *user_data);
```
查询/设置。`level` 为 `NULL` 时 `get_name`/`get_user_data` 返回 `NULL`，`get_index` 返回 -1。

```c
bapi_level_manager_t bapi_level_manager_create(void);
void bapi_level_manager_destroy(bapi_level_manager_t manager);
```
创建空 manager；`destroy` 级联销毁全部关卡。最大 `MAX_LEVELS`（64）个。

```c
int bapi_level_manager_add_level(bapi_level_manager_t manager, bapi_level_t level);
```
添加关卡，同时登记其 `index` 到序号映射。返回 0 成功；`-1` 参数非法；`-2` 已满。

```c
int bapi_level_manager_load_level(bapi_level_manager_t manager, const char *name);
int bapi_level_manager_load_level_by_index(bapi_level_manager_t manager, int index);
int bapi_level_manager_next_level(bapi_level_manager_t manager);
int bapi_level_manager_previous_level(bapi_level_manager_t manager);
```
加载关卡：卸载当前关卡（触发 `on_unload`）并加载目标关卡（触发 `on_load`）。按名字/序号/下一关/
上一关。返回 0 成功；`-1` 参数非法；`-2` 目标不存在（按名字未找到、序号未登记、已到边界）。

```c
bapi_level_t bapi_level_manager_get_current_level(bapi_level_manager_t manager);
bapi_level_t bapi_level_manager_get_level(bapi_level_manager_t manager, const char *name);
bapi_level_t bapi_level_manager_get_level_by_index(bapi_level_manager_t manager, int index);
int bapi_level_manager_get_level_count(bapi_level_manager_t manager);
```
查询。未找到返回 `NULL`/0。

```c
void bapi_level_manager_update(bapi_level_manager_t manager, float delta_time);
void bapi_level_manager_render(bapi_level_manager_t manager);
```
把更新/渲染派发给当前关卡的回调。

### XML 持久化

受限的 scene/level 格式，不是通用 XML parser。

```c
bapi_scene_manager_t bapi_scene_manager_load_from_xml(const char *filepath);
bapi_level_manager_t bapi_level_manager_load_from_xml(const char *filepath);
```
从 XML 文件加载并返回 manager。只还原名字（和关卡的 index），回调为空；文件打不开或创建失败
返回 `NULL`。

```c
int bapi_scene_manager_save_to_xml(bapi_scene_manager_t manager, const char *filepath);
int bapi_level_manager_save_to_xml(bapi_level_manager_t manager, const char *filepath);
```
把 manager 的全部场景/关卡写到 XML。返回 0 成功；`-1` 参数非法或文件写入失败。

格式：

```xml
<?xml version="1.0" encoding="UTF-8"?>
<scenes>
  <scene name="menu" />
  <scene name="game" />
</scenes>
```

```xml
<?xml version="1.0" encoding="UTF-8"?>
<levels>
  <level name="level_1" index="1" />
</levels>
```

### UI XML

UI XML 是独立于 scene/level 持久化的受限界面描述格式，不是通用 XML parser。

```c
typedef struct bapi_ui_internal *bapi_ui_t;
typedef struct bapi_ui_component_internal *bapi_ui_component_t;

bapi_ui_t bapi_ui_load_from_xml(const char *filepath);
void bapi_ui_destroy(bapi_ui_t ui);
void bapi_ui_update(bapi_ui_t ui, const bapi_event_t *event);
void bapi_ui_render(bapi_ui_t ui);
void bapi_ui_layout(bapi_ui_t ui);
int bapi_ui_was_clicked(bapi_ui_t ui, const char *id);
bapi_ui_component_t bapi_ui_find(bapi_ui_t ui, const char *id);
bapi_ui_component_type_t bapi_ui_component_get_type(bapi_ui_component_t component);
void bapi_ui_component_get_rect(bapi_ui_component_t component, bapi_rect_t *out_rect);
int bapi_ui_component_is_visible(bapi_ui_component_t component);
void bapi_ui_component_set_visible(bapi_ui_component_t component, int visible);
int bapi_ui_component_is_enabled(bapi_ui_component_t component);
void bapi_ui_component_set_enabled(bapi_ui_component_t component, int enabled);
const char *bapi_ui_component_get_text(bapi_ui_component_t component);
int bapi_ui_component_set_text(bapi_ui_component_t component, const char *text);
float bapi_ui_component_get_value(bapi_ui_component_t component);
int bapi_ui_component_set_value(bapi_ui_component_t component, float value);
int bapi_ui_component_is_checked(bapi_ui_component_t component);
int bapi_ui_component_is_focused(bapi_ui_component_t component);
int bapi_ui_component_get_selected_index(bapi_ui_component_t component);
int bapi_ui_component_set_selected_index(bapi_ui_component_t component, int index);
float bapi_ui_component_get_scroll_offset(bapi_ui_component_t component);
int bapi_ui_component_set_scroll_offset(bapi_ui_component_t component, float offset);
```

`bapi_ui_load_from_xml` 加载一个 `<ui>` 根节点及其自闭合组件。UI XML 只描述界面的静态结构和
视觉属性，不在 XML 里绑定 C 回调；业务逻辑应通过组件 `id` 在 C 代码中处理。组件按照 XML
出现的顺序渲染，后面的组件会覆盖前面的组件。UI 对象拥有组件树、文本和纹理/视频资源，
必须使用 `bapi_ui_destroy` 释放。文件打不开、组件属性非法、未知组件、重复 `id` 或资源加载
失败时返回 `NULL`。

`bapi_ui_update` 将事件派发给当前最上层的可交互组件。重叠组件按 XML 顺序决定层级，后出现的
组件优先命中；鼠标必须在同一组件内按下和释放才算点击。`bapi_ui_was_clicked(ui, id)` 返回
1；查询非按钮或未知 `id` 返回 0。点击状态在下一次 `bapi_ui_update` 时更新。

`bapi_ui_update` 还接受以下输入事件：

- `BAPI_EVENT_MOUSE_WHEEL`：以事件坐标从最上层组件向父级查找最近的 `scroll` 容器，更新其
  `scroll_offset`；单步滚动量为容器高度 10%，结果夹在 0 与 `内容高度 - 视口高度` 之间。坐标
  不在任何组件上时不滚动。
- `BAPI_EVENT_TEXT_INPUT`：把 UTF-8 文本追加到当前焦点 `input` 的文本末尾，按
  `max_length` 截断；无焦点 `input` 时忽略。
- `BAPI_EVENT_KEY_DOWN`：`TAB` 移动焦点到下一个可聚焦组件，`ESC` 清除焦点，`ENTER` 激活
  当前焦点组件，`BACKSPACE` 删除 `input` 末尾一个字节，其余键按 ASCII 追加到焦点 `input`。
- 其余鼠标事件：`MOTION` 刷新悬停，`BUTTON_DOWN` 记录按下，`BUTTON_UP` 在同组件内按下并
  释放时激活（切换 checkbox/radio/toggle 的 `checked`、按指针位置设置 slider 的 `value`、
  循环 `select` 的 `selected_index`）。

`hovered`/`pressed` 是瞬时状态：每次 `bapi_ui_update` 先清空再按事件重建，`pressed` 只在
`BUTTON_UP` 时清除。渲染函数不会修改这些状态。

文件格式约束：

- 根节点必须是 `<ui>`。根节点可以为空：`<ui></ui>` 或 `<ui />`。
- 叶子组件必须是自闭合标签，例如 `<button ... />`；容器组件可以使用成对标签包含子组件。
- 属性必须使用双引号。支持属性跨行。
- 支持 XML 声明 `<?xml version="1.0" encoding="UTF-8"?>` 和 `<!-- comment -->` 注释。
- 支持基础 entity：`&amp;`、`&lt;`、`&gt;`、`&quot;`、`&apos;`。
- 不支持百分比坐标、样式继承、脚本、回调名或通用 XML 解析能力。

通用属性：

- `id`：所有组件必填，必须非空，且在同一个 UI 文件内唯一。C 代码通过它查询按钮状态。
- `x` / `y`：组件左上角位置，单位是当前渲染坐标中的像素。
- `w` / `h`：组件宽高。`rect`、`button`、`image` 必填。
- `visible` / `enabled`：可选布尔值，接受 `true`、`false`、`1`、`0`、`yes`。
- `relative`：可选布尔值。为 `true` 时，组件坐标相对于父容器；默认为绝对坐标。
- `text`：文本组件或交互组件的初始文本。文本中的 `&amp;` 等基础 entity 会被解码。
- 颜色写成 `#RRGGBB` 或 `#RRGGBBAA`。省略 alpha 时按 `255` 处理。

查询 API 返回的是 UI 内部借用句柄，不需要也不能单独释放。`bapi_ui_find` 按 `id` 在整棵
组件树中查找；`bapi_ui_component_get_type` 返回组件类型；`get_rect`、`get_text`、`get_value`
和状态查询用于读取组件当前状态。`set_visible`、`set_enabled`、`set_text`、`set_value`
用于运行时修改组件。组件被 `bapi_ui_destroy` 销毁后，所有借用句柄失效。修改布局组件尺寸、
子组件坐标或滚动偏移后调用 `bapi_ui_layout` 重新计算位置。`Tab` 在可交互组件之间移动焦点，
`Enter` 激活焦点组件，`Esc` 清除焦点；`input` 支持可打印字符和 `KEY_BACKSPACE`，`max_length`
限制文本长度。

组件属性：

```xml
<rect id="panel" x="20" y="16" w="400" h="180" color="#20242CFF" />
```

`rect` 绘制一个实心矩形。必填属性：`id`、`x`、`y`、`w`、`h`、`color`。

```xml
<label id="title" x="40" y="24" text="Main Menu" size="28" color="#FFFFFFFF" />
```

`label` 绘制一段文本。必填属性：`id`、`x`、`y`、`text`、`size`、`color`。文本使用当前引擎
文字系统，字体加载失败时不会绘制文本。

```xml
<button id="start" x="40" y="80" w="180" h="48" text="Start"
        text_size="20" normal="#2F80EDFF" hover="#56CCF2FF"
        click="#1C5DB8FF" text_color="#FFFFFFFF" />
```

`button` 使用内置按钮样式和鼠标状态。必填属性：`id`、`x`、`y`、`w`、`h`、`text`、
`text_size`、`normal`、`hover`、`click`、`text_color`。`normal` 是默认颜色，`hover` 是鼠标
悬停颜色，`click` 是按下状态颜色，`text_color` 是按钮文字颜色。

```xml
<image id="logo" x="260" y="24" w="128" h="128" src="assets/logo.png" />
```

`image` 加载并绘制一张纹理。必填属性：`id`、`x`、`y`、`w`、`h`、`src`。`src` 可以是绝对路径，
也可以是相对路径；相对路径基于 XML 文件所在目录，而不是进程当前工作目录。图片加载失败时，
整个 `bapi_ui_load_from_xml` 返回 `NULL`。

`progress` 和 `slider` 使用 `value`、`min`、`max`、`step` 属性（`step` 仅在非 1 时写出）。内部绘制会把数值归一化到 `[0, 1]`，
因此 `min="20" max="100" value="50"` 会绘制为 37.5% 的进度。`slider` 点击轨道后更新
`value`，`bapi_ui_component_set_value` 也会自动限制到 `[min, max]`。

`checkbox`、`toggle` 和 `radio` 通过 `checked` 查询当前状态；同一父容器中的 `radio` 互斥。
`select`、`list` 和 `tab` 使用 `items` 和 selected index 作为基础选择模型：
`bapi_ui_component_get_selected_index` 查询索引，`bapi_ui_component_set_selected_index` 修改索引。
`scroll` 使用 `bapi_ui_component_get_scroll_offset` / `bapi_ui_component_set_scroll_offset` 管理
垂直偏移，修改后调用 `bapi_ui_layout`；滚动容器外的子组件不会参与命中和绘制。
XJ380 后端会把 `video` 组件降级为不可见、禁用节点，使同一份 UI XML 可以继续加载。

绘制和布局组件：

- `line`：使用 `x`、`y`、`w`、`h` 表示起点和终点偏移，使用 `color` 绘制线段。
- `circle`：使用 `x`、`y`、`radius` 和 `color`。`checked="true"` 时绘制实心圆，否则绘制圆框。
- `polygon`：使用 `x`、`y`、`radius`、`sides` 和 `color` 绘制多边形。
- `border`：使用 `x`、`y`、`w`、`h` 和 `color` 绘制矩形边框。
- `progress`：使用 `x`、`y`、`w`、`h`、`value`、`min`、`max`、`color` 和可选 `color2` 绘制进度条。
- `separator`：使用矩形属性绘制分隔线。
- `panel` / `container`：绘制背景并可包含子组件。
- `row` / `column`：按 `step` 间距沿横向/纵向排列子组件。
- `grid`：按 `columns` 和 `step` 将子组件排列为网格。

交互组件：

- `checkbox`、`radio`、`toggle`：点击后修改 `checked` 状态；同一容器下的 `radio` 互斥。
- `slider`：点击轨道修改 `value`，使用 `min` 和 `max` 限制范围。
- `input`：点击后获得焦点；键盘输入追加到 `text`，`KEY_BACKSPACE` 删除末尾字符。
- `select`、`list`、`tab`：当前提供可查询、可点击的文本组件基础行为，选项数据和弹出菜单由调用方管理。
- `scroll`、`canvas`：作为可嵌套的布局/扩展节点，目前不提供自动滚动或自定义绘制回调。
- `video`：使用 `src` 加载视频并按组件矩形渲染；视频后端加载失败会使整个 UI 加载失败。
- `nine_patch`、`animation`、`tooltip`、`modal`、`popup`：提供 XML 节点和基础渲染/容器行为；高级资源切片、
  动画时间轴、弹层管理和自动定位由调用方通过组件属性或业务代码控制。

典型使用方式是在场景初始化时加载 UI，在事件循环中更新 UI，在场景渲染回调中渲染 UI：

```c
static bapi_ui_t menu_ui;

static void menu_render(bapi_scene_t scene)
{
	(void)scene;
	bapi_ui_render(menu_ui);
}

/* 初始化阶段 */
menu_ui = bapi_ui_load_from_xml("assets/ui/menu.xml");

/* 事件循环中，仅在对应场景活跃时更新 */
bapi_ui_update(menu_ui, &event);
if (bapi_ui_was_clicked(menu_ui, "start")) {
	bapi_scene_manager_switch_scene(scene_manager, "game");
}

/* 退出阶段 */
bapi_ui_destroy(menu_ui);
```

示例：

```xml
<ui>
  <rect id="panel" x="20" y="16" w="400" h="180" color="#20242CFF" />
  <label id="title" x="40" y="24" text="Main Menu" size="28" color="#FFFFFFFF" />
  <button id="start" x="40" y="80" w="180" h="48" text="Start"
          text_size="20" normal="#2F80EDFF" hover="#56CCF2FF"
          click="#1C5DB8FF" text_color="#FFFFFFFF" />
  <image id="logo" x="260" y="24" w="128" h="128" src="assets/logo.png" />
</ui>
```

### UI 持久化与编辑

```c
int bapi_ui_save_to_xml(bapi_ui_t ui, const char *filepath);
```
把组件树序列化回 loader 能读回的 XML 格式。返回 0 成功；`ui` 或 `filepath` 为 `NULL`、文件写入
失败或遇到无法映射的组件类型时返回 -1。

- 坐标输出组件的 `local_rect`（布局前的原始坐标）而不是布局后的 `rect`，因此 row/column/grid
  等布局容器的子组件位置保存后可精确还原。
- `text` 与 `src` 中的 `&`、`<`、`>`、`"`、`'` 会转义为实体；`src` 保存的是 XML 中的原始相对路径，
  不是加载时解析出的绝对路径。
- 属性按「非默认值才写出」：颜色省略等价于默认值、`text_size` 省略等价于 18、`visible`/`enabled`
  省略等价于 `true`、`value`/`min`/`max` 只对 `progress`/`slider` 写出、`step` 只对
  `row`/`column`/`grid` 写出。
- 有子组件的节点写成成对标签，无子组件的节点写成自闭合标签，两种形式 loader 都接受。

```c
bapi_ui_t bapi_ui_load_from_file(const char *filepath, const char *key);
int bapi_ui_save_to_file(bapi_ui_t ui, const char *filepath, const char *key);
```

带可选密钥的 UI 文档读写，用于防止发布资源被直接编辑（.uix 加密格式）。

- `key` 为 `NULL` 或空串：`save_to_file` 等价于 `save_to_xml`，写明文 XML；`load_from_file` 读明文
  XML（等价于 `load_from_xml`）。
- `key` 非空：保存时把 XML 序列化后用密钥做流加密，写成二进制 `.uix` 文件（文件头 `BUIX` +
  校验和 + 长度 + 密文）；加载时用相同密钥解密并按校验和校验，密钥错误、文件被篡改或明文文件
  但传了密钥都会返回 `NULL`。
- 这是混淆级保护（密钥在加载程序内部），用于挡住直接改文件的普通玩家，不是强加密。
- UI Kit 通过「File > UI Encryption Key...」设置密钥，设置后保存的文档即为 .uix。

```c
int bapi_uix_encrypt_file(const char *src_xml_path, const char *dst_uix_path, const char *key);
int bapi_uix_decrypt_file(const char *src_uix_path, const char *dst_xml_path, const char *key);
```

构建期加密/解密工具，供项目模板的 POST_BUILD 使用：仓库保存可编辑的明文 XML，构建时用
`bridgeengine_uix_encrypt` 工具把 `ui/*.xml` 加密成 `*.uix` 输出到产物目录，运行时用同一密钥
`bapi_ui_load_from_file` 加载。两个函数返回 0 成功、-1 失败（密钥为空或文件读写失败）。

UI Kit 在「File > Save Project」时会把 UI 加密密钥写入项目根目录的 `.uixkey` 文件；项目模板的
CMakeLists 构建时自动读取它并加密 `ui/*.xml`。没有 `.uixkey`（或命令行未传
`-DBRIDGEENGINE_UI_KEY=...`）时产物直接复制明文 xml，行为与旧版一致。

```c
void bapi_ui_render_ex(bapi_ui_t ui, float offset_x, float offset_y, float scale);
```
带平移和缩放的渲染：所有组件按 `(rect.x + offset_x) * scale` 计算屏幕坐标，宽高和字号同步缩放。
`scale` 非正时按 1 处理。`ui` 为 `NULL` 时 no-op。`bapi_ui_render` 等价于以
`(0, 0, 1)` 调用。

### UI 组件树编辑

```c
bapi_ui_component_t bapi_ui_component_create(bapi_ui_component_type_t type, const char *id);
void bapi_ui_component_destroy(bapi_ui_component_t component);
bapi_ui_component_t bapi_ui_component_clone(bapi_ui_component_t source);
int bapi_ui_component_add_child(bapi_ui_component_t parent, bapi_ui_component_t child);
int bapi_ui_component_insert_child(bapi_ui_component_t parent, bapi_ui_component_t child, int index);
int bapi_ui_component_remove(bapi_ui_component_t component);
int bapi_ui_add_root(bapi_ui_t ui, bapi_ui_component_t component);
int bapi_ui_insert_root(bapi_ui_t ui, bapi_ui_component_t component, int index);
int bapi_ui_remove_root(bapi_ui_t ui, bapi_ui_component_t component);
```
运行时编辑组件树，供工具或脚本用。`create` 创建孤立组件，新组件默认可见、启用、白色、
`text_size` 18。`add_child` 把 `child` 挂到 `parent` 末尾并设置父指针；`insert_child` 把已脱离
树的 `child` 插入到 `parent` 的第 `index` 个位置（`index` 等于当前子数量即追加），插入前会
拒绝「已挂载的 child」和「会形成环」（把 `parent` 挂进自己的子树）两种非法操作；`remove` 把组件
从父节点的子列表中摘除并清空父指针（不释放内存）。`add_root` / `insert_root` / `remove_root`
管理 UI 根列表；`insert_root` 与 `insert_child` 语义一致，把已脱离树的组件插入到根列表第
`index` 个位置，与 `add_root` 共享同一套重复 `id` 检查。
`destroy` 递归释放组件及其子树；只应对已从树中摘除的组件调用。

- 返回值：`create` 返回新组件或 `NULL`（`id` 空或分配失败）；`add_child` / `insert_child` /
  `remove` / `add_root` / `insert_root` / `remove_root` 返回 0 成功，-1 参数非法、未找到、child
  仍挂载或形成环。
- `clone` 深拷贝 `source` 整棵子树并返回新的孤立组件：`id`、文本、颜色、几何、数值属性以及所有
  子节点全部复制，资源型组件（`image`/`video`/`nine_patch`）按 `src` 重新加载独立的纹理/视频，
  不与源共享。克隆体未挂载，调用方负责分配唯一 `id` 后挂载，用完必须 `destroy`。
- `add_root` 与加载器共享同一套重复 `id` 检查：与现有根冲突时返回 -1。
- 修改布局容器（`row`/`column`/`grid`）的子组件后调用 `bapi_ui_layout` 重排位置。

```c
bapi_ui_component_t bapi_ui_component_get_parent(bapi_ui_component_t component);
int					bapi_ui_component_get_child_count(bapi_ui_component_t component);
bapi_ui_component_t bapi_ui_component_get_child(bapi_ui_component_t component, int index);
int					bapi_ui_get_root_count(bapi_ui_t ui);
bapi_ui_component_t bapi_ui_get_root(bapi_ui_t ui, int index);
const char		   *bapi_ui_component_get_id(bapi_ui_component_t component);
bapi_ui_t			bapi_ui_create(void);
```
树遍历。`get_parent` / `get_child` / `get_root` 在 `component`（或 `ui`）为 `NULL` 或越界时返回
`NULL`；`get_child_count` 和 `get_root_count` 在入参为 `NULL` 时返回 0。`get_id` 返回组件 `id`
的借用指针，组件无 `id` 时为 `NULL`。`create` 分配一个空 UI 对象（根列表为空），供以编程方式
从零搭建组件树后保存；与 `bapi_ui_load_from_xml` 返回的 UI 一样，用完必须 `bapi_ui_destroy`。

### UI 组件属性编辑

```c
void bapi_ui_component_set_rect(bapi_ui_component_t component, bapi_rect_t rect);
int	 bapi_ui_component_set_id(bapi_ui_component_t component, const char *id);
int	 bapi_ui_component_set_color(bapi_ui_component_t component, bapi_ui_color_role_t role,
							   bapi_color_t color);
int	 bapi_ui_component_get_color(bapi_ui_component_t component, bapi_ui_color_role_t role,
							   bapi_color_t *out);
void bapi_ui_component_set_text_size(bapi_ui_component_t component, float size);
float bapi_ui_component_get_text_size(bapi_ui_component_t component);
int	 bapi_ui_component_set_src(bapi_ui_component_t component, const char *src);
const char *bapi_ui_component_get_src(bapi_ui_component_t component);
```

`set_rect` 同时更新组件的 `local_rect`（持久化坐标）和 `rect`（当前绘制坐标）。`set_id` 校验非空，
返回 0 成功、-1 失败；不检查全局唯一性（由保存后的重新加载或 `add_root` 检查）。颜色角色
`bapi_ui_color_role_t`：`BAPI_UI_COLOR_NORMAL`（`color`/`normal`）、`BAPI_UI_COLOR_HOVER`
（`color2`/`hover`）、`BAPI_UI_COLOR_CLICK`（`color3`/`click`）、`BAPI_UI_COLOR_TEXT`
（`text_color`）。`set_color` / `get_color` 对非法角色或 `NULL` 参数返回 -1。`set_text_size`
对 `NULL` no-op，`get_text_size` 对 `NULL` 返回 0。

`set_src` 动态替换资源型组件（`image` / `video` / `nine_patch` / `animation`）的资源：先加载新
资源，成功后才释放旧资源并更新内部 `src`，因此失败时组件保持原资源不变；非资源型组件或加载失败
返回 -1，成功返回 0。`get_src` 返回保存时的原始 `src` 字符串（可能是相对路径）的借用指针，
组件无 `src` 时为 `NULL`。

数值型组件属性（`slider` / `progress` 的范围与步进，及网格、圆形、多边形几何参数）：

```c
float bapi_ui_component_get_min_value(bapi_ui_component_t component);
int	 bapi_ui_component_set_min_value(bapi_ui_component_t component, float value);
float bapi_ui_component_get_max_value(bapi_ui_component_t component);
int	 bapi_ui_component_set_max_value(bapi_ui_component_t component, float value);
float bapi_ui_component_get_step(bapi_ui_component_t component);
int	 bapi_ui_component_set_step(bapi_ui_component_t component, float step);
int	 bapi_ui_component_get_columns(bapi_ui_component_t component);
int	 bapi_ui_component_set_columns(bapi_ui_component_t component, int columns);
float bapi_ui_component_get_radius(bapi_ui_component_t component);
int	 bapi_ui_component_set_radius(bapi_ui_component_t component, float radius);
int	 bapi_ui_component_get_sides(bapi_ui_component_t component);
int	 bapi_ui_component_set_sides(bapi_ui_component_t component, int sides);
int	 bapi_ui_component_is_checked(bapi_ui_component_t component);
int	 bapi_ui_component_set_checked(bapi_ui_component_t component, int checked);
int	 bapi_ui_component_get_relative(bapi_ui_component_t component);
int	 bapi_ui_component_set_relative(bapi_ui_component_t component, int relative);
```

- `set_min_value` / `set_max_value` 调整范围后会把当前 `value` 重新 clamp 到新范围内（越界则就地
  修正到边界），返回 0 成功、-1 参数非法。
- `set_columns` 拒绝负数，`set_radius` 拒绝负数，`set_sides` 拒绝负数（0 会被接受，渲染时按
  `sides < 3` 不绘制）；`set_checked` / `set_relative` 按「非零即真」存储。全部 setter 对
  `NULL` 返回 -1，getter 对 `NULL` 返回 0。
- 默认值：`min_value` 0、`max_value` 1、`step` 1（与 XML 加载器的默认一致）；`checked`、
  `columns`、`sides`、`relative` 默认 0。
- 这些属性与 XML 双向持久化一致：`slider` / `progress` 的 `step`（非 1 时）随 `min` / `max` /
  `value` 一起写出；`columns`、`radius`、`sides`、`checked`、`relative` 按非默认值写出。

## 数学、日志与版本

### 向量

```c
bapi_vec2_t bapi_vec2(float x, float y);
bapi_vec2_t bapi_vec2_add(bapi_vec2_t a, bapi_vec2_t b);
bapi_vec2_t bapi_vec2_sub(bapi_vec2_t a, bapi_vec2_t b);
bapi_vec2_t bapi_vec2_scale(bapi_vec2_t vector, float scalar);
bapi_vec2_t bapi_vec2_negate(bapi_vec2_t vector);
```
构造与基本运算。`bapi_vec2_t` 为 `{x,y}`。

```c
float bapi_vec2_dot(bapi_vec2_t a, bapi_vec2_t b);
float bapi_vec2_length(bapi_vec2_t vector);
float bapi_vec2_length_sq(bapi_vec2_t vector);
float bapi_vec2_distance(bapi_vec2_t a, bapi_vec2_t b);
```
点积、长度、长度平方（免开方比较）、两点距离。

```c
bapi_vec2_t bapi_vec2_normalize(bapi_vec2_t vector);
```
单位化。长度小于 `0.00001` 时返回零向量（不做除零）。

```c
bapi_vec2_t bapi_vec2_lerp(bapi_vec2_t a, bapi_vec2_t b, float t);
bapi_vec2_t bapi_vec2_rotate(bapi_vec2_t vector, float angle);
int bapi_vec2_equals(bapi_vec2_t a, bapi_vec2_t b);
```
线性插值、绕原点旋转（`angle` 弧度）、近似相等（容差 `0.00001`）。

### 几何

```c
bapi_circle_t bapi_circle(float x, float y, float radius);
```
构造圆，`bapi_circle_t` 为 `{x,y,r}`。

```c
int bapi_rect_contains_point(bapi_rect_t rect, bapi_vec2_t point);
int bapi_circle_contains_point(bapi_circle_t circle, bapi_vec2_t point);
```
点在矩形/圆内（边界算包含，`<=` / `>=` 比较）。

```c
int bapi_rect_intersects(bapi_rect_t a, bapi_rect_t b);
int bapi_circle_intersects_circle(bapi_circle_t a, bapi_circle_t b);
int bapi_circle_intersects_rect(bapi_circle_t circle, bapi_rect_t rect);
```
矩形相交要求**正面积重叠**（边/角恰好接触算不相交）。圆的相交把相切算作相交。几何查询要求
有限坐标、非负半径、非负矩形宽高，违反时结果不作承诺。

```c
bapi_rect_t bapi_rect_intersection(bapi_rect_t a, bapi_rect_t b);
bapi_rect_t bapi_rect_union(bapi_rect_t a, bapi_rect_t b);
bapi_vec2_t bapi_rect_center(bapi_rect_t rect);
```
相交矩形（无重叠时宽高钳制为 0）、包围矩形、矩形中心。

```c
int bapi_collision_aabb(bapi_rect_t a, bapi_rect_t b);
```
`bapi_rect_intersects` 的别名，语义完全一致。

### 标量工具

```c
float bapi_clamp(float value, float min, float max);
float bapi_lerp(float a, float b, float t);
float bapi_deg_to_rad(float degrees);
float bapi_rad_to_deg(float radians);
```
钳制、线性插值（`a + (b-a)*t`）、角度换算。`bapi_lerp` 不钳制 `t`。

### 错误上报

```c
const char *bapi_get_last_error(void);
void        bapi_clear_last_error(void);
```
返回最近一次引擎失败的错误消息（空串表示没有错误）。消息存储在引擎内部静态缓冲，下一次失败
或 `bapi_clear_last_error()` 时被覆盖。`bapi_engine_init()` 成功会清除旧错误。

### 日志

```c
bool bapi_log_init(const bapi_log_config_t *config);
void bapi_log_shutdown(void);
```
初始化日志。`config` 为 `NULL` 时用默认值（级别 INFO、着色开、不写文件）。`config` 含 `min_level`、
`use_colors`、`use_file`、`log_file_path`。重复调用返回 `true`（幂等）。`bapi_log_shutdown` 关闭
日志文件与互斥锁。`bapi_engine_quit()` 会自动调用。

```c
void bapi_log_set_level(bapi_log_level_t level);
```
调整最低输出级别，低于该级别的消息被丢弃。

```c
void bapi_log_message(bapi_log_level_t level, const char *file, int line, const char *func,
                      const char *format, ...);
```
底层记录函数，一般用下面的宏代替。日志级别见 `bapi_log_level_t`：
`DEBUG` < `INFO` < `WARN` < `ERROR` < `CRITICAL`（`NONE` 用于 `min_level` 屏蔽全部）。

```c
BAPI_LOG_DEBUG(fmt, ...);
BAPI_LOG_INFO(fmt, ...);
BAPI_LOG_WARN(fmt, ...);
BAPI_LOG_ERROR(fmt, ...);
BAPI_LOG_CRITICAL(fmt, ...);
BAPI_LOG_ASSERT(condition, fmt, ...);
BAPI_LOG_INIT_DEFAULT();
```
带 `__FILE__`/`__LINE__`/`__func__` 的便捷宏。仅在编译时定义 `BAPI_LOG_ENABLED` 时展开为实际
输出，否则为 no-op（`BAPI_LOG_ASSERT` 求值条件但不输出）。`BAPI_LOG_INIT_DEFAULT()` 用默认配置
初始化日志。

### 文件 I/O

跨平台只读文件访问，走平台 `io` 能力组。可在 XJ380 等无标准 C 文件 IO 的环境使用。

```c
bapi_file_t bapi_file_open(const char *path);
```
以只读方式打开文件，返回句柄。失败或 `path` 为 `NULL` 返回 `NULL`。

```c
size_t bapi_file_read(bapi_file_t file, void *buffer, size_t size);
```
从当前位置读取最多 `size` 字节，返回实际读取字节数；参数非法返回 0。

```c
int64_t bapi_file_seek(bapi_file_t file, int64_t offset, int origin);
int64_t bapi_file_tell(bapi_file_t file);
int64_t bapi_file_size(bapi_file_t file);
```
`seek` 的 `origin` 与标准 `fseek` 一致（`SEEK_SET`/`SEEK_CUR`/`SEEK_END`）。失败或参数非法返回
`-1`。`tell` 返回当前偏移，`size` 返回文件字节数。

```c
void bapi_file_close(bapi_file_t file);
```
关闭并释放文件句柄。`file` 为 `NULL` 时 no-op。关闭后不得再使用该句柄。

```c
uint8_t *bapi_file_read_alloc(const char *path, size_t *out_size);
```
把整个文件读入 `malloc` 的缓冲区（调用方 `free`）。失败（文件缺失、分配失败、`path` 为 `NULL`）
返回 `NULL`，且 `*out_size` 不被写入；空文件返回非 `NULL` 的空缓冲区且 `*out_size == 0`。
`out_size` 可为 `NULL`。

### 资源包（RZip Pack）

只读读取 RZip（`.rz`）资源包：枚举条目、按名字查询、整文件读取。仅桌面端实现；XJ380 上所有
函数返回失败值并记录一次 warning。本组 API 为单线程使用：一个包对应一个句柄，句柄不可复制；
`bapi_pack_file_name` 返回的名字指针归归档所有，`bapi_pack_close` 后失效。

```c
bapi_pack_t bapi_pack_open(const char *path);
```
打开资源包并解析头部与文件索引。失败（文件缺失、格式非法、截断）或 `path` 为 `NULL` 返回
`NULL`。

```c
void bapi_pack_close(bapi_pack_t pack);
```
关闭归档并释放句柄。`pack` 为 `NULL` 时 no-op。关闭后不得再使用该句柄及其名字指针。

```c
int bapi_pack_file_count(bapi_pack_t pack);
const char *bapi_pack_file_name(bapi_pack_t pack, int index);
```
条目数量与按索引取名字（索引 0 起，顺序与归档内顺序一致）。句柄非法返回 `-1`；
`index` 越界返回 `NULL`。条目名按字节比较（UTF-8 不做归一化）。

```c
int bapi_pack_find_file(bapi_pack_t pack, const char *name);
```
按名字查找条目，返回索引；未找到或参数非法返回 `-1`。名字重复时返回第一个匹配。

```c
int64_t bapi_pack_file_size(bapi_pack_t pack, const char *name);
```
条目的逻辑（解压后）大小。当前 RZip 版本未实现压缩（`comp_size == dcom_size`），两者相等。
未找到或参数非法返回 `-1`。

```c
size_t bapi_pack_read_file(bapi_pack_t pack, const char *name, void *buffer, size_t buffer_size);
uint8_t *bapi_pack_read_file_alloc(bapi_pack_t pack, const char *name, size_t *out_size);
```
整文件读取。`read_file` 把条目内容拷入调用方缓冲区，返回实际拷贝字节数；缓冲区小于文件时
截断（不视为错误）；参数非法或读取失败返回 0。空文件返回 0（与"未找到"不可区分，先
`find_file` 判断存在性）。`read_file_alloc` 返回 `malloc` 的缓冲区（调用方 `free`），失败返回
`NULL`；空文件返回非 `NULL` 的空缓冲区且 `*out_size == 0`。`out_size` 可为 `NULL`，仅在成功时
写入。

```c
bapi_pack_stream_t bapi_pack_stream_open(bapi_pack_t pack, const char *name);
size_t bapi_pack_stream_read(bapi_pack_stream_t stream, void *buffer, size_t size);
int64_t bapi_pack_stream_seek(bapi_pack_stream_t stream, int64_t offset, int whence);
int64_t bapi_pack_stream_tell(bapi_pack_stream_t stream);
int64_t bapi_pack_stream_size(bapi_pack_stream_t stream);
void bapi_pack_stream_close(bapi_pack_stream_t stream);
```
流式读取单个条目：`read` 从当前偏移读取最多 `size` 字节（到达条目末尾返回 0）；`seek` 的
`whence` 与 `fseek` 一致（`SEEK_SET`/`SEEK_CUR`/`SEEK_END`），位置钳制在 `[0, size]`，返回新偏移；
`tell` 返回当前偏移；`size` 返回条目大小。参数非法返回 0 / `-1`。流借用包句柄：**包必须在流关闭
前保持打开**。`open` 在条目不存在或参数非法时返回 `NULL`；`close` 释放流，`NULL` 时 no-op。

```c
bapi_sound_t bapi_sound_load_from_pack(bapi_pack_t pack, const char *name);
bapi_texture_t bapi_texture_load_from_pack(bapi_pack_t pack, const char *name);
bapi_video_t bapi_video_load_from_pack(bapi_pack_t pack, const char *name);
```
全量缓冲套的包内加载：把条目整体读入内存缓冲后交给对应的 `*_load_from_memory` 解码，缓冲在解码
后立即释放，解码资源的归属见对应内存加载接口。条目不存在、平台不支持资源包或内存解码时返回
`NULL`。

### 版本

```c
const char *bridgeengine_get_version(void);
int bridgeengine_get_version_number(void);
```
`get_version` 返回形如 `"2.0.0"` 的版本字符串。`get_version_number` 返回
`MAJOR*10000 + MINOR*100 + PATCH` 的整数（`2.0.0` → `20000`）。
