#include "simulate.h"
#include "renderer/renderer.h"

static uint32_t g_simulate_steps = 0;
static uint32_t g_simstepsize = 1;
static BOOL g_simulation_running = FALSE;
static BOOL g_simulation_started = FALSE;
static char* g_viewmode_labels[] = { "Free Perspective", "Free Orthographic", "Cubic", "Ramped", "Corners" };
static char* g_geomode_labels[] = { "Faces", "Edges" };
static char* g_edgemode_labels[] = { "Static", "Colored", "Directional", "Flow" };
static char* g_cutaxis_labels[] = { "None", "X <", "X >", "Y <", "Y >", "Z <", "Z >" };

static size_t DropdownSelectViewmode(void* data, size_t index) {
    if (index == (size_t)-1) {
        return RenderConfig()->viewmode;
    } else {
        RenderConfig()->viewmode = index;
    }
    return index;
}

static size_t DropdownSelectGeomode(void* data, size_t index) {
    if (index == (size_t)-1) {
        return RenderConfig()->geomode;
    } else {
        RenderConfig()->geomode = index;
    }
    return index;
}

static size_t DropdownSelectEdgemode(void* data, size_t index) {
    if (index == (size_t)-1) {
        return RenderConfig()->edgemode;
    } else {
        RenderConfig()->edgemode = index;
    }
    return index;
}

static size_t DropdownSelectCutAxis(void* data, size_t index) {
    if (index == (size_t)-1) {
        return RenderConfig()->cut_axis;
    } else {
        RenderConfig()->cut_axis = (uint32_t)index;
    }
    return index;
}

static void DrawSimulatePanel(float width, float height) {
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
    if (RenderConfig()->geomode != 0) DisableUI();
    UIDrawText("Hide Solid Faces");
    UIMoveCursor((width - 20.0f)/2.0f - 2, -20);
    UICheckbox(&RenderConfig()->hide_solid);
    EnableUI();
    UIDrawText("Cross Section");
    UIMoveCursor((width - 20.0f)/2.0f, -20);
    UIDragFloat(&(RenderConfig()->depth), 0.0f, FLT_MAX, 0.01f, (width - 20.0f)/2.0f);
    UIDrawText("Cut Axis");
    UIMoveCursor((width - 20.0f)/2.0f, -20);
    UIDropdownMenu((width - 20.0f)/2.0f, 7, g_cutaxis_labels, DropdownSelectCutAxis, NULL);
    if (RenderConfig()->cut_axis == 0) DisableUI();
    UIDrawText("Cut Position");
    UIMoveCursor((width - 20.0f)/2.0f, -20);
    UIDragFloat(&(RenderConfig()->cut_offset), -FLT_MAX, FLT_MAX, 0.01f, (width - 20.0f)/2.0f);
    EnableUI();
}

Panel GenerateSimulatePanel() {
    Panel p = { 0 };
	SetupPanel(&p, "Simulation");
	p.draw = DrawSimulatePanel;
	return p;
}
