# BridgeEngine
## 鹊桥引擎 (ZH)
鹊桥引擎是一个跨平台的2D图形引擎。
* 查看api文档请点击[这里](BAPI_README.md)
### 构建
```bash
# 动态库
make lib

# 静态库
make staticlib

# 同时构建动态库和静态库
make lib staticlib
```

这将生成：
- `libbridgeengine.so` - 动态库
- `libbridgeengine.a` - 静态库

```
# 构建示例程序
cd examples && make -f Makefile.examples clean && make -f Makefile.examples
# 构建main
make main
```
把libbridgeengine.so放进程序下相同目录即可运行：）


## BridgeEngine (EN)
BridgeEngine is a multi-platform 2D graphics engine.
