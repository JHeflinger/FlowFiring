#include "core/editor.h"
#include <easylogger.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char** argv) {
    int headless = 0;
    const char* load_path = NULL;
    const char* save_path = NULL;
    const char* image_path = NULL;
    int steps = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--headless") == 0) headless = 1;
        else if (strcmp(argv[i], "--load") == 0 && i + 1 < argc) load_path = argv[++i];
        else if (strcmp(argv[i], "--save") == 0 && i + 1 < argc) save_path = argv[++i];
        else if (strcmp(argv[i], "--image") == 0 && i + 1 < argc) image_path = argv[++i];
        else if (strcmp(argv[i], "--steps") == 0 && i + 1 < argc) steps = atoi(argv[++i]);
    }
    if (headless) {
        RunEditorHeadless(load_path, save_path, image_path, steps);
    } else {
        RunEditor();
    }
    EZ_INFO("See you, Space Cowboy");
    return 0;
}
