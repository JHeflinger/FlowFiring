#include "edit.h"
#include "renderer/renderer.h"
#include "renderer/overlay.h"
#include "renderer/rmath.h"
#include <easylogger.h>
#include <raymath.h>

typedef enum {
    EDIT_SINGLE_TRIANGLE,
    EDIT_SINGLE_EDGE
} EditType;

size_t g_edit_item_index = 0;
size_t g_edit_item_index_2 = 0;
BOOL g_item_selected = FALSE;
EditType g_edit_type = EDIT_SINGLE_TRIANGLE;

void SetEditTriangle(size_t index) {
    g_item_selected = TRUE;
    g_edit_item_index = index;
    g_edit_type = EDIT_SINGLE_TRIANGLE;
    SetSelectedVertex((VertexID)-1);
    SetSelectedEdge((Edge){ (VertexID)-1, (VertexID)-1 });
    SetSelectedTriangle(index);
}

void SetEditEdge(Edge edge) {
    g_item_selected = TRUE;
    g_edit_item_index = edge.a;
    g_edit_item_index_2 = edge.b;
    g_edit_type = EDIT_SINGLE_EDGE;
    SetSelectedVertex((VertexID)-1);
    SetSelectedTriangle((TriangleID)-1);
    SetSelectedEdge(edge);
}

void DeselectEditTarget() {
    g_item_selected = FALSE;
    SetSelectedTriangle((TriangleID)-1);
    SetSelectedVertex((VertexID)-1);
    SetSelectedEdge((Edge){ (VertexID)-1, (VertexID)-1 });
}

void DrawEditPanel(float width, float height) {
    float sboxwidth = width - 20 - 140;
    BOOL changed = FALSE;
    if (g_item_selected) {
        if (g_edit_type == EDIT_SINGLE_TRIANGLE) {
            Triangle* ref = TriangleReference(g_edit_item_index);
            UIMoveCursor((width - 20 - UITextWidth("Edit Face")) / 2.0f, 0);
            UIDrawText("Edit Face");
            UIDivider(width - 20);
            UIMoveCursor(0, 10);
            UIDrawText("Toplogical Hole");
            UIMoveCursor(140, -20);
            BOOL hole = ref->hole != 0;
            UICheckbox(&hole);
            changed |= hole != (ref->hole != 0);
            ref->hole = hole;
            if (changed) UpdateTriangles();
        } else if (g_edit_type == EDIT_SINGLE_EDGE) {
            EdgeMeta* ref = EdgeReference((Edge){ g_edit_item_index, g_edit_item_index_2 });
            UIMoveCursor((width - 20 - UITextWidth("Edit Edge")) / 2.0f, 0);
            UIDrawText("Edit Edge");
            UIDivider(width - 20);
            UIMoveCursor(0, 10);
            UIDrawText("Flow");
            UIMoveCursor(140, -20);
            changed |= UIDragInt(&(ref->flow), INT32_MIN, INT32_MAX, 1, sboxwidth);
            if (changed) UpdateEdges();
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
