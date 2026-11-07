# BridgeEngine

## 鹊桥引擎

BridgeEngine 是一个跨平台 2D 图形引擎，提供简洁的 C API。桌面端使用 SDL3 + FFmpeg，XJ380 端使用原生 XAPI 后端。

---

## 功能特性

| 模块 | 说明 |
|------|------|
| **渲染** | SDL_Renderer / Vulkan(SDL_GPU) 可选后端，点线矩形圆多边形、实心填充 |
| **后端** | 默认尝试 Vulkan，失败自动回退 SDL_Renderer；UI Kit 固定 SDL |
| **帧循环** | delta、固定步长、时间缩放、暂停、FPS 统计、目标帧率与 vsync |
| **2D 变换** | 变换栈、摄像机接入、sprite 源矩形/旋转/缩放/翻转/tint、图集批处理 |
| **窗口** | 逻辑分辨率、全屏、vsync、窗口尺寸查询与 resize 事件 |
| **文字** | 桌面端基于 SDL3_ttf，支持 TTF 字体 |
| **图像** | 加载和绘制 PNG/JPEG 等格式纹理，按路径缓存 |
| **音频** | 桌面端 WAV 音效加载播放，支持循环、音量、master volume；FFmpeg 流式 BGM |
| **视频** | 桌面端基于 FFmpeg 解码，支持 MP4/AVI/MKV 播放 |
| **场景实体** | transform 层级、父子挂载、sprite 绘制、更新回调 |
| **世界运行时** | 根实体管理、spawn/despawn、暂停、摄像机、统一 update/draw |
| **输入动作** | 命名动作、多键/鼠标绑定、down/pressed/released |
| **Tilemap** | 行优先网格、tileset、solid 标记、AABB 碰撞、分块移动解算 |
| **动画状态机** | clip、帧时长、循环、bool 参数、transition、finished |
| **碰撞** | AABB collider、layer/mask、查询、X/Y 扫掠移动解算 |
| **摄像机控制器** | 死区、平滑跟随、边界夹紧、shake |
| **Tween** | 8 种 easing、manager、loop、finished |
| **粒子** | 固定池、发射率、重力、颜色/透明度插值 |
| **寻路** | tilemap A*，4/8 邻接，solid 阻挡 |
| **定时器** | 帧驱动 one-shot / repeating，回调内可 cancel |
| **场景管理** | 多场景创建/切换，生命周期回调 |
| **关卡管理** | 关卡加载/卸载，序列管理，上下级切换 |
| **XML 配置** | 场景和关卡的 XML 文件加载与保存 |
| **资源包** | RZip (.rz) 资源包读取：枚举条目、按名字查询、整文件读取 |
| **图集** | 构建期 `bridgeengine_atlas_pack` 打包图集页 + 清单，运行时按名字绘制 |
| **资源烘焙** | `bridgeengine_atlas_pack` 打包图集，`bridgeengine_bctex_pack` 生成 BC1/BC3，`bridgeengine_rz_pack` 打包产物 |
| **输入系统** | 键盘状态查询、鼠标位置获取 |
| **摄像机** | 2D 摄像机，支持平移、缩放、世界坐标转换 |
| **数学工具** | 向量运算、插值、钳制等工具函数 |
| **错误上报** | `bapi_get_last_error()` 返回最近一次引擎/资源失败原因 |
| **项目 CLI** | `bcli` 管理 `.bep` 项目、模板、UI 文档、构建/运行和依赖诊断 |
| **UI Kit** | 独立 UI 文档设计工具：多文档、层级、属性、预览、项目模板；不是完整游戏/场景编辑器 |
| **Vertical Slice** | `examples/vertical_slice`：可胜负/重开的 Coin Runner，展示世界、输入、碰撞、寻路、摄像机、动画、粒子和 HUD |

完整API文档请查看[BAPI文档](docs/bapi.md)，项目命令行见[CLI 文档](docs/cli.md)。

