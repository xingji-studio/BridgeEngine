# BridgeEngine API 文档

## 简介

BridgeEngine 是一个基于 SDL3 的跨平台图形引擎，提供了简洁的 API 接口.

## 导入头文件

```c
#include <BridgeEngine.h>
```

## 核心类型

### 句柄类型（不透明指针）

```c
typedef struct bapi_window_internal* bapi_window_t;      // 窗口句柄
typedef struct bapi_renderer_internal* bapi_renderer_t;  // 渲染器句柄
typedef struct bapi_texture_internal* bapi_texture_t;    // 纹理句柄
typedef struct bapi_event_internal bapi_event_t;         // 事件结构体
```

### 颜色类型

```c
typedef struct {
    uint8_t r;  // 红色分量 (0-255)
    uint8_t g;  // 绿色分量 (0-255)
    uint8_t b;  // 蓝色分量 (0-255)
    uint8_t a;  // 透明度分量 (0-255)
} bapi_color_t;
```

### 矩形类型

```c
typedef struct {
    float x;  // 左上角 x 坐标
    float y;  // 左上角 y 坐标
    float w;  // 宽度
    float h;  // 高度
} bapi_rect_t;
```

### 颜色辅助函数

```c
bapi_color_t bapi_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
bapi_color_t bapi_color_from_hex(uint32_t hex_color);
```

- `bapi_color()`: 使用四个分量直接创建颜色
- `bapi_color_from_hex()`: 使用十六进制颜色值创建颜色（如 0xRRGGBBAA）

## 引擎初始化与清理

### 初始化引擎

```c
int bapi_engine_init(const char* title, int width, int height);
```

- **功能**: 初始化引擎，创建窗口
- **参数**:
  - `title`: 窗口标题
  - `width`: 窗口宽度
  - `height`: 窗口高度
- **返回值**: 成功返回 0，失败返回非 0

### 退出引擎

```c
void bapi_engine_quit(void);
```

- **功能**: 释放引擎所有资源，包括窗口和渲染器

### 创建渲染器

```c
void bapi_engine_render_create(void);
```

- **功能**: 创建渲染器，准备图形绘制环境

## 渲染控制

### 清屏

```c
void bapi_render_clear(void);
```

- **功能**: 使用黑色清空屏幕

### 呈现画面

```c
void bapi_render_present(void);
```

- **功能**: 将绘制的内容呈现到屏幕

### 设置绘制颜色

```c
void bapi_set_render_color(bapi_color_t color);
```

- **功能**: 设置后续绘制操作使用的颜色

### 延时

```c
void bapi_delay(uint32_t ms);
```

- **功能**: 暂停程序执行指定毫秒数

## 基本图形绘制

### 绘制像素

```c
void bapi_draw_pixel(float x, float y, bapi_color_t color);
```

- **功能**: 在指定位置绘制单个像素
- **参数**:
  - `x`, `y`: 像素坐标
  - `color`: 颜色

### 绘制线条

```c
void bapi_draw_line(float x1, float y1, float x2, float y2, bapi_color_t color);
```

- **功能**: 在两点之间绘制线条
- **参数**:
  - `x1`, `y1`: 起点坐标
  - `x2`, `y2`: 终点坐标
  - `color`: 颜色

### 绘制矩形边框

```c
void bapi_draw_rect(float x, float y, float w, float h, bapi_color_t color);
```

- **功能**: 绘制矩形边框
- **参数**:
  - `x`, `y`: 左上角坐标
  - `w`, `h`: 宽和高
  - `color`: 颜色

### 填充矩形

```c
void bapi_fill_rect(float x, float y, float w, float h, bapi_color_t color);
```

- **功能**: 绘制填充矩形
- **参数**:
  - `x`, `y`: 左上角坐标
  - `w`, `h`: 宽和高
  - `color`: 颜色

### 绘制三角形

```c
void bapi_draw_triangle(float x1, float y1, float x2, float y2, float x3, float y3, bapi_color_t color);
```

- **功能**: 绘制三角形轮廓
- **参数**:
  - `x1`, `y1`, `x2`, `y2`, `x3`, `y3`: 三个顶点坐标
  - `color`: 颜色

