#include "simulate.h"
#include "renderer/renderer.h"

uint32_t g_simulate_steps = 0;
uint32_t g_simstepsize = 1;
BOOL g_simulation_running = FALSE;
BOOL g_simulation_started = FALSE;

void DrawSimulatePanel(float width, float height) {
    UIDrawText("Simulation Controls");
    UIDivider(width - 20);
    if (UIButton(g_simulation_started ? (g_simulation_running ? "Pause" : "Resume") : "Start", width - 20)) {
        if (g_simulation_running) g_simulation_running = FALSE;
        else g_simulation_running = TRUE;
        g_simulation_started = TRUE;
    }
    if (UIButton("Stop", width - 20)) {
        g_simulate_steps = (uint32_t)-1;
        g_simulation_running = FALSE;
        g_simulation_started = FALSE;
        RestartSimulation();
    }
    if (UIButton("Step", (width - 20.0f)/2.0f)) {
        g_simulation_running = TRUE;
        g_simulate_steps = g_simstepsize;
    }
    UIMoveCursor((width - 20.0f)/2.0f, -20);
    UIDragUInt(&g_simstepsize, 1, 10000, 1, (width - 20.0f)/2.0f);
    UIMoveCursor(0, 35);
    RenderConfig()->simulate = FALSE;
    if (g_simulation_running) {
        RenderConfig()->simulate = TRUE;
        if (g_simulate_steps != (uint32_t)-1) {
            g_simulate_steps--;
            if (g_simulate_steps == 0) {
                g_simulation_running = FALSE;
                g_simulate_steps = (uint32_t)-1;
            }
        }
    }
}

Panel GenerateSimulatePanel() {
    Panel p = { 0 };
	SetupPanel(&p, "Simulation");
	p.draw = DrawSimulatePanel;
	return p;
}
