SDL_PKG_CONFIG	:= $(shell pkg-config --cflags --libs sdl3)
SDL_IMAGE_PKG_CONFIG := $(shell pkg-config --cflags --libs sdl3-image)
SDL_TTF_PKG_CONFIG := $(shell pkg-config --cflags --libs sdl3-ttf)

CC				:= gcc
C_FLAGS			:= -Wall -Wextra -O2 -g3 -I include -fPIC

C_SOURCES		:= $(filter-out text_example.c main.c examples/simple_example.c examples/be_example.c,$(shell find * -name "*.c")) engine/version.c
LIB_OBJS		:= $(C_SOURCES:%.c=%.o)
MAIN_OBJS		:= $(filter-out main.o,$(C_SOURCES:%.c=%.o)) main.o

all: lib staticlib main

%.o: %.c
		@printf "**CC** $< -> $@\n"
		@$(CC) $(C_FLAGS) -c -o $@ $<

lib: $(LIB_OBJS)
		@printf "**LIB** $^ -> libbridgeengine.so\n"
		@$(CC) $(C_FLAGS) -shared -fPIC $^ -o libbridgeengine.so $(SDL_PKG_CONFIG) $(SDL_IMAGE_PKG_CONFIG) $(SDL_TTF_PKG_CONFIG)

staticlib: $(LIB_OBJS)
		@printf "**AR** $^ -> libbridgeengine.a\n"
		@ar rcs libbridgeengine.a $^

# 构建main
main: $(MAIN_OBJS)
		@printf "**LINK** $^ -> main\n"
		@$(CC) $(C_FLAGS) $^ -o main $(SDL_PKG_CONFIG) $(SDL_IMAGE_PKG_CONFIG) $(SDL_TTF_PKG_CONFIG)

text_example: text_example.o engine/master/init.o engine/render/create.o engine/text.o
		@printf "**LINK** $^ -> text_example\n"
		@$(CC) $(C_FLAGS) $^ -o text_example $(SDL_PKG_CONFIG) $(SDL_TTF_PKG_CONFIG)

%.fmt: %
		@printf "**fmt** $< ...\n"
		@clang-format -i $<

%.tidy: %
		@printf "**tidy** $< ...\n"
		@clang-tidy $< -- $(C_FLAGS)

.PHONY: format check clean

format: $(C_SOURCES:%=%.fmt) $(S_SOURCES:%=%.fmt) $(HEADERS:%=%.fmt)
		@printf "**fmt** done \n"

check: $(C_SOURCES:%=%.tidy) $(S_SOURCES:%=%.tidy) $(HEADERS:%=%.tidy)
		@printf "**check** done \n"

clean:
	@printf "Removing $(LIB_OBJS) $(MAIN_OBJS) main libbridgeengine.so libbridgeengine.a text_example\n"
	@rm -f $(LIB_OBJS) $(MAIN_OBJS) main libbridgeengine.so libbridgeengine.a text_example

install: lib
	@mkdir -p $(DESTDIR)/usr/local/lib
	@mkdir -p $(DESTDIR)/usr/local/include
	@mkdir -p $(DESTDIR)/usr/local/lib/pkgconfig
	@cp libbridgeengine.so libbridgeengine.a $(DESTDIR)/usr/local/lib/
	@cp -r include/* $(DESTDIR)/usr/local/include/
	@cp bridgeengine.pc $(DESTDIR)/usr/local/lib/pkgconfig/
	@printf "BridgeEngine installed successfully!\n"