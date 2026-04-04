#include "simulate.h"

void DrawSimulatePanel(float width, float height) {
    UIDrawText("Application FPS: %d", (int)(1.0f / GetFrameTime()));
}

Panel GenerateSimulatePanel() {
    Panel p = { 0 };
	SetupPanel(&p, "Simulation");
	p.draw = DrawSimulatePanel;
	return p;
}
