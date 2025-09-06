# BridgeEngine API DOCS
## 初始化与清理
### 导入头文件
```
#include <BridgeEngine.h>
```

### 引擎初始化
```
int bapi_engine_init(char *title, int width, int height);
```
- **功能**: 初始化引擎，创建窗口
- **参数**: 
  - `title`: 窗口标题
  - `width`: 窗口宽度
  - `height`: 窗口高度
- **返回值**: 成功返回0，失败返回非0

### 渲染器创建
```
void bapi_engine_render_create(void);
```
- **功能**: 创建渲染器

## 基本图形绘制

### 绘制像素
```
void bapi_engine_render_drawpixel(float x, float y, int color);
```
- **功能**: 在指定位置绘制像素
- **参数**: 
  - `x`, `y`: 坐标
  - `color`: 颜色值（RGBA格式，如0xRRGGBBAA）

### 填充矩形
```
void bapi_engine_render_fillrect(float ax, float ay, float width, float height, int color);
```
- **功能**: 绘制填充矩形
- **参数**: 
  - `ax`, `ay`: 左上角坐标
  - `width`, `height`: 矩形宽高
  - `color`: 颜色值

### 绘制三角形
```
void bapi_engine_render_draw_triangle(float x1, float y1, float x2, float y2, float x3, float y3, int color);
```
- **功能**: 绘制三角形
- **参数**: 
  - `x1`, `y1`, `x2`, `y2`, `x3`, `y3`: 三个顶点坐标
  - `color`: 颜色值

## 图像处理

### 加载图像
```
SDL_Texture* bapi_engine_render_load_image(const char* filepath);
```
- **功能**: 从文件加载图像
- **参数**: `filepath`: 图像文件路径
- **返回值**: 成功返回纹理指针，失败返回NULL

### 绘制图像
```
void bapi_engine_render_draw_image(SDL_Texture* texture, float x, float y, float width, float height);
```
- **功能**: 在指定位置绘制图像
- **参数**: 
  - `texture`: 纹理指针
  - `x`, `y`: 绘制位置
  - `width`, `height`: 绘制尺寸

### 销毁图像
```
void bapi_engine_render_destroy_image(SDL_Texture* texture);
```
- **功能**: 释放图像资源
- **参数**: `texture`: 要销毁的纹理指针

## 鼠标绘图

### 鼠标绘图初始化
```
void bapi_mouse_drawing_init(void);
```
- **功能**: 初始化鼠标绘图功能

### 处理鼠标事件
```
void bapi_mouse_drawing_handle_event(SDL_Event *event);
```
- **功能**: 处理鼠标绘图相关事件
- **参数**: `event`: SDL事件指针

### 渲染鼠标绘图
```
void bapi_mouse_drawing_render(void);
```
- **功能**: 渲染鼠标绘制的线条

### 清理鼠标绘图
```
void bapi_mouse_drawing_cleanup(void);
```
- **功能**: 清理鼠标绘图资源

### 绘制线条
```
void bapi_mouse_drawing_draw_line(float x1, float y1, float x2, float y2, int color);
```
- **功能**: 绘制线条
- **参数**: 
  - `x1`, `y1`: 起点坐标
  - `x2`, `y2`: 终点坐标
  - `color`: 颜色值


### 清除线条
```
void bapi_mouse_drawing_clear_lines(void);
```
- **功能**: 清除所有绘制的线条

## 文字处理

### 文字系统初始化
```
int bapi_text_init(const char* font_path, int font_size);
```
- **功能**: 初始化文字系统，加载字体文件
- **参数**: 
  - `font_path`: 字体文件路径
  - `font_size`: 字体大小
- **返回值**: 成功返回0，失败返回-1

### 渲染文字
```
SDL_Texture* bapi_text_render(const char* text, int color);
```
- **功能**: 将文字渲染为纹理
- **参数**: 
  - `text`: 要渲染的文字内容
  - `color`: 文字颜色（RGBA格式，如0xRRGGBBAA）
- **返回值**: 成功返回纹理指针，失败返回NULL

### 绘制文字
```
void bapi_text_draw(SDL_Texture* text_texture, float x, float y, float width, float height);
```
- **功能**: 在指定位置绘制文字纹理
- **参数**: 
  - `text_texture`: 文字纹理指针
  - `x`, `y`: 绘制位置
  - `width`, `height`: 绘制尺寸

### 销毁文字纹理
```
void bapi_text_destroy(SDL_Texture* text_texture);
```
- **功能**: 释放文字纹理资源
- **参数**: `text_texture`: 要销毁的文字纹理指针

### 清理文字系统
```
void bapi_text_cleanup(void);
```
- **功能**: 清理文字系统资源