### 绘制圆形边框

```c
void bapi_draw_circle(float cx, float cy, float radius, bapi_color_t color);
```

- **功能**: 绘制圆形边框
- **参数**:
  - `cx`, `cy`: 圆心坐标
  - `radius`: 半径
  - `color`: 颜色

### 填充圆形

```c
void bapi_fill_circle(float cx, float cy, float radius, bapi_color_t color);
```

- **功能**: 绘制填充圆形
- **参数**:
  - `cx`, `cy`: 圆心坐标
  - `radius`: 半径
  - `color`: 颜色

### 绘制正多边形边框

```c
void bapi_draw_polygon(float cx, float cy, float radius, int sides, bapi_color_t color);
```

- **功能**: 绘制正多边形边框
- **参数**:
  - `cx`, `cy`: 中心坐标
  - `radius`: 外接圆半径
  - `sides`: 边数（最小为3）
  - `color`: 颜色

### 填充正多边形

```c
void bapi_fill_polygon(float cx, float cy, float radius, int sides, bapi_color_t color);
```

- **功能**: 绘制填充正多边形
- **参数**:
  - `cx`, `cy`: 中心坐标
  - `radius`: 外接圆半径
  - `sides`: 边数（最小为3，小于3时不绘制）
  - `color`: 颜色

### 兼容旧 API

以下函数保持向后兼容，支持十六进制颜色参数：

```c
void bapi_engine_render_drawpixel(float x, float y, int color);
void bapi_engine_render_fillrect(float ax, float ay, float width, float height, int color);
void bapi_engine_render_draw_triangle(float x1, float y1, float x2, float y2, float x3, float y3, int color);
```

## 图像处理

### 加载图像

```c
bapi_texture_t bapi_load_image(const char* filepath);
```

- **功能**: 从文件加载图像
- **参数**: `filepath`: 图像文件路径（支持 PNG、JPEG 等格式）

## 按钮系统

### 按钮类型

```c
typedef struct {
    bapi_rect_t rect;            // 按钮矩形区域
    bapi_color_t normal_color;   // 正常状态颜色
    bapi_color_t hover_color;    // 悬停状态颜色
    bapi_color_t click_color;    // 点击状态颜色
    const char* text;            // 按钮文本
    bapi_color_t text_color;     // 文本颜色
    float text_size;             // 文本大小
    int is_clicked;              // 是否被点击
    int is_hovered;              // 是否被悬停
} bapi_button_t;
```

### 创建按钮

```c
bapi_button_t* bapi_create_button(float x, float y, float w, float h, const char* text, bapi_color_t normal_color, bapi_color_t hover_color, bapi_color_t click_color, bapi_color_t text_color, float text_size);
```

- **功能**: 创建一个新的按钮
- **参数**:
  - `x`, `y`: 按钮左上角坐标
  - `w`, `h`: 按钮宽度和高度
  - `text`: 按钮上显示的文本
  - `normal_color`: 按钮正常状态下的颜色
  - `hover_color`: 鼠标悬停在按钮上时的颜色
  - `click_color`: 按钮被点击时的颜色
  - `text_color`: 按钮文本的颜色
  - `text_size`: 按钮文本的大小
- **返回值**: 成功返回按钮指针，失败返回 NULL

### 销毁按钮

```c
void bapi_destroy_button(bapi_button_t* button);
```

- **功能**: 销毁按钮并释放资源
- **参数**: `button`: 要销毁的按钮指针

### 更新按钮状态

```c
int bapi_button_update(bapi_button_t* button, const bapi_event_t* event);
```

- **功能**: 更新按钮状态，处理鼠标事件
- **参数**:
  - `button`: 要更新的按钮指针
  - `event`: 事件指针
- **返回值**: 当按钮被点击并释放时返回 1，否则返回 0

### 渲染按钮

```c
void bapi_button_render(bapi_button_t* button);
```

- **功能**: 渲染按钮到屏幕上
- **参数**: `button`: 要渲染的按钮指针

### 检查按钮是否被点击

