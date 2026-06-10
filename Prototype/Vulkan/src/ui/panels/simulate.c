#include "simulate.h"
#include "renderer/renderer.h"
#include "renderer/overlay.h"

uint32_t g_simulate_steps = 0;
uint32_t g_simstepsize = 1;
BOOL g_simulation_running = FALSE;
BOOL g_simulation_started = FALSE;
char* g_viewmode_labels[] = { "Free Perspective", "Free Orthographic", "Cubic", "Ramped", "Corners" };
char* g_geomode_labels[] = { "Faces", "Edges" };
char* g_edgemode_labels[] = { "Static", "Colored", "Directional", "Flow" };
char* g_crossmode_labels[] = { "Dynamic", "Snap" };

void StartSimulation() {
    g_simulation_running = TRUE;
    g_simulation_started = TRUE;
}

void PauseSimulation() {
    g_simulation_running = FALSE;
    g_simulation_started = TRUE;
}

void ResumeSimulation() {
    g_simulation_running = TRUE;
    g_simulation_started = TRUE;
}

void StopSimulation() {
    g_simulate_steps = (uint32_t)-1;
    g_simulation_running = FALSE;
    g_simulation_started = FALSE;
    RestartSimulation();
    DisableReplay();
    ClearReplay();
}

void ReplaySimulation() {
    EnableReplay();
    RestartSimulation();
    g_simulation_running = TRUE;
    g_simulation_started = TRUE;
}

size_t DropdownSelectViewmode(void* data, size_t index) {
    if (index == (size_t)-1) {
        return RenderConfig()->viewmode;
    } else {
        RenderConfig()->viewmode = index;
    }
    return index;
}

size_t DropdownSelectGeomode(void* data, size_t index) {
    if (index == (size_t)-1) {
        return RenderConfig()->geomode;
    } else {
        RenderConfig()->geomode = index;
        if (RenderConfig()->geomode == 1 && GetOverlayMode() == TRIANGLE_SELECT_MODE) {
            SelectEdgeMode();
        }
    }
    return index;
}

size_t DropdownSelectEdgemode(void* data, size_t index) {
    if (index == (size_t)-1) {
        return RenderConfig()->edgemode;
    } else {
        RenderConfig()->edgemode = index;
    }
    return index;
}

size_t DropdownSelectCrossmode(void* data, size_t index) {
    if (index == (size_t)-1) {
        return RenderConfig()->crossmode;
    } else {
        RenderConfig()->crossmode = index;
    }
    return index;
}

void DrawSimulatePanel(float width, float height) {
    UIDrawText("Simulation Controls");
    UIDivider(width - 20);
    if (UIButton(g_simulation_started ? (g_simulation_running ? "Pause" : "Resume") : "Start", width - 20)) {
        if (g_simulation_running) g_simulation_running = FALSE;
        else g_simulation_running = TRUE;
        g_simulation_started = TRUE;
    }
    if (UIButton("Stop", width - 20)) {
        StopSimulation();
    }
    if (UIButton("Step", (width - 20.0f)/2.0f)) {
        g_simulation_running = TRUE;
        g_simulate_steps = g_simstepsize;
    }
    UIMoveCursor((width - 20.0f)/2.0f, -20);
    UIDragUInt(&g_simstepsize, 1, 10000, 1, (width - 20.0f)/2.0f);
    if (ReplaySize() == 0 || g_simulation_running) DisableUI();
    if (UIButton("Replay", width - 20)) {
        ReplaySimulation();
    }
    EnableUI();
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
    UIDrawText("Simulation View");
    UIDivider(width - 20);
    UIDrawText("Reference Grid");
    UIMoveCursor((width - 20.0f)/2.0f - 2, -20);
	UICheckbox(&RenderConfig()->grid);
    UIDrawText("Camera Mode");
    UIMoveCursor((width - 20.0f)/2.0f, -20);
    UIDropdownMenu((width - 20.0f)/2.0f, 5, g_viewmode_labels, DropdownSelectViewmode, NULL);
    UIDrawText("Geometry Mode");
    UIMoveCursor((width - 20.0f)/2.0f, -20);
    UIDropdownMenu((width - 20.0f)/2.0f, 2, g_geomode_labels, DropdownSelectGeomode, NULL);
    if (RenderConfig()->geomode != 1) DisableUI();
    UIDrawText("Edge Mode");
    UIMoveCursor((width - 20.0f)/2.0f, -20);
    UIDropdownMenu((width - 20.0f)/2.0f, 4, g_edgemode_labels, DropdownSelectEdgemode, NULL);
    EnableUI();
    UIDrawText("Cross Section Mode");
    UIMoveCursor((width - 20.0f)/2.0f, -20);
    UIDropdownMenu((width - 20.0f)/2.0f, 2, g_crossmode_labels, DropdownSelectCrossmode, NULL);
    UIDrawText("Cross Section");
    UIMoveCursor((width - 20.0f)/2.0f, -20);
    if (RenderConfig()->crossmode == 0) {
        UIDragFloat(&(RenderConfig()->depth), 0.0f, FLT_MAX, 0.01f, (width - 20.0f)/2.0f);
    } else {
        UIDragUInt(&(RenderConfig()->snap), 0, 9999999, 1, (width - 20.0f)/2.0f);
    }
}

Panel GenerateSimulatePanel() {
    Panel p = { 0 };
	SetupPanel(&p, "Simulation");
	p.draw = DrawSimulatePanel;
	return p;
}
