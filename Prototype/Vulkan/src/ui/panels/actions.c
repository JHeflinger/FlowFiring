#include "actions.h"

void DrawActionsPanel(float width, float height) {
    UIDrawText("TODO");
}

Panel GenerateActionsPanel() {
	Panel p = { 0 };
	SetupPanel(&p, "Actions");
	p.draw = DrawActionsPanel;
	return p;
}