```c
int bapi_button_is_clicked(bapi_button_t* button);
```

- **功能**: 检查按钮当前是否处于点击状态
- **参数**: `button`: 要检查的按钮指针
- **返回值**: 按钮被点击返回 1，否则返回 0

### 检查按钮是否被悬停

```c
int bapi_button_is_hovered(bapi_button_t* button);
```

- **功能**: 检查鼠标是否悬停在按钮上
- **参数**: `button`: 要检查的按钮指针
- **返回值**: 鼠标悬停返回 1，否则返回 0
- **返回值**: 成功返回纹理句柄，失败返回 NULL

### 绘制图像

```c
void bapi_draw_image(bapi_texture_t texture, float x, float y, float w, float h);
```

- **功能**: 在指定位置绘制图像
- **参数**:
  - `texture`: 纹理句柄
  - `x`, `y`: 绘制位置
  - `w`, `h`: 绘制尺寸

### 销毁图像

```c
void bapi_destroy_texture(bapi_texture_t texture);
```

- **功能**: 释放图像资源
- **参数**: `texture`: 要销毁的纹理句柄

### 兼容旧 API

```c
SDL_Texture* bapi_engine_render_load_image(const char* filepath);
void bapi_engine_render_draw_image(SDL_Texture* texture, float x, float y, float width, float height);
void bapi_engine_render_destroy_image(SDL_Texture* texture);
```

## 事件处理

### 轮询事件

```c
int bapi_poll_event(bapi_event_t* event);
```

- **功能**: 从事件队列中获取一个事件
- **参数**: `event`: 事件结构体指针，如果为 NULL 则仅检查是否有事件
- **返回值**: 有事件返回 1，无事件返回 0

### 获取事件类型

```c
int bapi_event_get_type(const bapi_event_t* event);
```

- **返回值**: 事件类型（SDL_EVENT_QUIT、SDL_EVENT_KEY_DOWN 等）

### 获取按键代码

```c
int bapi_event_get_key_code(const bapi_event_t* event);
```

- **返回值**: 按键代码

### 获取鼠标位置

```c
int bapi_event_get_mouse_x(const bapi_event_t* event);
int bapi_event_get_mouse_y(const bapi_event_t* event);
```

- **返回值**: 鼠标坐标

### 获取鼠标按钮

```c
int bapi_event_get_mouse_button(const bapi_event_t* event);
```

- **返回值**: 鼠标按钮代码（SDL_BUTTON_LEFT 等）

## 文字处理

### 文字系统初始化

```c
int bapi_text_init(const char* font_path, int font_size);
```

- **功能**: 初始化文字系统，加载字体文件
- **参数**:
  - `font_path`: 字体文件路径
  - `font_size`: 字体大小
- **返回值**: 成功返回 0，失败返回 -1

### 渲染文字

```c
bapi_texture_t bapi_render_text(const char* text, bapi_color_t color);
```

- **功能**: 将文字渲染为纹理
- **参数**:
  - `text`: 要渲染的文字内容（支持 UTF-8 编码，中英文均可）
  - `color`: 文字颜色
- **返回值**: 成功返回纹理句柄，失败返回 NULL

### 绘制文字

```c
void bapi_draw_text(bapi_texture_t text_texture, float x, float y, float w, float h);
```

- **功能**: 在指定位置绘制文字纹理
- **参数**:
  - `text_texture`: 文字纹理句柄
  - `x`, `y`: 绘制位置
  - `w`, `h`: 绘制尺寸

### 销毁文字纹理

```c
void bapi_destroy_text(bapi_texture_t text_texture);
```

- **功能**: 释放文字纹理资源
- **参数**: `text_texture`: 要销毁的文字纹理句柄

### 清理文字系统

```c
void bapi_text_cleanup(void);
```

- **功能**: 清理文字系统资源

### 兼容旧 API

```c
SDL_Texture* bapi_text_render(const char* text, int color);
void bapi_text_draw(SDL_Texture* text_texture, float x, float y, float width, float height);
void bapi_text_destroy(SDL_Texture* text_texture);
```

## 鼠标绘图

