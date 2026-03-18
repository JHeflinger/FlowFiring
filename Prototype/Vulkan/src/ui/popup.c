#include "popup.h"
#include "ui/ui.h"
#include "data/colors.h"
#include "renderer/renderer.h"
#include <easymemory.h>
#include <easylogger.h>

uint32_t g_toh_w = 0;
uint32_t g_toh_l = 0;
uint32_t g_toh_h = 0;

int add_object_popup_stage_0(size_t x, size_t y, size_t w, size_t h) {
    g_toh_w = 5;
    g_toh_l = 5;
    g_toh_h = 5;
    float width = 350;
    float height = 180;
    float xpos = x + ((w - width) / 2.0f);
    float ypos = y + ((h - height) / 2.0f);
    float button_width = 300;
    UISetPosition(0, 0);
    UISetCursor(0, ypos + 10);
    DrawRectangle(xpos, ypos, width, height, MappedColor(PANEL_BG_COLOR));
    UIMoveCursor(xpos + (width / 2) - (UITextWidth("Add to Scene") / 2), 0);
    UIDrawText("Add to Scene");
    UIMoveCursor(xpos + (width / 2) - (button_width / 2) - 10, 20);
    if (UIButton("Tetrahedral-Octahedral Honeycomb", button_width)) return 0;
    UISetCursor(xpos + (width / 2) - (button_width / 2), ypos + height - 40);
    if (UIButton("Cancel", button_width)) return 2;
    return -1;
}

