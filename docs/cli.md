# bcli — 项目命令行工具

`bcli` 是 BridgeEngine 的项目管理 CLI，也是后续 UI Kit 和完整游戏编辑器的
“项目后端”。它不链接 SDL/FFmpeg，也不启动引擎运行时，只负责项目文件、模板、
目录结构和构建命令。

## 为什么先做 CLI

完整游戏编辑器需要的底层能力其实是同一套：

- 找到项目根目录
- 读写 `.bep` 项目文件
- 增删 UI 文档
- 生成/更新模板
- 调用 CMake 构建
- 找到产物并运行
- 输出机器可读的项目信息

先把这些做成稳定的 CLI，后续编辑器可以直接复用同一套语义，而不是在 GUI 里再实现
一遍项目逻辑。

## 命令

```bash
bcli new <name> [--dir DIR] [--engine PATH]
bcli init [--name NAME] [--engine PATH]
bcli info [--json]
bcli build [--debug|--release] [--clean] [--reconfigure]
bcli run [--build] [--debug|--release] [-- ARGS...]
bcli clean
bcli add-doc <relative-path>
bcli remove-doc <relative-path>
bcli list-docs
bcli set-engine <path>
bcli pack [--assets DIR] [--out FILE] [--atlas DIR] [--bctex bc1|bc3] [--no-rz]
bcli ui validate [FILE|DIR]
bcli watch [--build] [--run] [--interval MS]
bcli doctor
bcli self-test
```

### 新建项目

```bash
bcli new my_game --dir ~/projects --engine ~/BridgeEngine
cd ~/projects/my_game
bcli build
bcli run
```

`--engine` 省略时会使用：

1. `BRIDGEENGINE_SOURCE_DIR` 环境变量
2. `bcli` 编译时记录的引擎源码目录

### 初始化已有目录

```bash
mkdir my_game && cd my_game
bcli init --name my_game --engine ~/BridgeEngine
```

当前目录已存在 `CMakeLists.txt`、`main.c` 或 `project.bep` 时会拒绝，避免覆盖。

### 查看项目

```bash
bcli info
bcli info --json
```

`--json` 是给 UI Kit / 未来编辑器用的稳定接口：

```json
{
  "root": "/home/me/my_game",
  "name": "my_game",
  "engine": "/home/me/BridgeEngine",
  "bep": "/home/me/my_game/my_game.bep",
  "build_dir": "/home/me/my_game/build",
  "documents": ["ui/menu.xml", "ui/game.xml"],
  "executable": "/home/me/my_game/build/my_game"
}
```

### 文档管理

```bash
bcli add-doc ui/hud.xml
bcli remove-doc ui/old.xml
bcli list-docs
```

只接受项目内相对路径；`../` 和绝对路径会被拒绝。添加/删除只改 `.bep`，不移动文件。

### 构建和运行

```bash
bcli build                 # 默认 Debug，自动 configure
bcli build --release
bcli build --clean         # 删除 build/ 后重配
bcli build --reconfigure
bcli run                   # 没有产物时自动构建
bcli run --release -- --fullscreen
```

构建目录固定为项目根目录下的 `build/`。CLI 调用 CMake，不自己维护编译逻辑。

### 诊断

```bash
bcli doctor
```

检查 `cmake`、`ninja`、C/C++ 编译器、SDL3、SDL3_image、SDL3_ttf、FFmpeg、项目模板和
引擎源码。引擎源码下的 `.bridgeengine-deps` 会自动加入 `PKG_CONFIG_PATH`。

### 资源打包

```bash
bcli pack                                  # assets/ -> build/assets.rz（默认）
bcli pack --assets art --out build/art.rz
bcli pack --atlas assets/images --out build/atlas.rz
bcli pack --atlas assets/images --out build/atlas.rz --bctex bc1
```

`pack` 不自己实现打包格式，而是调用引擎构建出来的三个工具：
`bridgeengine_rz_pack`、`bridgeengine_atlas_pack`、`bridgeengine_bctex_pack`。

- `.rz`（默认）：递归收集 `--assets` 下的文件，执行
  `bridgeengine_rz_pack -C <assets> -o <out> <相对路径...>`。