### 鼠标绘图初始化

```c
void bapi_mouse_init(void);
```

- **功能**: 初始化鼠标绘图功能

### 处理鼠标事件

```c
void bapi_mouse_handle_event(const bapi_event_t* event);
```

- **功能**: 处理鼠标绘图相关事件（支持左键拖拽绘图）
- **参数**: `event`: 事件结构体指针

### 渲染鼠标绘图

```c
void bapi_mouse_render(void);
```

- **功能**: 渲染鼠标绘制的线条

### 绘制线条

```c
void bapi_mouse_draw_line(float x1, float y1, float x2, float y2, bapi_color_t color);
```

- **功能**: 绘制线条
- **参数**:
  - `x1`, `y1`: 起点坐标
  - `x2`, `y2`: 终点坐标
  - `color`: 颜色

### 清除线条

```c
void bapi_mouse_clear(void);
```

- **功能**: 清除所有绘制的线条

### 清理鼠标绘图

```c
void bapi_mouse_cleanup(void);
```

- **功能**: 清理鼠标绘图资源

### 兼容旧 API

```c
void bapi_mouse_drawing_init(void);
void bapi_mouse_drawing_handle_event(SDL_Event *event);
void bapi_mouse_drawing_render(void);
void bapi_mouse_drawing_cleanup(void);
void bapi_mouse_drawing_draw_line(float x1, float y1, float x2, float y2, int color);
void bapi_mouse_drawing_clear_lines(void);
```

## 音频系统

音频系统支持加载和播放WAV格式的音效文件，包括音量控制和循环播放功能。

### 音频类型

```c
typedef struct bapi_sound_internal* bapi_sound_t;
```

### 音频初始化与清理

```c
int bapi_audio_init(void);
void bapi_audio_cleanup(void);
```

- **bapi_audio_init()**: 初始化音频子系统，返回0表示成功
- **bapi_audio_cleanup()**: 清理音频子系统，释放所有音频资源

### 音效加载与播放

```c
bapi_sound_t bapi_sound_load(const char* filepath);
int bapi_sound_play(bapi_sound_t sound);
void bapi_sound_stop(bapi_sound_t sound);
void bapi_sound_free(bapi_sound_t sound);
```

- **bapi_sound_load()**: 加载WAV格式的音效文件，返回音效句柄，失败返回NULL
- **bapi_sound_play()**: 播放已加载的音效，返回0表示成功
- **bapi_sound_stop()**: 停止播放音效
- **bapi_sound_free()**: 释放音效资源

### 音效控制

```c
void bapi_sound_set_volume(bapi_sound_t sound, float volume);
void bapi_sound_set_loop(bapi_sound_t sound, int loop);
```

- **bapi_sound_set_volume()**: 设置音效音量（范围：0.0-1.0）
- **bapi_sound_set_loop()**: 设置音效是否循环播放（1=循环，0=单次）

### 音频更新

```c
void bapi_sound_update(void);
```

- **bapi_sound_update()**: 在主循环中调用，用于处理循环播放等音频更新操作

## 视频系统

视频系统支持加载和播放MP4等格式的视频文件，基于FFmpeg实现完整的视频解码和渲染功能。

### 视频类型

```c
typedef struct bapi_video_internal* bapi_video_t;
```

### 视频初始化与清理

```c
int bapi_video_init(void);
void bapi_video_cleanup(void);
```

- **bapi_video_init()**: 初始化视频子系统，返回0表示成功
- **bapi_video_cleanup()**: 清理视频子系统，释放所有视频资源

### 视频加载与释放

```c
bapi_video_t bapi_video_load(const char* filepath);
void bapi_video_free(bapi_video_t video);
```

- **bapi_video_load()**: 加载视频文件（支持MP4、AVI、MKV等格式），返回视频句柄，失败返回NULL
- **bapi_video_free()**: 释放视频资源

### 视频播放控制

```c
int bapi_video_play(bapi_video_t video);
void bapi_video_pause(bapi_video_t video);
void bapi_video_stop(bapi_video_t video);
```

