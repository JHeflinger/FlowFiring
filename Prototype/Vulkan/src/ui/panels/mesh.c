#include "mesh.h"
#include "renderer/renderer.h"

void DrawMeshPanel(float width, float height) {
}

Panel GenerateMeshPanel() {
	Panel p = { 0 };
	SetupPanel(&p, "Mesh");
	p.draw = DrawMeshPanel;
	return p;
}
