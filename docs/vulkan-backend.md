# Vulkan (SDL_GPU) 渲染后端

## 目标

为桌面 SDL3 路径提供一个可选的 Vulkan 渲染后端，同时保留 SDL_Renderer 回退。默认
`bapi_engine_init()` 优先 Vulkan；不可用时自动回退，避免旧驱动/无 Vulkan 设备直接无法启动。

## 为什么用 SDL_GPU 而不是手写 Vulkan

- SDL_GPU 内部使用 Vulkan（`SDL_CreateGPUDeviceWithProperties` 指定 `"vulkan"`），同时处理
  交换链、格式、内存、同步和驱动差异。
- 手写 Vulkan 需要自己维护实例/设备/队列/交换链/描述符/管线/同步/格式兼容，成本远高于
  一个 2D 引擎当前能获得的收益。
- SDL_GPU 原生支持 BC1/BC3 等压缩纹理格式，满足块压缩目标；手写 Vulkan 也要做同样的
  格式探测和上传逻辑。

## 兼容性

- Vulkan 1.0 + `VK_KHR_swapchain` + `VK_KHR_maintenance1`（SDL_GPU 的 Vulkan 要求）。
- 设备创建时关闭非必需特性，扩大老设备和移动端覆盖：
  - clip distance
  - depth clamp
  - indirect draw first instance
  - anisotropy
- 不要求计算管线、动态渲染、Vulkan 1.1/1.2 特性。
- 纹理格式通过 `SDL_GPUTextureSupportsFormat` 探测；BC1/BC3 不可用时调用方可回退 RGBA。
- 无 Vulkan 设备时 `BAPI_RENDERER_AUTO` 回退 SDL_Renderer；显式
  `BAPI_RENDERER_VULKAN` 返回初始化失败。
- UI Kit 固定 `BAPI_RENDERER_SDL`，避免预览面板依赖的 SDL_Renderer 原生句柄失效。

## 实现

- `src/platform/sdl3/sdlgpu_renderer.c`：设备、交换链、管线、采样器、白色纹理、顶点缓冲、
  批处理、纹理创建/上传/销毁。
- `src/platform/sdl3/shaders/`：GLSL 源；SPIR-V 以 C 数组形式放入
  `sdlgpu_vert_spv.h` / `sdlgpu_frag_spv.h`，构建不依赖 glslc。
- `src/platform/sdl3/platform_sdl3.c`：在同一个平台接口内按 `renderer->gpu` 分发；窗口、
  事件、音频、TTF、IO 保持原有实现。
- `src/internal/platform/sdl3_internal.h`：共享 window/renderer/texture 结构与 GPU 接口。

## 验证

已用 Mesa lavapipe（软件 Vulkan）在 headless/SDL offscreen 环境验证：

- `SDL_CreateGPUDeviceWithProperties` 创建 Vulkan 设备成功
- 引擎 `AUTO` 选择 Vulkan，`bapi_engine_get_renderer_backend() == BAPI_RENDERER_VULKAN`
- 图元、纹理、旋转 sprite、sprite batch、`.bctex`（BC1）绘制无错误
- 默认预设 + 本地依赖完整构建 357 个目标通过，39/39 测试通过

关键点：`SDL_CreateGPUDeviceWithProperties` 必须显式设置
`SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN = true`，否则 SDL_GPU 会报
`SDL_HINT_GPU_DRIVER vulkan unsupported!`。

## 当前限制

- 每帧 `SDL_WaitForGPUIdle`，优先正确性；后续改为帧环 + fence。
- 纹理上传在加载时同步等待；帧内创建的文本纹理通过当前 command buffer 的 copy pass 上传。
- 压缩纹理格式平台层已支持创建/上传；`.bctex` 装载器与 `bridgeengine_bctex_pack`
  （BC1/BC3）已接入，Vulkan 路径直接上传压缩数据，SDL_Renderer 路径 CPU 解码为 RGBA。
- minios/XJ380 不使用此路径。
