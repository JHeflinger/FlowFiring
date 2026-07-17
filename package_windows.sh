#!/usr/bin/env bash

set -e # exit immediately on any error

APP_NAME="Flow"
BINARY_SRC="Prototype/Vulkan/build/bin.exe"
SHADER_SRC="Prototype/Vulkan/build/shaders"
ASSETS_SRC="Prototype/Vulkan/assets"
SHADERS_RAW_SRC="Prototype/Vulkan/shaders"

OUT_DIR="${APP_NAME}"
ZIP_NAME="Flow-Windows.zip"

# ── 1. Validate inputs ───────────────────────────────────────────────────────
if [ ! -f "$BINARY_SRC" ]; then
    echo "ERROR: $BINARY_SRC not found. Run ./publish_windows.sh first."
    exit 1
fi

# ── 2. Create directory structure ────────────────────────────────────────────
# Unlike macOS (where a .app launched from Finder gets $HOME as its working
# directory), a Windows .exe launched from Explorer uses its own folder as
# the working directory. So we can just lay out the relative paths the code
# expects ("assets/...", "shaders/...", "build/shaders/...") right next to
# the executable - no launcher wrapper needed.
echo "Creating ${OUT_DIR}/ layout..."
rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"

# ── 3. Copy the binary ───────────────────────────────────────────────────────
cp "$BINARY_SRC" "${OUT_DIR}/${APP_NAME}.exe"

# ── 4. Copy runtime assets ───────────────────────────────────────────────────
mkdir -p "${OUT_DIR}/build/shaders"
if [ -d "$SHADER_SRC" ]; then
    cp -R "$SHADER_SRC"/. "${OUT_DIR}/build/shaders/"
fi
mkdir -p "${OUT_DIR}/shaders"
if [ -d "$SHADERS_RAW_SRC" ]; then
    cp -R "$SHADERS_RAW_SRC"/. "${OUT_DIR}/shaders/"
fi
if [ -d "$ASSETS_SRC" ]; then
    cp -r "$ASSETS_SRC" "${OUT_DIR}/assets"
fi

# ── 5. Bundle MinGW runtime DLLs ─────────────────────────────────────────────
# The exe is built with MinGW64 gcc and dynamically links against a few
# runtime DLLs (e.g. libwinpthread-1.dll, libgcc_s_seh-1.dll) as well as
# glfw3.dll (raylib was built against shared GLFW - see vendor_windows.sh).
# Users without MSYS2/MinGW installed won't have these on PATH, so copy
# whatever the binary actually needs from the MinGW prefix next to the exe.
if command -v ldd >/dev/null 2>&1; then
    echo "Copying required MinGW runtime DLLs..."
    ldd "${OUT_DIR}/${APP_NAME}.exe" 2>/dev/null |
        awk '{print $3}' |
        grep -iE '/mingw64/' |
        while read -r dll; do
            [ -f "$dll" ] && cp -n "$dll" "${OUT_DIR}/" || true
        done
fi

# ── 6. Zip for distribution ──────────────────────────────────────────────────
echo "Zipping ${OUT_DIR}..."
rm -f "$ZIP_NAME"
if command -v zip >/dev/null 2>&1; then
    zip -r "$ZIP_NAME" "$OUT_DIR" >/dev/null
else
    powershell -NoProfile -Command "Compress-Archive -Path '${OUT_DIR}' -DestinationPath '${ZIP_NAME}' -Force"
fi

echo ""
echo "Done! ${ZIP_NAME} is ready for distribution."
echo ""
echo "To test locally:"
echo "  unzip ${ZIP_NAME} && ./${OUT_DIR}/${APP_NAME}.exe"
echo ""
echo "NOTE: The exe is unsigned. Windows SmartScreen may warn on first launch;"
echo "users can bypass this via 'More info' -> 'Run anyway'."