- **bapi_video_play()**: 开始播放视频，返回0表示成功
- **bapi_video_pause()**: 暂停/恢复视频播放（切换状态）
- **bapi_video_stop()**: 停止视频播放并重置到开头

### 视频渲染

```c
void bapi_video_render(bapi_video_t video, int x, int y, int w, int h);
void bapi_video_render_fit(bapi_video_t video, int area_x, int area_y, int area_w, int area_h);
void bapi_video_render_center(bapi_video_t video, int window_w, int window_h);
```

- **bapi_video_render()**: 在指定位置渲染视频（拉伸模式，不保持宽高比）
  - `x`, `y`: 渲染位置左上角坐标
  - `w`, `h`: 渲染尺寸

- **bapi_video_render_fit()**: 在指定区域内自适应渲染（保持宽高比，居中显示）
  - `area_x`, `area_y`: 区域左上角坐标
  - `area_w`, `area_h`: 区域尺寸
  - 视频会自动缩放以适应区域，同时保持原始宽高比

- **bapi_video_render_center()**: 在整个窗口内居中渲染（保持宽高比）
  - `window_w`, `window_h`: 窗口尺寸
  - 视频会自动缩放以适应窗口，同时保持原始宽高比

### 视频属性设置

```c
void bapi_video_set_loop(bapi_video_t video, int loop);
void bapi_video_set_volume(bapi_video_t video, float volume);
```

- **bapi_video_set_loop()**: 设置视频是否循环播放（1=循环，0=单次）
- **bapi_video_set_volume()**: 设置视频音量（范围：0.0-1.0）

### 视频状态查询

```c
int bapi_video_is_playing(bapi_video_t video);
void bapi_video_get_size(bapi_video_t video, int* w, int* h);
```

- **bapi_video_is_playing()**: 检查视频是否正在播放，返回1表示播放中，0表示未播放
- **bapi_video_get_size()**: 获取视频原始尺寸

### 视频更新

```c
void bapi_video_update(void);
```

- **bapi_video_update()**: 在主循环中调用，用于更新视频帧

### 视频使用示例

```c
bapi_video_t video = bapi_video_load("video.mp4");
if (video != NULL) {
    bapi_video_set_loop(video, 1);
    bapi_video_play(video);
}

while (running) {
    bapi_video_update();
    
    bapi_render_clear();
    
    bapi_video_render_fit(video, 0, 0, 800, 600);
    
    bapi_render_present();
}

bapi_video_stop(video);
bapi_video_free(video);
```

## 场景管理

场景管理系统允许您创建和管理多个游戏场景，支持场景切换和生命周期回调。

### 场景类型

```c
typedef struct bapi_scene_internal* bapi_scene_t;
typedef struct bapi_scene_manager_internal* bapi_scene_manager_t;
```

### 场景回调类型

```c
typedef void (*bapi_scene_on_enter_fn)(bapi_scene_t scene);
typedef void (*bapi_scene_on_exit_fn)(bapi_scene_t scene);
typedef void (*bapi_scene_on_update_fn)(bapi_scene_t scene, float delta_time);
typedef void (*bapi_scene_on_render_fn)(bapi_scene_t scene);

typedef struct {
    bapi_scene_on_enter_fn on_enter;
    bapi_scene_on_exit_fn on_exit;
    bapi_scene_on_update_fn on_update;
    bapi_scene_on_render_fn on_render;
    void* user_data;
} bapi_scene_callbacks_t;
```

### 创建场景

```c
bapi_scene_t bapi_scene_create(const char* name, bapi_scene_callbacks_t callbacks);
```

- **功能**: 创建一个新场景
- **参数**:
  - `name`: 场景名称
  - `callbacks`: 场景回调函数结构体
- **返回值**: 成功返回场景句柄，失败返回 NULL

### 销毁场景

```c
void bapi_scene_destroy(bapi_scene_t scene);
```

- **功能**: 销毁场景并释放资源
- **参数**: `scene`: 要销毁的场景句柄

### 获取场景名称

```c
const char* bapi_scene_get_name(bapi_scene_t scene);
```