- `--atlas DIR`：递归收集图片，执行
  `bridgeengine_atlas_pack -o <out目录> -n atlas --format png <图片...>`。
  图集页和清单默认写到 `build/assets/`（`--out` 的父目录 + 文件名主干），
  生成 `atlas.png` 和 `atlas.atlas`；随后默认再用 `rz_pack` 把该目录打进
  `--out`。若 `--out` 是一个目录（无扩展名或已存在目录）或传了 `--no-rz`，
  则只产出图集文件，不生成归档。
- `--bctex bc1|bc3`：在生成 `atlas.png` 后调用
  `bridgeengine_bctex_pack -f <格式> -o <out目录>/atlas.bctex <atlas.png>`。
  注意 `.atlas` 清单仍然指向 `atlas.png`；`.bctex` 是额外的压缩页，
  运行时要按你的资源加载路径选择它。

工具查找顺序：`BRIDGEENGINE_TOOLS_DIR` → `PATH` → 引擎构建目录
（`<engine>/build` 及 `<engine>/build/*`、`<engine>/build/*/bin`）→ `bcli`
所在目录。缺工具时会打印期望的工具名、查找位置和构建目标，并返回非零。

### UI 校验

```bash
bcli ui validate                    # 校验 .bep 中 [documents] 列出的文件
bcli ui validate ui/menu.xml        # 单个文件
bcli ui validate ui/                # 目录下所有 .xml（递归）
```

不依赖引擎运行时，内置一个小型 XML 扫描器，检查：

- 开/闭标签匹配、属性引号闭合、单根元素
- 同一文档内的重复 `id`
- 每个 `src="..."` 相对 XML 所在目录解析后文件是否存在（POSIX 区分大小写）

每个错误输出一行 `path:line: message`，最后打印汇总；有错误时返回 1。

### 资产清单

```bash
bcli assets scan                     # 扫描引用与 assets/ 下的文件，写 assets.manifest
bcli assets scan --root . --out assets.manifest
bcli assets verify                   # 校验清单里每个条目是否存在
```

`scan` 收集这些来源，去重排序后按项目相对路径写入：

- `.bep` 的 `[documents]` 条目
- `assets/` 下的全部文件
- `*.btm` 中的 `tileset <path> <columns>`
- `*.bworld` 中的 `sprite <path> ...`
- `ui/*.xml` 与 `assets/**/*.xml` 中的 `src="..."`

`verify` 逐条检查，缺失时打印 `missing <path>` 并返回 1。`bcli build` 在
`assets.manifest` 存在时会先做一次校验，缺资产直接中断，避免运行时黑屏。

### 文件监视

```bash
bcli watch
bcli watch --interval 500
bcli watch --run
bcli watch --build --run       # --build 为兼容保留：watch 变更时总会构建
```

每 `--interval` 毫秒（默认 250）轮询 `CMakeLists.txt`、根目录 `*.bep`、
`src/`、`assets/`、`ui/` 的修改时间，忽略 `build/`。检测到变化后打印时间戳行，
调用与 `bcli build` 相同的构建流程；`--run` 会在构建成功后运行产物。
Ctrl+C 结束。

## `.bep` 格式

```ini
# BridgeEngine Project
name = my_game
engine = /home/me/BridgeEngine

[documents]
ui/menu.xml
ui/game.xml
ui/settings.xml
```

- `name`：C 标识符，也是可执行文件名
- `engine`：引擎源码目录，构建时传给 CMake
- `[documents]`：UI 文档相对路径，顺序稳定

CLI 写文件时使用临时文件 + rename，避免半写入。

## 与 UI Kit 的关系

UI Kit 目前自带一套项目打开/保存逻辑。后续应改为：

- `bcli info --json` 读取项目
- `bcli add-doc` / `remove-doc` 修改文档列表
- `bcli build` / `bcli run` 作为唯一构建入口

这样 UI Kit 和未来完整编辑器不会各自维护一份项目格式。

## 后续命令

计划中但本轮未实现：

- `bcli project migrate`：`.bep` 格式迁移
- `bcli new --template game`：垂直切片模板
