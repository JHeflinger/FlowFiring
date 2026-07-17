#!/usr/bin/env bash
#
# Vendors Windows/MinGW64 dependencies into the paths .tinyconf expects:
#   platform/windows/vulkan/{include,libs}
#   platform/windows/GLFW/{include,<libglfw3.a lives here directly>}
#   vendor/raylib/lib/win_x64_libraylib.a
#
# Run this from an MSYS2 MINGW64 shell, from Prototype/Vulkan/, after installing:
#   pacman -S --needed \
#     mingw-w64-x86_64-gcc \
#     mingw-w64-x86_64-vulkan-headers \
#     mingw-w64-x86_64-vulkan-loader \
#     mingw-w64-x86_64-shaderc \
#     mingw-w64-x86_64-glfw \
#     mingw-w64-x86_64-raylib
#
set -e

MINGW_PREFIX="${MINGW_PREFIX:-/mingw64}"

if [ ! -d "$MINGW_PREFIX" ]; then
    echo "ERROR: $MINGW_PREFIX not found. Run this from an MSYS2 MINGW64 shell."
    exit 1
fi

echo "Vendoring Windows dependencies from $MINGW_PREFIX ..."

# ── Vulkan headers + loader import lib ───────────────────────────────────────
mkdir -p platform/windows/vulkan/include
mkdir -p platform/windows/vulkan/libs
rm -rf platform/windows/vulkan/include/vulkan
cp -R "$MINGW_PREFIX/include/vulkan" platform/windows/vulkan/include/
if [ -d "$MINGW_PREFIX/include/vk_video" ]; then
    rm -rf platform/windows/vulkan/include/vk_video
    cp -R "$MINGW_PREFIX/include/vk_video" platform/windows/vulkan/include/
fi
cp "$MINGW_PREFIX/lib/libvulkan-1.dll.a" platform/windows/vulkan/libs/

# ── GLFW headers + static lib ────────────────────────────────────────────────
mkdir -p platform/windows/GLFW/include
rm -rf platform/windows/GLFW/include/GLFW
cp -R "$MINGW_PREFIX/include/GLFW" platform/windows/GLFW/include/
cp "$MINGW_PREFIX/lib/libglfw3.a" platform/windows/GLFW/

# ── raylib static lib (shares vendor/raylib/lib with the Linux/macOS build) ──
mkdir -p vendor/raylib/lib
cp "$MINGW_PREFIX/lib/libraylib.a" vendor/raylib/lib/win_x64_libraylib.a

echo "Done. Vendored:"
echo "  platform/windows/vulkan/include, platform/windows/vulkan/libs/libvulkan-1.dll.a"
echo "  platform/windows/GLFW/include, platform/windows/GLFW/libglfw3.a"
echo "  vendor/raylib/lib/win_x64_libraylib.a"
