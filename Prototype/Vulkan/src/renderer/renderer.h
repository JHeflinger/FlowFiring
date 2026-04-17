#ifndef RENDERER_H
#define RENDERER_H

#include "renderer/rstructs.h"

PipelineFlags GetPipelineFlags();

void SetPipelineFlags(PipelineFlags flags);

void SetViewportSlice(size_t w, size_t h);

void OverrideResolution(size_t x, size_t y);

void InitializeRenderer();

void DestroyRenderer();

SimpleCamera GetCamera();

void MoveCamera(SimpleCamera camera);

void FitCamera();

void ReorientCamera();

void GetVertex(size_t index, vec3 out);

float* VertexReference(VertexID vertex);

void SubmitVertex(vec3 vertex);

void ClearVertices();

TriangleID SubmitTriangle(Triangle triangle);

void ClearTriangles();

void Render();

void Draw(float x, float y, float w, float h);

float RenderTime();

size_t NumVertices();

size_t NumTriangles();

void UpdateVertices();

void PrimeEdges();

Vector2 RenderResolution();

RendererConfig* RenderConfig();

Geometry* RendererGeometry();

float RenderFrameTime();

Triangle* TriangleReference(size_t index);

void UpdateTriangles();

EdgeMeta* EdgeReference(Edge e);

Edge EdgePrimed(Edge e);

void UpdateEdges();

void SaveRender(const char* filepath);

char* GPUModel();

void PollGPUCache(BOOL init);

size_t GPUHeapCount();

size_t GPUHeapUsage(size_t i);

size_t GPUHeapBudget(size_t i);

const char* GPUHeapType(size_t i);

void SubmitTOH(size_t width, size_t length, size_t height);

void RestartSimulation();

void SaveSimulation();

BOOL LoadSimulation();

void ClearSimulation();

#endif