```bash
bcli new my_game --dir ~/projects --engine ~/BridgeEngine
cd ~/projects/my_game
bcli build
bcli run

# 引擎自带的 vertical slice
cmake --build build --target bridgeengine_vertical_slice_example
./build/bridgeengine_vertical_slice_example
```

> UI Kit 的定位是 **UI 文档设计工具**。引擎的完整场景/ECS/tilemap/动画状态机仍在推进，
> 因此当前不把它称为“完整游戏编辑器”。

---

## 依赖

- SDL3 + SDL3_image + SDL3_ttf（优先使用已安装依赖；缺失时 CMake 自动下载固定源码版本）
- FFmpeg 开发库（libavcodec、libavformat、libavutil、libswscale、libswresample）
- OpenGL (Linux) 或 opengl32 (Windows)
- CMake >= 3.20
- C11 编译器 (GCC/Clang/MSVC)

### 安装依赖

SDL3、SDL3_image 和 SDL3_ttf 不需要预先安装。CMake 会优先采用 `CMAKE_PREFIX_PATH`、vcpkg
toolchain 或系统包；均不可用时自动通过 `FetchContent` 下载 SDL3 3.4.12、SDL3_image 3.4.4、
SDL3_ttf 3.2.2 到构建目录的 `_deps/`。可用 `-DBRIDGEENGINE_FETCH_SDL=OFF` 禁用自动下载。

FFmpeg 始终是外部开发依赖，不会由 CMake 自动构建：

```bash
# macOS (Homebrew)
brew install sdl3 sdl3_image sdl3_ttf ffmpeg

# Linux (apt)
sudo apt install libsdl3-dev libsdl3-image-dev libsdl3-ttf-dev \
                  libavcodec-dev libavformat-dev libavutil-dev \
                  libswscale-dev libswresample-dev
```

若 Linux 或 macOS 没有可用包管理器，可从官方源码构建到项目本地目录：

```bash
./scripts/bootstrap-ffmpeg.sh
PKG_CONFIG_PATH="$PWD/.bridgeengine-deps/ffmpeg/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}" \
  cmake --preset default
```

脚本固定 FFmpeg 7.1.1 并校验 SHA-256，构建包含 BridgeEngine 所需
`avcodec`、`avformat`、`avutil`、`swscale`、`swresample` 的 FFmpeg。传入第一个参数可指定安装前缀。

Windows 请在 PowerShell 执行：

```powershell
.\scripts\bootstrap-vcpkg.ps1
```

脚本会检查 Visual Studio Build Tools 的 C++ 工作负载，并在指定依赖目录（默认
`.bridgeengine-deps/vcpkg`）中引导 vcpkg，无需管理员权限。它默认构建 `ffmpeg:x64-windows`、
`sdl3`、`sdl3-image[png]` 和 `sdl3-ttf`；可用 `-Triplet` 或 `VCPKG_TARGET_TRIPLET` 环境变量覆盖目标 triplet。
完成后使用脚本输出的 `CMAKE_TOOLCHAIN_FILE` 和 `VCPKG_TARGET_TRIPLET` 参数配置 CMake。

vcpkg 的 `sdl3-image` 默认不编译图像编解码器，需要按需启用 feature（如 `sdl3-image[png]`）才能用
`bapi_texture_load()` 加载 PNG/JPEG；未启用时仅 BMP 可用。

---

## 构建

### CMake

```bash
cmake --preset default
cmake --build --preset default
```

预设使用 Ninja，并将默认构建目录固定为 `build/`。若此前用其他生成器配置过该目录，先删除 `build/CMakeCache.txt` 和 `build/CMakeFiles/` 再重新配置。

`default` 预设会构建 UI Kit，因此首次配置可能从 GitHub 下载 SDL3_image、SDL3_ttf 和 Dear ImGui，网络不畅时会显得很慢。只构建引擎/示例时可以跳过 UI Kit：

```bash
cmake --preset default -DBRIDGEENGINE_BUILD_UIKIT=OFF
```

