#include <cassert>

#include "gldrawarray.h"

/* Create a vertex out of a VECTOR3 and IndexedVertex */
#define MAKE_VERTEX(vec, iver) { (vec).x, (vec).y, (vec).z, (iver).u, (iver).v, (iver).c }

/* Project once per shared position and cache reciprocal depth too.
 * UV-over-depth still depends on the face's UV coordinates, but tex_q does not. */
static void projectPosition(ProcessedPosition &p)
{
    if(p.perspective_available)
        return;

    p.perspective = p.transformed;
    nglPerspective(&p.perspective);
    p.tex_q = perspectiveReciprocalDepth(p.transformed.z, nglGetNearPlane());
    p.perspective_available = true;
}

static void attachTexture(VERTEX &vertex, const ProcessedPosition &p)
{
    vertex.tex_q = p.tex_q;
    vertex.u_over_z = perspectiveProduct(vertex.u, p.tex_q);
    vertex.v_over_z = perspectiveProduct(vertex.v, p.tex_q);
}

/* A convex projected polygon wholly outside one screen edge cannot contribute
 * any pixels. Reject it before doing UV setup or entering the rasterizer. */
static bool offScreen(const VERTEX *vertices, unsigned int count)
{
    const GLFix left(-1), right(SCREEN_WIDTH + 1);
    const GLFix top(-1), bottom(SCREEN_HEIGHT + 1);
    bool all_left = true, all_right = true, all_above = true, all_below = true;

    for(unsigned int i = 0; i < count; ++i)
    {
        all_left = all_left && vertices[i].x < left;
        all_right = all_right && vertices[i].x >= right;
        all_above = all_above && vertices[i].y < top;
        all_below = all_below && vertices[i].y >= bottom;
    }

    return all_left || all_right || all_above || all_below;
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
        // Project/cull before calculating face-specific UV-over-depth values.
        projectPosition(p_low);
        projectPosition(p_middle);
        projectPosition(p_high);

        VERTEX corners[3] = {
            MAKE_VERTEX(p_low.perspective, low),
            MAKE_VERTEX(p_middle.perspective, middle),
            MAKE_VERTEX(p_high.perspective, high)
        };

        if(backface_culling && nglIsBackface(&corners[0], &corners[1], &corners[2]))
            return false;
        if(offScreen(corners, 3))
            return true;

        attachTexture(corners[0], p_low);
        attachTexture(corners[1], p_middle);
        attachTexture(corners[2], p_high);
        nglDrawTriangleZClipped(&corners[0], &corners[1], &corners[2]);
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

    if(offScreen(clipped, clipped_count))
        return true;

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

