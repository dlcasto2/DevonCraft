#include <cassert>

#include "gldrawarray.h"

/* Create a vertex out of a VECTOR3 and IndexedVertex */
#define MAKE_VERTEX(vec, iver) { (vec).x, (vec).y, (vec).z, (iver).u, (iver).v, (iver).c }

/* Apply perspective to a ProcessedVertex
 * and return the resulting VERTEX.
 * Caches the result in the ProcessedVertex. */
static VERTEX perspective(const IndexedVertex &v, ProcessedPosition &p)
{
    if(!p.perspective_available)
    {
        p.perspective = p.transformed;
        nglPerspective(&p.perspective);
        p.perspective_available = true;
    }

    VERTEX result = MAKE_VERTEX(p.perspective, v);
    // p.perspective caches only the projected position. Without these values,
    // the rasterizer divides zero UV/depth attributes and samples atlas (0, 0)
    // for every aligned chunk vertex, making all block textures appear black.
    nglSetPerspectiveTextureAttributes(result, nglGetNearPlane());
    return result;
}

static bool drawTriangle(ProcessedPosition *processed, const IndexedVertex &low, const IndexedVertex &middle, const IndexedVertex &high, bool backface_culling)
{
    ProcessedPosition &p_low = processed[low.index], &p_middle = processed[middle.index], &p_high = processed[high.index];

    const bool all_in_front =
        p_low.transformed.z >= GLFix(CLIP_PLANE)
        && p_middle.transformed.z >= GLFix(CLIP_PLANE)
        && p_high.transformed.z >= GLFix(CLIP_PLANE);

    if(all_in_front)
    {
        // Preserve the cached perspective path for the normal case where no
        // vertex approaches or crosses the near plane.
        VERTEX a = perspective(low, p_low);
        VERTEX b = perspective(middle, p_middle);
        VERTEX c = perspective(high, p_high);

        if(backface_culling && nglIsBackface(&a, &b, &c))
            return false;

        nglDrawTriangleZClipped(&a, &b, &c);
        return true;
    }

#ifndef Z_CLIPPING
    // Keep the legacy opt-out behavior when near-plane clipping is disabled.
    return true;
#else
    const VERTEX source[3] = {
        MAKE_VERTEX(p_low.transformed, low),
        MAKE_VERTEX(p_middle.transformed, middle),
        MAKE_VERTEX(p_high.transformed, high)
    };
    VERTEX clipped[4];
    const unsigned int clipped_count =
        nglClipTriangleToNearPlane(source, clipped, GLFix(CLIP_PLANE));

    if(clipped_count < 3)
        return true;

    // New intersection vertices and the surviving original vertices are all
    // projected only after the camera-space polygon has been clipped.
    for(unsigned int i = 0; i < clipped_count; ++i)
        nglPerspective(&clipped[i]);

    // Clipping preserves polygon winding. Cull clipped triangles and quads
    // consistently before fan-triangulating the visible polygon.
    if(backface_culling
       && nglIsBackface(&clipped[0], &clipped[1], &clipped[2]))
        return false;

    for(unsigned int i = 1; i + 1 < clipped_count; ++i)
        nglDrawTriangleZClipped(&clipped[0], &clipped[i], &clipped[i + 1]);

    return true;
#endif
}

void nglDrawArray(const IndexedVertex *vertices, const unsigned int count_vertices, const VECTOR3 *positions, const unsigned int count_positions, ProcessedPosition *processed, const GLDrawMode draw_mode, const bool reset_processed)
{
    if(reset_processed)
    {
        // Reset processed vertices and apply transformation
        for(unsigned int i = 0; i < count_positions; ++i)
        {
            processed[i].perspective_available = false;
            nglMultMatVectRes(transformation, &positions[i], &processed[i].transformed);
        }
    }

    // Draw
    if(draw_mode == GL_TRIANGLES)
    {
        for(unsigned int i = 0; i < count_vertices; i += 3)
            drawTriangle(processed, vertices[i], vertices[i + 1], vertices[i + 2], !nglGetTexture() || (vertices[i].c & TEXTURE_DRAW_BACKFACE) != TEXTURE_DRAW_BACKFACE);
    }
    else if(draw_mode == GL_QUADS)
    {
        for(unsigned int i = 0; i < count_vertices; i += 4)
        {
            // Either none or both parts of a quad face the camera
            if(drawTriangle(processed, vertices[i], vertices[i + 1], vertices[i + 2], !nglGetTexture() || (vertices[i].c & TEXTURE_DRAW_BACKFACE) != TEXTURE_DRAW_BACKFACE))
                drawTriangle(processed, vertices[i + 2], vertices[i + 3], vertices[i], false);
        }
    }
    else
        assert(!"Not implemented");
}

