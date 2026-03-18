#include "edit.h"
#include "renderer/renderer.h"
#include "renderer/overlay.h"
#include "renderer/rmath.h"
#include <easylogger.h>
#include <raymath.h>

typedef enum {
    EDIT_SINGLE_TRIANGLE,
} EditType;

size_t g_edit_item_index = 0;
BOOL g_item_selected = FALSE;
EditType g_edit_type = EDIT_SINGLE_TRIANGLE;

void SetEditTriangle(size_t index) {
    g_item_selected = TRUE;
    g_edit_item_index = index;
    g_edit_type = EDIT_SINGLE_TRIANGLE;
    SetSelectedVertex((TriangleID)-1);
}

void DeselectEditTarget() {
    g_item_selected = FALSE;
    SetSelectedTriangle((TriangleID)-1);
}

void DrawEditPanel(float width, float height) {
    if (g_item_selected) {
        if (g_edit_type == EDIT_SINGLE_TRIANGLE) {
            UIDrawText("FACE SELECTED!!");
        } else {
            EZ_FATAL("Unhandled edit type detected");
        }
    } else {
        UISetCursor((width - UITextWidth("No Selected Element"))/2.0f, height / 2.0f - 20);
        UIDrawText("No Selected Element");
    }

}

Panel GenerateEditPanel() {
	Panel p = { 0 };
	SetupPanel(&p, "Edit Selected");
	p.draw = DrawEditPanel;
	return p;
}
