#!/usr/bin/env bash

# create all build directories if it does not exist
if [ ! -d "build" ]; then
    mkdir "build"
fi
cd build
if [ ! -d "shaders" ]; then
    mkdir "shaders"
fi
if [ ! -d "cache" ]; then
    mkdir "cache"
fi
cd cache
if [ ! -d "shaders" ]; then
    mkdir "shaders"
fi
cd ..
cd ..

# detect platform
PLATFORM=$(uname -s)

# macOS: expose Vulkan SDK paths to compiler
if [ "$PLATFORM" == "Darwin" ] && [ -n "$VULKAN_SDK" ]; then
    export CPATH="${VULKAN_SDK}/include:${CPATH:-}"
    export LIBRARY_PATH="${VULKAN_SDK}/lib:${LIBRARY_PATH:-}"
fi

# compile shaders
echo "Compiling shaders..."
startTime=$(date +%s)
SHADERS_UP_TO_DATE="true"
while IFS= read -r file; do
    filename=$(basename "$file")
    if [ ! -f "build/cache/shaders/$filename" ]; then
        SHADERS_UP_TO_DATE="false"
        echo -e "- [$filename] \033[33m(compiling...)\033[0m"
        glslc $file -o "build/shaders/$filename.spv"
        if [ $? -ne 0 ]; then
            echo -e "Building shader \033[31mfailed\033[0m"
            exit 1
        fi
        echo -e "\033[1A\033[0K- [$filename] \033[32mOK\033[0m"
        cp $file "build/cache/shaders/$filename"
    else
        if ! cmp -s $file "build/cache/shaders/$filename"; then
            SHADERS_UP_TO_DATE="false"
            echo -e "- [$filename] \033[33m(compiling...)\033[0m"
            glslc $file -o "build/shaders/$filename.spv"
            if [ $? -ne 0 ]; then
                echo -e "Building shader \033[31mfailed\033[0m"
                exit 1
            fi
            echo -e "\033[1A\033[0K- [$filename] \033[32mOK\033[0m"
            cp $file "build/cache/shaders/$filename"
        fi
    fi
done < <(find "shaders" -type f \( -name "*.vert" -o -name "*.frag" -o -name "*.comp" \))
endTime=$(date +%s)
elapsed=$((endTime - startTime))
if [ "$SHADERS_UP_TO_DATE" == "true" ]; then
    echo -e "\033[1A\033[0KShaders are currently \033[32mup to date\033[0m"
else
    echo -e "\033[32mFinished\033[0m building shaders in ${elapsed}s"
fi

# for mac - compile builder instead
if [ "$PLATFORM" = "Darwin" ] && [ ! -f "build/tiny_macos.bin" ]; then
    cd build
    curl -L -s -o "tiny.c" "https://raw.githubusercontent.com/JHeflinger/tiny/refs/heads/main/tiny.c"
    gcc -Wall -O2 tiny.c -o tiny_macos.bin -pthread
    ./tiny_macos.bin -v
    rm tiny.c
    cd ..
fi

ls /opt/homebrew/opt/glfw/lib

# download builder
if [ "$PLATFORM" = "Darwin" ]; then
    URL="https://github.com/JHeflinger/tiny/raw/refs/heads/main/bin/tiny_macos.bin"
    OUT="tiny_macos.bin"
else
    URL="https://github.com/JHeflinger/tiny/raw/refs/heads/main/bin/tiny_linux.bin"
    OUT="tiny_linux.bin"
fi
if [ ! -f "build/$OUT" ] || [ "$1" == "-u" ] || [ "$2" == "-u" ]; then
    if [ -f "build/$OUT" ]; then
        echo "Updating tiny builder..."
        rm "build/$OUT"
    else
        echo "Downloading tiny builder..."
    fi
    cd build
    curl -L -s -o "$OUT" "$URL"
    chmod +x "$OUT"
    cd ..
fi

# run builder
PROD=""
if [ "$1" == "-p" ] || [ "$2" == "-p" ] || [ "$3" == "-p" ]; then
    PROD="-prod"
fi
./build/$OUT $PROD
if [ $? -ne 0 ]; then
    exit 1
fi