已有本地 ImGui 源码时用 `-DBRIDGEENGINE_IMGUI_SOURCE_DIR=/path/to/imgui` 避免 clone。系统已安装
SDL3_image/SDL3_ttf 时可加 `-DBRIDGEENGINE_FETCH_SDL=OFF`；只有系统 zlib/libpng/freetype/harfbuzz
时可用 `-DSDLIMAGE_VENDORED=OFF -DSDLTTF_VENDORED=OFF`。复用已经下载过的 SDL 源码：

```bash
cmake --preset default -DBRIDGEENGINE_BUILD_UIKIT=OFF \
  -DFETCHCONTENT_SOURCE_DIR_SDL3_IMAGE="$PWD/build/desktop/_deps/sdl3_image-src" \
  -DFETCHCONTENT_SOURCE_DIR_SDL3_TTF="$PWD/build/desktop/_deps/sdl3_ttf-src" \
  -DSDLIMAGE_VENDORED=OFF -DSDLTTF_VENDORED=OFF
```

配置被中断后不要让半初始化的目录继续用，先删掉对应 build 目录再重配。

本机已把 SDL3_image/SDL3_ttf 和 Dear ImGui 以无 root 方式安装到 `.bridgeengine-deps/`。
CMake 会自动检测该目录，所以默认预设不再需要联网：

```bash
cmake --preset default
cmake --build --preset default
```

只想构建引擎、跳过 UI Kit 时：

```bash
cmake --preset local
cmake --build --preset local
```

`local` 预设使用 `.bridgeengine-deps`、跳过 UI Kit 与网络下载。若该目录不存在，先按上面的
`FETCHCONTENT_SOURCE_DIR_*` 方式配置一次，或安装系统 SDL3_image/SDL3_ttf。

CMake 默认生成 `build/compile_commands.json`，并构建：

- `bridgeengine_shared`
- `bridgeengine_static`
- `bridgeengine_desktop_example`
- `bridgeengine_text_example`
- `bridgeengine_log_example`

请求桌面库或示例时，CMake 会解析 SDL3、SDL3_image、SDL3_ttf 和 FFmpeg。SDL 依赖缺失时自动
下载固定版本；缺少 FFmpeg 时会显示按平台安装 `avcodec`、`avformat`、`avutil`、`swscale`、
`swresample` 的修复指引。

### XJ380

XJ380 SDK 不进入 Git。优先在配置时指定 `XJ380_SDK`，其值应指向 SDK 解压后的
`XJ380_CPP_API_Depend_1_3` 目录：

```text
XJ380_CPP_API_Depend_1_3/include
XJ380_CPP_API_Depend_1_3/obj-gui
```

未指定时，CMake 会从 XXCC-suite 的 2026v4 Release 下载 ZIP 到构建目录的 `_deps/`，并校验
SHA-256。使用 `-DBRIDGEENGINE_FETCH_XJ380=OFF` 可禁用自动下载，此时必须提供 `XJ380_SDK`。

构建目标：

```bash
cmake --preset xj380
cmake --build --preset xj380 --target xj380_staticlib
cmake --build --preset xj380 --target xj380_package
cmake --build --preset xj380 --target xj380_epf
```

若 SDK 位于其他位置，可在配置时指定：

```bash
cmake --preset xj380 -DXJ380_SDK=/实际路径/XJ380_CPP_API_Depend_1_3
```

外部 XJ380 项目应先构建 `xj380_package`，再将 `build/xj380/package/include/` 和
`build/xj380/package/lib/` 复制到项目中，或作为 xxcc 的头文件和库搜索路径。包内提供
`BridgeEngine.h`、生成的 `bridgeengine_version.h` 与 `libbridgeengine.a`；项目仍需自行提供 XJ380 SDK 的
系统头、链接脚本和运行时对象。

---

## 后端说明

桌面 SDL3 后端使用 SDL3_ttf 加载字体文件，并通过 FFmpeg 实现视频。

XJ380 后端使用 XAPI 内置文本绘制，当前不加载 TTF 文件。XJ380 用户态头文件里的 `WSTR` 实际是 `char *`，BridgeEngine 不把它当作 UTF-16 或 `wchar_t`。XJ380 不支持音频和视频：`bapi_audio_init()` 与 `bapi_video_init()` 返回非零，声音和视频加载返回 `NULL`，并通过平台 warning 说明原因。调用方应检查这些既有返回值；不会新增媒体 capability 查询 BAPI。