int add_toh(size_t x, size_t y, size_t w, size_t h) {
    float width = 385;
    float height = 200;
    float xpos = x + ((w - width) / 2.0f);
    float ypos = y + ((h - height) / 2.0f);
    float button_width = 200;
    UISetPosition(0, 0);
    UISetCursor(0, ypos + 10);
    DrawRectangle(xpos, ypos, width, height, MappedColor(PANEL_BG_COLOR));
    UIMoveCursor(xpos + (width / 2) - (UITextWidth("Add Tetrahedral-Octahedral Honeycomb") / 2), 0);
    UIDrawText("Add Tetrahedral-Octahedral Honeycomb");

    UIMoveCursor(0, 15);
    UIMoveCursor(xpos + (width / 2) - (UITextWidth("Dimensions") / 2) - 10, 0);
    UIDrawText("Dimensions");
    UIMoveCursor(xpos, 0);
    UIDrawText("w");
    UIMoveCursor(xpos + 15, -20);
    UIDragUInt(&g_toh_w, 0, 100, 1, 100);
    UIMoveCursor(xpos + 125, -20);
    UIDrawText("h");
    UIMoveCursor(xpos + 140, -20);
    UIDragUInt(&g_toh_h, 0, 100, 1, 100);
    UIMoveCursor(xpos + 250, -20);
    UIDrawText("l");
    UIMoveCursor(xpos + 265, -20);
    UIDragUInt(&g_toh_l, 0, 100, 1, 100);

    UISetCursor(xpos + (width / 2) - (button_width / 2), ypos + height - 70);
    if (UIButton("Submit", button_width)) {
        VertexID vstart = NumVertices();
        vec3 origin = { 0.0f - ((float)g_toh_w/2.0f)*1.0f, 0.0f - ((float)(g_toh_h + 1)/2.0f)*0.7f, 0.0f - ((float)g_toh_l/2.0f)*1.0f };
        for (size_t h = 0; h <= g_toh_h + 1; h++) {
            for (size_t l = 0; l <= g_toh_l; l++) {
                for (size_t w = 0; w <= g_toh_w; w++) {
                    size_t pyrlayers = (h - 1)/2;
                    size_t sqlayers = (h - 1)/2 + (h - 1)%2;
                    size_t lindex = sqlayers*(g_toh_w+1)*(g_toh_l+1) + pyrlayers*g_toh_w*g_toh_l + vstart;
                    if (h%2 == 0) {
                        vec3 v = { origin[0] + w*1.0f, origin[1] + h*0.7f, origin[2] + l*1.0f };
                        SubmitVertex(v);
                        if (h > 0 && w > 0 && l > 0) {
                            SubmitTriangle((Triangle){
                                NumVertices() - 1, NumVertices() - 2, lindex + g_toh_w*(l - 1) + w - 1,
                                (uint32_t)-1, (uint32_t)-1, (uint32_t)-1, 0 });
                            SubmitTriangle((Triangle){
                                NumVertices() - 1 - (g_toh_l + 1), NumVertices() - 2 - (g_toh_l + 1), lindex + g_toh_w*(l - 1) + w - 1,
                                (uint32_t)-1, (uint32_t)-1, (uint32_t)-1, 0 });
                            SubmitTriangle((Triangle){
                                NumVertices() - 1 - (g_toh_l + 1), NumVertices() - 1, lindex + g_toh_w*(l - 1) + w - 1,
                                (uint32_t)-1, (uint32_t)-1, (uint32_t)-1, 0 });
                            SubmitTriangle((Triangle){
                                NumVertices() - 2, NumVertices() - 2 - (g_toh_l + 1), lindex + g_toh_w*(l - 1) + w - 1,
                                (uint32_t)-1, (uint32_t)-1, (uint32_t)-1, 0 });
                        }
                        if (h > 0 && w > 0 && w < g_toh_w && l < g_toh_l) {
                            SubmitTriangle((Triangle){
                                NumVertices() - 1, lindex + g_toh_w*l + w, lindex + g_toh_w*l + w - 1,
                                (uint32_t)-1, (uint32_t)-1, (uint32_t)-1, 0 });
                        }
                        if (h > 0 && w > 0 && w < g_toh_w && l > 0) {
                            SubmitTriangle((Triangle){
                                NumVertices() - 1, lindex + g_toh_w*(l - 1) + w, lindex + g_toh_w*(l - 1) + w - 1,
                                (uint32_t)-1, (uint32_t)-1, (uint32_t)-1, 0 });
                        }
                        if (h > 0 && w < g_toh_w && l > 0 && l < g_toh_l) {
                            SubmitTriangle((Triangle){
                                NumVertices() - 1, lindex + g_toh_w*(l - 1) + w, lindex + g_toh_w*l + w,
                                (uint32_t)-1, (uint32_t)-1, (uint32_t)-1, 0 });
                        }
                        if (h > 0 && w > 0 && l > 0 && l < g_toh_l) {
                            SubmitTriangle((Triangle){
                                NumVertices() - 1, lindex + g_toh_w*(l - 1) + w - 1, lindex + g_toh_w*l + w - 1,
                                (uint32_t)-1, (uint32_t)-1, (uint32_t)-1, 0 });
                        }
                    } else {
                        if (w == g_toh_w || l == g_toh_l) continue;
                        vec3 v = { origin[0] + w*1.0f + 0.5f, origin[1] + h*0.7f, origin[2] + l*1.0f + 0.5f };
                        SubmitVertex(v);
                        SubmitTriangle((Triangle){
                            NumVertices() - 1, lindex + (g_toh_w + 1)*l + w, lindex + (g_toh_w + 1)*l + w + 1,
                            (uint32_t)-1, (uint32_t)-1, (uint32_t)-1, 0 });
                        SubmitTriangle((Triangle){
                            NumVertices() - 1, lindex + (g_toh_w + 1)*(l + 1) + w, lindex + (g_toh_w + 1)*(l + 1) + w + 1,
                            (uint32_t)-1, (uint32_t)-1, (uint32_t)-1, 0 });
                        SubmitTriangle((Triangle){
                            NumVertices() - 1, lindex + (g_toh_w + 1)*l + w, lindex + (g_toh_w + 1)*(l + 1) + w,
                            (uint32_t)-1, (uint32_t)-1, (uint32_t)-1, 0 });
                        SubmitTriangle((Triangle){
                            NumVertices() - 1, lindex + (g_toh_w + 1)*l + w + 1, lindex + (g_toh_w + 1)*(l + 1) + w + 1,
                            (uint32_t)-1, (uint32_t)-1, (uint32_t)-1, 0 });
                        if (w > 0) {
                            SubmitTriangle((Triangle){
                                NumVertices() - 1, NumVertices() - 2, lindex + (g_toh_w + 1)*l + w,
                                (uint32_t)-1, (uint32_t)-1, (uint32_t)-1, 0 });
                            SubmitTriangle((Triangle){
                                NumVertices() - 1, NumVertices() - 2, lindex + (g_toh_w + 1)*(l + 1) + w,
                                (uint32_t)-1, (uint32_t)-1, (uint32_t)-1, 0 });
                        }
                        if (l > 0) {
                            SubmitTriangle((Triangle){
                                NumVertices() - 1, NumVertices() - 1 - g_toh_l, lindex + (g_toh_w + 1)*l + w,
                                (uint32_t)-1, (uint32_t)-1, (uint32_t)-1, 0 });
                            SubmitTriangle((Triangle){
                                NumVertices() - 1, NumVertices() - 1 - g_toh_l, lindex + (g_toh_w + 1)*l + w + 1,
                                (uint32_t)-1, (uint32_t)-1, (uint32_t)-1, 0 });
                        }
                    }
                }
            }
        }
        return 0;
    }
    UISetCursor(xpos + (width / 2) - (button_width / 2), ypos + height - 40);
    if (UIButton("Cancel", button_width)) return 0;
    return -1;
}

Popup* GenerateEmptyPopup() {
    return EZ_ALLOC(1, sizeof(Popup));
}

void CleanPopup(Popup* popup) {
    if (popup->options != 0)
        for (size_t i = 0; i < popup->options; i++)
            CleanPopup(((Popup**)popup->results)[i]);
    if (popup->options > 0) EZ_FREE(popup->results);
    EZ_FREE(popup);
}

Popup* GenerateAddObjectPopup() {
    Popup* popup = GenerateEmptyPopup();
    popup->options = 1;
    popup->behavior = add_object_popup_stage_0;
    popup->results = EZ_ALLOC(popup->options, sizeof(Popup*));
    PopupFunction stage_1[] = {add_toh};
    for (size_t i = 0; i < popup->options; i++) {
        Popup* next = GenerateEmptyPopup();
        next->options = 0;
        next->behavior = stage_1[i];
        ((Popup**)popup->results)[i] = next;
    }
    return popup;
}
