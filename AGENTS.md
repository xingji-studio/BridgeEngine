# AGENTS.md — BridgeEngine

AI agent 开发参考。先读这个，再改代码。

## 项目速览

鹊桥引擎，跨平台 2D 图形引擎，引擎核心用 C11 写成；XJ380 适配层用 C++11，UI Kit 用 C++14，bcli 用 C++17。对外暴露一个单头 C API：`include/BridgeEngine.h`。

两套后端：
- **桌面**：SDL3 + FFmpeg，走 `src/platform/sdl3/`
- **XJ380**：XXCC 交叉编译，走 `src/platform/xj380/`

## 改代码前必须看清楚的事

### 哪几个文件绝对不能改坏

- `include/BridgeEngine.h` — 唯一的公开头，v2 API 基线，只能追加，不能删/改签名
- `src/internal/platform/platform.h` — 平台抽象层的 vtable 定义，所有后端都实现它
- `src/core/render_context.c` — 引擎单例运行时，启动/停止顺序有严格依赖
- `tests/` — 每个 test 编译自己的依赖源文件（不链接完整库），改内部结构要同步改 test 的源文件列表

### 改 API 的约束

在 `BridgeEngine.h` 里新增函数/类型/枚举值之前：
1. 确认不加它是不是也能解决问题
2. 只追加，不动现有签名的返回值、参数类型、枚举数值
3. 函数先在实现处写了再声明，声明的注释要跟实现吻合
4. 如果加了类型，跑 `tests/public_header_c_test.c` 和 `tests/public_header_cpp_test.cpp` 确认 C/C++ 都能编过

### 平台分支的写法

源码里看到 `#ifdef USE_BACKEND_XJ380` / `#ifdef __XJ380_OS__`  之类，是有意为之的。XJ380 的 C 运行时不完整，sinf/cosf 得用 Taylor 展开替代，时间用 `xapi_GetTime()`，不是写错了。

加平台分支时：
- 用宏隔离，不要在公共函数里 if/else 动态判断后端
- XJ380 侧能少写就少写——不支持音频和视频，各自返回非零/NULL 并记录一次 warning 即可
- 别加公开的 capability 查询 API，调用方自己检查返回值

## 项目结构

```
include/                ← 唯一的 BridgeEngine.h，加 legacy/ 兼容旧拆分路径
src/
  core/                 ← init.c (公开 API), render_context.c (生命周期), version.c
  platform/             ← platform.c (单例), sdl3/ (含 SDL_GPU/Vulkan), xj380/
  internal/             ← 私有头，按 engine/platform/scene 分组
  input/                ← 键盘/鼠标状态、鼠标绘制
  audio.c, music.c (流式 BGM), music_stub.c (XJ380), texture.c, video/, text.c, draw.c, math.c, camera.c
  frame.c               ← 帧计时/固定步长/暂停/时间缩放
  transform.c           ← 2D 仿射变换栈与摄像机矩阵
  sprite_batch.c        ← 图集 sprite 批处理（显式 begin/draw/end）
  atlas.c               ← .atlas 清单解析、region 查询与绘制
  entity.c              ← 轻量场景实体/父子层级/sprite 绘制
  error.c               ← bapi_get_last_error / 内部错误文本
src/platform/sdl3/sdlgpu_renderer.c ← SDL_GPU/Vulkan 渲染器
src/platform/sdl3/shaders/          ← GLSL 源与预编译 SPIR-V 头
  scene.c, level.c, button.c, xml_loader.c, log.c, io.c
uikit/                  ← 独立 UI Kit（Dear ImGui，目标 bridgeengine_uikit）
cli/                    ← bcli 项目管理 CLI（目标 bridgeengine_cli，输出 bcli）
tests/                  ← 每个 ctest 编译自己的源文件子集，不链接 lib
examples/               ← desktop, text, log, xj380 四个示例
cmake/                  ← FindFFmpeg, BridgeEngineConfig.cmake.in, version header 模版
tools/                  ← uix_encrypt.c, atlas_pack.c, bctex_pack.c, rz_pack.c（构建期烘焙）
scripts/                ← bootstrap-vcpkg.ps1, bootstrap-ffmpeg.sh
docs/                   ← architecture.md, bapi.md, cli.md, vulkan-backend.md
```

## 构建和测试

