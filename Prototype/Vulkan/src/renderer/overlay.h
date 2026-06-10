#ifndef OVERLAY_H
#define OVERLAY_H

#include "renderer/vulkan/vstructs.h"

typedef enum {
    NO_SELECT_MODE = 0,
    TRIANGLE_SELECT_MODE = 1,
    VERTEX_SELECT_MODE = 2,
    EDGE_SELECT_MODE = 3,
} OverlayMode;

void SelectNoneMode();

void SelectTriangleMode();

void SelectVertexMode();

void SelectEdgeMode();

void SetOverlayContext(Renderer* renderer);

void SetViewportRec(Rectangle rec);

Rectangle GetViewportRec();

TriangleID HoveredTriangle();

OverlaySSBO* ExposedOverlaySSBO();

void SetSelectedTriangle(TriangleID tid);

TriangleID GetSelectedTriangle();

OverlayMode GetOverlayMode();

VertexID HoveredVertex();

void SetSelectedVertex(VertexID vid);

VertexID GetSelectedVertex();

Edge HoveredEdge();

uint32_t HoveredEdgeID();

void SetSelectedEdge(Edge edge);

Edge GetSelectedEdge();

void SetSelectedEdgeID(uint32_t index);

uint32_t GetSelectedEdgeID();

#endif