- **功能**: 获取场景名称
- **参数**: `scene`: 场景句柄
- **返回值**: 场景名称字符串

### 场景用户数据

```c
void* bapi_scene_get_user_data(bapi_scene_t scene);
void bapi_scene_set_user_data(bapi_scene_t scene, void* user_data);
```

- **功能**: 获取或设置场景的用户数据

### 创建场景管理器

```c
bapi_scene_manager_t bapi_scene_manager_create(void);
```

- **功能**: 创建场景管理器
- **返回值**: 成功返回场景管理器句柄，失败返回 NULL

### 销毁场景管理器

```c
void bapi_scene_manager_destroy(bapi_scene_manager_t manager);
```

- **功能**: 销毁场景管理器及其所有场景

### 添加场景

```c
int bapi_scene_manager_add_scene(bapi_scene_manager_t manager, bapi_scene_t scene);
```

- **功能**: 将场景添加到管理器
- **返回值**: 成功返回 0，失败返回负值

### 切换场景

```c
int bapi_scene_manager_switch_scene(bapi_scene_manager_t manager, const char* name);
```

- **功能**: 切换到指定名称的场景
- **参数**:
  - `manager`: 场景管理器
  - `name`: 目标场景名称
- **返回值**: 成功返回 0，场景不存在返回 -2

### 获取当前场景

```c
bapi_scene_t bapi_scene_manager_get_current_scene(bapi_scene_manager_t manager);
bapi_scene_t bapi_scene_manager_get_scene(bapi_scene_manager_t manager, const char* name);
```

- **功能**: 获取当前场景或指定名称的场景

### 更新和渲染

```c
void bapi_scene_manager_update(bapi_scene_manager_t manager, float delta_time);
void bapi_scene_manager_render(bapi_scene_manager_t manager);
```

- **功能**: 更新和渲染当前场景

## 关卡管理

关卡管理系统支持关卡的加载、卸载和序列管理。

### 关卡类型

```c
typedef struct bapi_level_internal* bapi_level_t;
typedef struct bapi_level_manager_internal* bapi_level_manager_t;
```

### 关卡回调类型

```c
typedef void (*bapi_level_on_load_fn)(bapi_level_t level);
typedef void (*bapi_level_on_unload_fn)(bapi_level_t level);
typedef void (*bapi_level_on_update_fn)(bapi_level_t level, float delta_time);
typedef void (*bapi_level_on_render_fn)(bapi_level_t level);

typedef struct {
    bapi_level_on_load_fn on_load;
    bapi_level_on_unload_fn on_unload;
    bapi_level_on_update_fn on_update;
    bapi_level_on_render_fn on_render;
    void* user_data;
} bapi_level_callbacks_t;
```

### 创建关卡

```c
bapi_level_t bapi_level_create(const char* name, int index, bapi_level_callbacks_t callbacks);
```

- **功能**: 创建一个新关卡
- **参数**:
  - `name`: 关卡名称
  - `index`: 关卡索引（用于序列管理）
  - `callbacks`: 关卡回调函数结构体
- **返回值**: 成功返回关卡句柄，失败返回 NULL

### 销毁关卡

```c
void bapi_level_destroy(bapi_level_t level);
```

- **功能**: 销毁关卡并释放资源

### 获取关卡信息

```c
const char* bapi_level_get_name(bapi_level_t level);
int bapi_level_get_index(bapi_level_t level);
void* bapi_level_get_user_data(bapi_level_t level);
void bapi_level_set_user_data(bapi_level_t level, void* user_data);
```

- **功能**: 获取关卡名称、索引或用户数据

### 创建关卡管理器

```c
bapi_level_manager_t bapi_level_manager_create(void);
void bapi_level_manager_destroy(bapi_level_manager_t manager);
```

- **功能**: 创建或销毁关卡管理器

### 添加关卡

```c
int bapi_level_manager_add_level(bapi_level_manager_t manager, bapi_level_t level);
```

- **功能**: 将关卡添加到管理器

### 加载关卡

```c
int bapi_level_manager_load_level(bapi_level_manager_t manager, const char* name);
int bapi_level_manager_load_level_by_index(bapi_level_manager_t manager, int index);
```

