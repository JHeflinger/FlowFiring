#ifndef EDIT_H
#define EDIT_H

#include "ui/ui.h"
#include "renderer/rstructs.h"

void SetEditTriangle(size_t index);

void SetEditEdge(Edge edge);

void DeselectEditTarget();

Panel GenerateEditPanel();

#endif
