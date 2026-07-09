#ifndef EDITOR_H
#define EDITOR_H

#include <stdlib.h>

void RunEditor();

void RunEditorHeadless(const char* load_path, const char* save_path, const char* image_path, int steps);

#endif