- **功能**: 按名称或索引加载关卡
- **返回值**: 成功返回 0，关卡不存在返回 -2

### 关卡序列管理

```c
int bapi_level_manager_next_level(bapi_level_manager_t manager);
int bapi_level_manager_previous_level(bapi_level_manager_t manager);
```

- **功能**: 加载下一个或上一个关卡
- **返回值**: 成功返回 0，没有更多关卡返回 -2

### 获取关卡信息

```c
bapi_level_t bapi_level_manager_get_current_level(bapi_level_manager_t manager);
bapi_level_t bapi_level_manager_get_level(bapi_level_manager_t manager, const char* name);
bapi_level_t bapi_level_manager_get_level_by_index(bapi_level_manager_t manager, int index);
int bapi_level_manager_get_level_count(bapi_level_manager_t manager);
```

- **功能**: 获取当前关卡、指定关卡或关卡总数

### 更新和渲染

```c
void bapi_level_manager_update(bapi_level_manager_t manager, float delta_time);
void bapi_level_manager_render(bapi_level_manager_t manager);
```

- **功能**: 更新和渲染当前关卡

## XML 配置文件支持

引擎支持通过 XML 文件加载场景和关卡配置。

### 从 XML 加载

```c
bapi_scene_manager_t bapi_scene_manager_load_from_xml(const char* filepath);
bapi_level_manager_t bapi_level_manager_load_from_xml(const char* filepath);
```

- **功能**: 从 XML 文件加载场景或关卡管理器
- **参数**: `filepath`: XML 文件路径
- **返回值**: 成功返回管理器句柄，失败返回 NULL

### 保存到 XML

```c
int bapi_scene_manager_save_to_xml(bapi_scene_manager_t manager, const char* filepath);
int bapi_level_manager_save_to_xml(bapi_level_manager_t manager, const char* filepath);
```

- **功能**: 将场景或关卡管理器保存到 XML 文件
- **返回值**: 成功返回 0，失败返回 -1

### XML 文件格式

#### 场景配置文件格式

```xml
<?xml version="1.0" encoding="UTF-8"?>
<scenes>
  <scene name="main_menu" />
  <scene name="game_play" />
  <scene name="settings" />
</scenes>
```

#### 关卡配置文件格式

```xml
<?xml version="1.0" encoding="UTF-8"?>
<levels>
  <level name="level_1" index="1" />
  <level name="level_2" index="2" />
  <level name="level_3" index="3" />
</levels>
```

## 完整使用示例

```c
#include <BridgeEngine.h>
#include <stdio.h>
#include <stdbool.h>

int main() {
    // 初始化引擎
    if (bapi_engine_init("示例程序", 800, 600) != 0) {
        printf("引擎初始化失败\n");
        return 1;
    }
    
    // 创建渲染器
    bapi_engine_render_create();
    
    // 初始化文字系统
    bapi_text_init("font.ttf", 24);
    
    // 渲染文字
    bapi_texture_t text = bapi_render_text("Hello, BridgeEngine!", 
                                           bapi_color(255, 255, 255, 255));
    
    // 主循环
    bool running = true;
    bapi_event_t event;
    
    while (running) {
        // 处理事件
        while (bapi_poll_event(&event)) {
            if (bapi_event_get_type(&event) == SDL_EVENT_QUIT) {
                running = false;
            }
        }
        
        // 清屏
        bapi_render_clear();
        
        // 绘制内容
        bapi_fill_rect(100, 100, 200, 100, bapi_color(255, 0, 0, 255));
        
        if (text != NULL) {
            bapi_draw_text(text, 50, 50, 300, 40);
        }
        
        // 呈现
        bapi_render_present();
        bapi_delay(16);
    }
    
    // 清理资源
    if (text != NULL) {
        bapi_destroy_text(text);
    }
    bapi_text_cleanup();
    bapi_engine_quit();
    
    return 0;
}
```

## 版本信息

```c
const char* bridgeengine_get_version(void);
int bridgeengine_get_version_number(void);
```

- **功能**: 获取引擎版本信息