## 运行时与兼容性

BridgeEngine 的运行时为单实例、单线程调用模型；当前 context-less API 不承诺多实例或并发调用。
`BridgeEngine.h` 继续是完整聚合入口，模块化头仅作为增量兼容路径。

XML 持久化和 UI XML 仅支持各自的受限格式，不是通用 XML parser。当前兼容补丁刻意不改变
scene/level 所有权、圆形或多边形的填充视觉语义、XML 字节格式，或无 PTS 视频的 FPS fallback。
XML 转义、真正的填充绘制、PTS/VFR 调度和 scene/level 所有权模型均为后续版本的独立设计工作。

---

## 最简单的例子

```c
#include <BridgeEngine.h>

int main(void)
{
    if (bapi_engine_init("Hello BridgeEngine", 800, 600) != 0) return 1;

    bapi_event_t event;
    int running = 1;
    while (running) {
        while (bapi_poll_event(&event)) {
            if (bapi_event_get_type(&event) == BAPI_EVENT_QUIT) running = 0;
        }
        bapi_render_clear();
        bapi_draw_circle(400, 300, 80, bapi_color(255, 255, 255, 255));
        bapi_render_present();
    }

    bapi_engine_quit();
    return 0;
}
```

`bapi_engine_quit()` 会先释放引擎仍追踪的纹理、音频和视频资源，再销毁 renderer、窗口和平台。退出后所有
资源句柄及借用的 window/renderer 句柄均失效，不应再使用或释放。

---

## 项目结构

```text
BridgeEngine/
├── include/            # 唯一公共头文件 BridgeEngine.h
├── src/                # 引擎实现
│   ├── core/           # 初始化、渲染上下文与版本
│   ├── internal/       # 平台及其他私有实现头
│   ├── platform/       # 平台层 (SDL3 / XJ380)
│   └── ...
├── examples/           # 桌面、文字、日志和 XJ380 示例及资源
├── uikit/              # UI Kit（Dear ImGui 独立工具）
├── cli/                # bcli 项目管理命令行
├── templates/          # 项目模板
├── tests/              # 回归测试
├── docs/               # 架构、API 与 CLI 文档
├── cmake/              # CMake 模块
├── scripts/            # 本地依赖引导脚本
├── CMakeLists.txt
└── README.md
```

完整 API 文档请查阅 [docs/bapi.md](docs/bapi.md)。

## 源码兼容性

v2 的公开接口仅为 `BridgeEngine.h`。旧项目若仍包含 `audio/audio.h`、`render/draw.h` 等拆分头路径，可额外将包内的 `include/legacy/` 加入头文件搜索路径；其中的兼容头会转发到当前聚合头。
这不包含 `bapi_internal.h`、`scene/scene_internal.h`、`bapi_event_t` 的旧平台内部字段、`bapi_texture_internal` 的旧内部字段，或已编译二进制的 ABI 兼容性。

---
## 相关项目
仓库内置的 `bcli` 负责项目创建、构建和运行；外部
[BridgeEngine CLI](https://github.com/GHhuang1057/becli) 作为相关项目保留。

## 第三方组件

| 组件 | 用途 | 许可证 | 位置 |
|------|------|--------|------|
| RZip | RZip (`.rz`) 资源包读取，`src/pack.c` 封装 | BSD 3-Clause，Copyright (c) 2026, Rainy101112 | `thirdparty/rzip/` |

RZip 以源码形式编译进 BridgeEngine 库（`rz_lib.c`）。其完整许可证文本见
[`thirdparty/rzip/LICENSE`](thirdparty/rzip/LICENSE)；二进制分发时请随发行物一并提供该文件。
SDK 其余依赖（SDL3、SDL3_image、SDL3_ttf、FFmpeg）由使用者按需安装，不随引擎分发。

## License

[XINGJI Open Sources License 2.0](LICENSE)