```bash
# 本地开发（需 SDL3 + FFmpeg）
cmake --preset default        # → build/（含 UI Kit，自动使用 .bridgeengine-deps）
cmake --preset local          # 跳过 UI Kit，使用本地依赖
cmake --build --preset default

# 跑回归测试
ctest --test-dir build --output-on-failure

# XJ380 交叉编译
cmake --preset xj380
cmake --build --preset xj380 --target xj380_staticlib
```

CMake 默认开了 `CMAKE_EXPORT_COMPILE_COMMANDS`，`build/compile_commands.json` 自动生成，LSP 可以直接用。

### 跑单个测试

```bash
cmake --build build --target bridgeengine_math_geometry_test
ctest --test-dir build -R bridgeengine.math_geometry --output-on-failure
```

## 写新功能的标准流程

1. **在 `include/BridgeEngine.h` 里声明公开 API**——只追加，放在已有分组里
2. **在 `src/` 对应模块实现**——引 `BridgeEngine.h` 和必要的 internal 头
3. **在 `src/internal/` 加私有结构**——如果不涉及平台差异
4. **需要平台差异时**：
   - 先在 `platform_types.h` / `platform.h` 里加 vtable 槽位
   - 两个后端都实现（XJ380 侧做 no-op 或返回失败）
   - 业务逻辑写在 `src/` 模块里，通过 `plat_get()` 调平台函数
5. **加 test**——在 `CMakeLists.txt` 里照着已有 test 的 pattern 写，编译自己需要的源文件
6. **更新 docs/**——API 语义有变化就更新 `docs/bapi.md`，架构变化就更新 `docs/architecture.md`
7. **完整构建并跑全量测试**

## 代码风格（clang-format 加的手工约定）

clang-format 配了 LLVM + 100 列 + 缩进用 tab。下面这些是 format 管不到的：

- 全局变量用 `g_` 前缀（`g_runtime`, `g_log_state`）
- 平台相关静态变量不加前缀，但要 static
- `typedef struct` 用 `_t` 后缀（`bapi_event_t`, `plat_interface_t`）
- 枚举值全部大写、前缀跟模块走（`BAPI_EVENT_*`, `PLAT_*`, `KEY_*`）
- 内部函数不搞 `static`  就别暴露到头文件里——哪怕只是 `extern` 也不行，C 没有 namespace
- `malloc` 完了要判 NULL，`free` 之前允许传 NULL
- 模块里有内部链表（audio 的 allocated/active，texture 的 cache），养成习惯：增删节点后把 next 置 NULL

## commit 风格

```
type: 简短描述
```

type: `feat`, `fix`, `build`, `ci`, `docs`, `refactor`, `test`

不要 suffix `!` 表示 breaking change——v2 就是 non-breaking 的。

CI 会拒绝提交二进制文件（`.dll`, `.exe`, `.o`, `.a`, `.so`, `.epf`），也别把 `XJ380_SDK/` 或 `build/` 加进 git。

## 写测试的注意点

- 测试是 self-contained 的：在 CMakeLists 里手动用 `add_executable` 列出需要编译的 `tests/xxx.c` + `src/xxx.c`
- 不链接 `bridgeengine_shared` 或 `bridgeengine_static`
- 需要 mock 平台行为时参考 `tests/lifecycle_test.c`：定义假的 struct 和 counter，在 main 里把平台借口的函数指针替换掉
- 返回 0 = 成功，1 = 失败，跟 CTest 的约定一致
- 命名：`bridgeengine.<topic>`，CMake target 和 ctest name 保持一致

## 常见坑

- **改 `platform.h` 的任何 struct** → 必须把 SDL3 和 XJ380 两个后端的实现也改了，不然编译直接炸
- **Windows MSVC**：`/W4` 很严，写 `float` 的地方别塞 `int`，Uint64 要显式 cast 到 uint32_t
- **给 XJ380 加 C 源文件** → 同时更新 `CMakeLists.txt` 里的 `XJ380_C_SOURCES` 列表，别忘了
- **`bapi_engine_quit()` 的释放顺序**：视频 → 音频 → 纹理 → 文字 → TTF → renderer → window → 平台。不能改顺序，test `lifecycle_test.c` 会验证
- **libc 函数在 XJ380 上不可用**：sinf/cosf/sqrtf 在 CMakeLists 里被映射到 baremetal 版本，别在 XJ380 路径里用标准 C 的 `<math.h>`
