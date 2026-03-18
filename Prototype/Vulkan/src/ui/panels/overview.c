#include "overview.h"
#include "renderer/renderer.h"
#include "data/strings.h"
#include "ui/panels/edit.h"

void DrawOverviewPanel(float width, float height) {
    UIDrawText("Add To Scene...");
    UIMoveCursor(width - 45, -20);
    if (UIButton("+", 0)) {
        UIPopup(GenerateAddObjectPopup());
    }
    UIDivider(width - 20);
}

Panel GenerateOverviewPanel() {
	Panel p = { 0 };
    SetupPanel(&p, "Overview");
    p.draw = DrawOverviewPanel;
	return p;
}
