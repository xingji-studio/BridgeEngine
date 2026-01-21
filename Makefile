# vcpkg paths
VCPKG_ROOT := H:/vcpkg
VCPKG_INCLUDE_DIR := $(VCPKG_ROOT)/packages/sdl3_x64-windows/include
VCPKG_INCLUDE_DIR := $(VCPKG_INCLUDE_DIR) $(VCPKG_ROOT)/packages/sdl3-image_x64-windows/include
VCPKG_INCLUDE_DIR := $(VCPKG_INCLUDE_DIR) $(VCPKG_ROOT)/packages/sdl3-ttf_x64-windows/include

VCPKG_LIB_DIR := $(VCPKG_ROOT)/packages/sdl3_x64-windows/lib
VCPKG_LIB_DIR := $(VCPKG_LIB_DIR) $(VCPKG_ROOT)/packages/sdl3-image_x64-windows/lib
VCPKG_LIB_DIR := $(VCPKG_LIB_DIR) $(VCPKG_ROOT)/packages/sdl3-ttf_x64-windows/lib

# Compiler settings
CC := gcc
C_FLAGS := -Wall -Wextra -O2 -g3 -I include -I $(VCPKG_ROOT)/packages/sdl3_x64-windows/include -I $(VCPKG_ROOT)/packages/sdl3-image_x64-windows/include -I $(VCPKG_ROOT)/packages/sdl3-ttf_x64-windows/include -fPIC -DBAPI_LOG_ENABLED

# Linker settings
LD_FLAGS := -L $(VCPKG_ROOT)/packages/sdl3_x64-windows/lib -L $(VCPKG_ROOT)/packages/sdl3-image_x64-windows/lib -L $(VCPKG_ROOT)/packages/sdl3-ttf_x64-windows/lib
LIBS := -lSDL3 -lSDL3_image -lSDL3_ttf -lopengl32 -lgdi32 -luser32 -lkernel32 -lshell32

# Source files
C_SOURCES := engine/master/init.c engine/render/create.c engine/render/draw.c engine/mouse_drawing.c engine/text.c engine/version.c engine/log.c
LIB_OBJS := $(C_SOURCES:%.c=%.o)
MAIN_OBJS := $(LIB_OBJS) main.o

all: lib main

# Compile rules
%.o: %.c
	@echo CC $< -> $@
	@$(CC) $(C_FLAGS) -c -o $@ $<

# Build shared library
lib: $(LIB_OBJS)
	@echo LIB $^ -> libbridgeengine.so
	@$(CC) $(C_FLAGS) $(LD_FLAGS) -shared -fPIC $^ -o libbridgeengine.so $(LIBS)

# Build static library
staticlib: engine/master/init.o engine/render/create.o engine/render/draw.o engine/mouse_drawing.o engine/text.o engine/version.o engine/log.o
	@ar cr libbridgeengine.a $^

# Build main executable
main: $(MAIN_OBJS)
	@echo LINK $^ -> main
	@$(CC) $(C_FLAGS) $(LD_FLAGS) $^ -o main.exe $(LIBS)

# Build text example
text_example: text_example.o engine/master/init.o engine/render/create.o engine/text.o
	@echo LINK $^ -> text_example
	@$(CC) $(C_FLAGS) $(LD_FLAGS) $^ -o text_example $(LIBS)

# Formatting
%.fmt: %
	@echo fmt $< ...
	@clang-format -i $<

# Tidy
%.tidy: %
	@echo tidy $< ...
	@clang-tidy $< -- $(C_FLAGS)

.PHONY: format check clean

format: $(C_SOURCES:%=%.fmt) $(HEADERS:%=%.fmt)
	@echo fmt done

check: $(C_SOURCES:%=%.tidy) $(S_SOURCES:%=%.tidy) $(HEADERS:%=%.tidy)
	@echo check done

# Clean
clean:
	@echo Removing $(LIB_OBJS) main.o main.exe libbridgeengine.so libbridgeengine.a text_example.exe
	@del /f /q engine\master\init.o engine\render\create.o engine\render\draw.o engine\mouse_drawing.o engine\text.o engine\version.o main.o main.exe libbridgeengine.so libbridgeengine.a text_example.exe 2>nul

# Install
install: lib
	@mkdir -p $(DESTDIR)/usr/local/lib
	@mkdir -p $(DESTDIR)/usr/local/include
	@mkdir -p $(DESTDIR)/usr/local/lib/pkgconfig
	@cp libbridgeengine.so libbridgeengine.a $(DESTDIR)/usr/local/lib/
	@cp -r include/* $(DESTDIR)/usr/local/include/
	@cp bridgeengine.pc $(DESTDIR)/usr/local/lib/pkgconfig/
	@echo BridgeEngine installed successfully!