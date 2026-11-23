// No-op stand-ins for nGL drawing calls that host-compiled code references
// (AABB::render) but tests never execute.
#include "gl.h"

void nglAddVertex(const VERTEX *) {}
void glBegin(const GLDrawMode) {}

#include "terrain.h"
TEXTURE *terrain_current = nullptr;
TEXTURE *terrain_quad = nullptr;


// Texture loader tests link the production texture tools without the full
// renderer. The greyscale path is not exercised by these tests.
RGB rgbColor(const COLOR) { return RGB(); }
