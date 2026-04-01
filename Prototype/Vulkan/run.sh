#!/usr/bin/env bash
# MoltenVK setup for macOS
if [ "$(uname -s)" == "Darwin" ] && [ -n "$VULKAN_SDK" ]; then
    export VK_ICD_FILENAMES="$VULKAN_SDK/share/vulkan/icd.d/MoltenVK_icd.json"
    export VK_LAYER_PATH="$VULKAN_SDK/share/vulkan/explicit_layer.d"
fi

./build.sh $1 $2 $3
if [ $? -ne 0 ]; then
    exit 1
fi
#./build/bin.exe ~/Dev/ADVGRAPHICS/example-scenes/CornellBox-Sphere.xml out.png 512 512 256 -1.0 true false
#./build/bin.exe ~/Dev/MESH/meshes/sphere.obj out.obj simplify 955
#./build/bin.exe ~/Dev/MESH/template_inis/final/simplify_cow.ini
./build/bin.exe
