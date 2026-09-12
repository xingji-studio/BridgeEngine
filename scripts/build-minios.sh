#!/bin/bash
# Build BridgeEngine sources for the MiniOS backend using the standard gcc toolchain.
set -e
cd "$(dirname "$0")/.."
SDK="-I../../minios-m/sdk/include"
FLAGS="-c -std=gnu11 -Iinclude -Isrc $SDK -Iplatform/minios -DUSE_BACKEND_MINIOS -DBAPI_LOG_ENABLED -O2 -g3 -Wall -Wextra -m64 -fno-stack-protector -fcf-protection=none -fno-pic -fno-pie"
OUT=build/bridge-minios
mkdir -p $OUT
ok=0
for f in \
    src/platform/platform.c \
    src/platform/minios/platform_minios.c \
    src/core/render_context.c src/core/init.c src/core/version.c \
    src/io.c src/log.c src/draw.c src/math.c src/text.c src/texture.c \
    src/input/mouse_drawing.c src/input/input.c src/camera.c \
    src/video/video_stub.c \
    src/audio.c src/scene.c src/level.c src/button.c src/xml_loader.c \
    src/ui.c src/ui_xml.c
do
    o="$OUT/$(echo "$f" | tr '/' '_').o"
    if gcc $FLAGS -o "$o" "$f" 2>"$OUT/err.log"; then
        ok=$((ok+1))
    else
        echo "FAIL $f"
        cat "$OUT/err.log"
        exit 1
    fi
done
echo "compiled $ok files OK"