// No-op stand-ins for nGL drawing calls that host-compiled code references
// (AABB::render) but tests never execute.
#include "gl.h"

void nglAddVertex(const VERTEX *) {}
void glBegin(const GLDrawMode) {}

#include "terrain.h"
TEXTURE *terrain_current = nullptr;
TEXTURE *terrain_quad = nullptr;


// Texture loader tests link the production texture tools without the full
// renderer. Keep these color conversion helpers identical to nGL/gl.cpp so
// texture conversion tests exercise real RGB565 channel packing.
RGB rgbColor(const COLOR c)
{
    const GLFix r = (c >> 11) & 0b11111;
    const GLFix g = (c >> 5) & 0b111111;
    const GLFix b = (c >> 0) & 0b11111;
    return {r / GLFix(0b11111), g / GLFix(0b111111), b / GLFix(0b11111)};
}

COLOR colorRGB(const GLFix r, const GLFix g, const GLFix b)
{
    const int r1 = (r * GLFix(0b11111)).round();
    const int g1 = (g * GLFix(0b111111)).round();
    const int b1 = (b * GLFix(0b11111)).round();
    return ((r1 & 0b11111) << 11) | ((g1 & 0b111111) << 5) | (b1 & 0b11111);
}

COLOR colorRGB(const RGB rgb)
{
    return colorRGB(rgb.r, rgb.g, rgb.b);
}
