# 文字绘制与资源包查询优化

基线提交：`3dfc48fcf5dda4afd6cc0cdc06a792b6c53bbe50`。本次只修改引擎内部缓存和资源包索引，
不改公开 API、平台 vtable、归档格式或 UI 文件格式。

## 实测结果

Linux x86_64、GCC 13.3.0、`-O2`，相同测试程序、输入与编译参数，运行三次取中位数。
时间来自 C `clock()`，为进程 CPU 时间；不包含构建、资源包生成及打开时间。

| 工作负载 | 原版 | 修改版 |
| --- | ---: | ---: |
| 4 个固定标签绘制 10,000 帧：光栅化次数 | 40,000 | 4 |
| 同一文字工作负载：纹理创建次数 | 40,000 | 4 |
| 同一文字工作负载：文字测量次数 | 40,000 | 4 |
| 4,097 条目的包，50,000 次名称查找 | 373.024 ms | 9.503 ms |
| 按序号遍历前 4,096 个条目，重复 20 次 | 529.685 ms | 0.142 ms |

名称查找在该负载下约快 **39 倍**。这是一次合成微基准，收益随包大小、访问方式和硬件变化。
序号枚举修改后的耗时很小，容易受计时精度影响；更可靠的结论是单次查询由 O(n) 变为 O(1)。
打开包时新增 O(n log n) 建索引开销及 O(n) 元数据空间，因此只打开、不查询的负载不会受益。

文字基准使用 mock 平台，验证的是减少工作次数，不代表真实 SDL/GPU 帧率提升。
4 个标签始终在缓存内；大量每帧变化的文字、超预算纹理或频繁字体淘汰会降低命中率。
桌面缓存默认 128 条 / 4 MiB，XJ380 默认 32 条 / 256 KiB（纹理估算大小加文字键）；
基准使用测试专用的 8 条 / 4 KiB 限制。缓存所有权和预算细节见 [architecture.md](architecture.md)。

## 复现

无需 SDL/FFmpeg 即可构建独立回归测试：

```sh
cmake -S . -B build/check -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBRIDGEENGINE_BUILD_SHARED=OFF \
  -DBRIDGEENGINE_BUILD_STATIC=OFF \
  -DBRIDGEENGINE_BUILD_EXAMPLES=OFF \
  -DBRIDGEENGINE_BUILD_EDITOR=OFF \
  -DBRIDGEENGINE_BUILD_XJ380=OFF \
  -DBRIDGEENGINE_BUILD_TESTS=ON
cmake --build build/check
ctest --test-dir build/check --output-on-failure
python3 scripts/benchmark-hotpaths.py \
  --baseline-ref 3dfc48fcf5dda4afd6cc0cdc06a792b6c53bbe50 \
  --build-dir build/check
```

基准脚本需要 Python 3、Git 和支持 GCC 风格参数的 C11 编译器；可用 `--cc` 指定编译器。
它从指定提交读取原版 `src/text.c` / `src/pack.c`，并用当前相同的测试桩编译两版模块。
临时程序和测试资源包自动清理，不改变工作树。两项测试也支持单独运行：

```sh
build/check/bridgeengine_text_cache_test --benchmark
build/check/bridgeengine_pack_index_test build/check/bench.rz --benchmark
```

## 验证范围

- 独立 CTest：27/27 通过，覆盖原有测试与新增文字缓存、资源包索引测试。
- AddressSanitizer + UndefinedBehaviorSanitizer：27/27 通过。
  当前执行环境不允许 LeakSanitizer 读取进程信息，运行时使用 `ASAN_OPTIONS=detect_leaks=0`；
  不将此结果计为通过泄漏检测。文字测试另以分配/释放计数和真实运行时退出顺序检查资源清理。
- 新增文字测试覆盖位置、RGBA、UTF-8、调用方修改字符串、字号量化、两类预算淘汰、
  多条目淘汰、超大纹理、失败重试、字体淘汰、清理幂等和引擎重启。
- 新增资源包测试用带 CRC 的 4,097 条目归档验证原始序号、查找边界、首个同名条目、
  整文件和流式读取。首尾同名条目的数据不同，防止排序后误读。
- 完整桌面构建已尝试，停在缺少 X11/Wayland 开发依赖；SDL/FFmpeg 后端集成、
  编辑器画面、真实 GPU 帧率和 XJ380 交叉编译尚未在本环境验证。
