#!/usr/bin/env bash

set -e # exit immediately on any error

APP_NAME="Flow"
BUNDLE_ID="com.yourname.flow"
MIN_MACOS="12.0"
BINARY_SRC="Prototype/Vulkan/build/bin.exe"
SHADER_SRC="Prototype/Vulkan/build/shaders"
ASSETS_SRC="Prototype/Vulkan/assets"
SHADERS_RAW_SRC="Prototype/Vulkan/shaders"

APP_DIR="${APP_NAME}.app"
CONTENTS="${APP_DIR}/Contents"
MACOS_DIR="${CONTENTS}/MacOS"
RESOURCES_DIR="${CONTENTS}/Resources"

# ── 1. Validate inputs ───────────────────────────────────────────────────────
if [ ! -f "$BINARY_SRC" ]; then
    echo "ERROR: $BINARY_SRC not found. Run ./build.sh -p first."
    exit 1
fi

# ── 2. Create directory structure ────────────────────────────────────────────
echo "Creating .app structure..."
rm -rf "$APP_DIR"
mkdir -p "$MACOS_DIR"
mkdir -p "$RESOURCES_DIR"

# ── 3. Copy the binary ───────────────────────────────────────────────────────
cp "$BINARY_SRC" "${MACOS_DIR}/${APP_NAME}_bin"
chmod +x "${MACOS_DIR}/${APP_NAME}_bin"

# ── 4. Copy runtime assets into Resources ────────────────────────────────────
mkdir -p "${RESOURCES_DIR}/shaders"
if [ -d "$SHADER_SRC" ]; then
    cp -R "$SHADER_SRC"/. "${RESOURCES_DIR}/shaders/"
fi
if [ -d "$SHADERS_RAW_SRC" ]; then
    cp -R "$SHADERS_RAW_SRC"/. "${RESOURCES_DIR}/shaders/"
fi
if [ -d "$ASSETS_SRC" ]; then
    cp -r "$ASSETS_SRC" "${RESOURCES_DIR}/assets"
fi

# ── 5. Launcher script ───────────────────────────────────────────────────────
# macOS sets the working directory to $HOME when launching a .app from Finder.
# Your code loads assets from relative paths like "assets/fonts/..." and
# "build/shaders/...".  This launcher script:
#   a) changes into the Resources directory (where we placed everything), and
#   b) creates the "build/shaders" path your code expects
# so that all relative paths resolve correctly — no code changes needed.
cat >"${MACOS_DIR}/${APP_NAME}" <<'EOF'
#!/usr/bin/env bash
# Resolve the true Resources dir regardless of where the .app lives
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
RESOURCES_DIR="$(cd "${SCRIPT_DIR}/../Resources" && pwd)"

cd "$RESOURCES_DIR"

# Your code reads shaders from "build/shaders/<name>.spv"
# Recreate that path here as a symlink so nothing in the binary needs changing
mkdir -p build
if [ ! -L "build/shaders" ]; then
    ln -sf "${RESOURCES_DIR}/shaders" build/shaders
fi

# Launch the actual binary
exec "${SCRIPT_DIR}/Flow_bin" "$@"
EOF
chmod +x "${MACOS_DIR}/${APP_NAME}"

# ── 6. Write Info.plist ──────────────────────────────────────────────────────
cat >"${CONTENTS}/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
    "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <!-- The name shown in the Dock and menu bar -->
    <key>CFBundleName</key>
    <string>${APP_NAME}</string>

    <!-- Reverse-DNS bundle identifier — must be unique on the system -->
    <key>CFBundleIdentifier</key>
    <string>${BUNDLE_ID}</string>

    <!-- Which executable inside MacOS/ to run (our launcher script) -->
    <key>CFBundleExecutable</key>
    <string>${APP_NAME}</string>

    <!-- Shown in About This Mac / Finder Get Info -->
    <key>CFBundleVersion</key>
    <string>1.0.0</string>
    <key>CFBundleShortVersionString</key>
    <string>1.0</string>

    <!-- Tells macOS this is a standard GUI application -->
    <key>CFBundlePackageType</key>
    <string>APPL</string>

    <!-- Minimum macOS version required -->
    <key>LSMinimumSystemVersion</key>
    <string>${MIN_MACOS}</string>

    <!-- Suppress "file has no document types" warning in console -->
    <key>LSUIElement</key>
    <false/>

    <!-- High-resolution (Retina) display support -->
    <key>NSHighResolutionCapable</key>
    <true/>

    <!-- Needed for any app that opens windows (suppress focus warnings) -->
    <key>NSPrincipalClass</key>
    <string>NSApplication</string>
</dict>
</plist>
EOF

# ── 7. (Optional) Copy icon ──────────────────────────────────────────────────
# If you have an icon file, convert it to .icns and drop it next to this script,
# then uncomment the block below.  Tools to create .icns:
#   - https://cloudconvert.com/png-to-icns
#   - iconutil (built into macOS): iconutil -c icns MyIcon.iconset
#
# if [ -f "AppIcon.icns" ]; then
#     cp AppIcon.icns "${RESOURCES_DIR}/AppIcon.icns"
#     # Also register it in Info.plist (add before the closing </dict>):
#     # <key>CFBundleIconFile</key><string>AppIcon</string>
# fi

codesign --force --deep --sign - "${APP_DIR}"

# ── 8. Zip for distribution ──────────────────────────────────────────────────
echo "Zipping ${APP_DIR}..."
zip -r --symlinks "Flow.zip" "$APP_DIR"

echo ""
echo "Done! Flow.zip is ready for distribution."
echo ""
echo "To test locally:"
echo "  unzip Flow.zip && open Flow.app"
echo ""
echo "NOTE: On first launch, Gatekeeper will block the app because it is not"
echo "code-signed. Users can bypass this by right-clicking the .app and"
echo "choosing 'Open', then confirming in the dialog."
