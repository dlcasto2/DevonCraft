#ifndef GL_H
#define GL_H

#ifndef __cplusplus
#error You need to use a C++ compiler to use nGL!
#endif

//nGL version 0.8
#include "fix.h"

#include "glconfig.h"

//These values are used to calculate offsets into the buffer.
//If you want something like FBOs, make them variables and set them accordingly.
//Watch out for different buffer sizes!
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

//GLFix is an integral part of all calculations.
//Changing resolution and width may be an improvement or even break everything.
typedef Fix<8, int32_t> GLFix;
// Q10 texture attributes for perspective-correct interpolation.
typedef Fix<10, int32_t> TexFix;

inline TexFix perspectiveReciprocalDepth(GLFix depth, GLFix near_depth)
{
    if(depth <= GLFix(0)) return TexFix(0);
    return TexFix(near_depth) / TexFix(depth);
}
inline TexFix perspectiveProduct(TexFix a, TexFix b)
{
    TexFix result;
    result.value = static_cast<int32_t>((static_cast<int64_t>(a.value) * b.value) / 1024);
    return result;
}
inline TexFix perspectiveProduct(GLFix coordinate, TexFix reciprocal_depth)
{
    return perspectiveProduct(TexFix(coordinate), reciprocal_depth);
}
inline TexFix perspectiveLerp(TexFix a, TexFix b, TexFix t)
{
    TexFix result;
    result.value = static_cast<int32_t>(a.value + (static_cast<int64_t>(b.value - a.value) * t.value) / 1024);
    return result;
}
inline GLFix perspectiveRecoverCoordinate(TexFix coordinate_over_depth, TexFix reciprocal_depth)
{
    if(reciprocal_depth <= TexFix(0)) return GLFix(0);
    GLFix result;
    const int64_t q10 = (static_cast<int64_t>(coordinate_over_depth.value) * 1024) / reciprocal_depth.value;
    result.value = static_cast<int32_t>(q10 / 4);
    return result;
}

 // Keep sampler indices inside the bitmap even when fixed-point interpolation
 // produces a coordinate just outside the texture edge.
inline int clampTextureTexel(int coordinate, int extent)
{
    if(extent <= 0)
        return 0;
    if(coordinate < 0)
        return 0;
    if(coordinate >= extent)
        return extent - 1;
    return coordinate;
}

/* Column vectors and matrices:
 * [ [0][0] [0][1] [0][2] [0][3] ]   [x]
 * [ [1][0] [1][1] [1][2] [1][3] ] * [y]
 * [ [2][0] [2][1] [2][2] [2][3] ]   [z]
 * [   0      0      0      1    ]   [1] (implicit)
 *
 * The 4th row (translation) is treated with integer precision only
 * for greater range during matrix multiplication.
 */

/* If TEXTURE_SUPPORT is enabled and a VERTEX has this as color, black pixels of the texture won't be drawn */
#define TEXTURE_TRANSPARENT 0xF000
/* Disables backface culling for this face */
#define TEXTURE_DRAW_BACKFACE 0x0F00

typedef uint16_t COLOR;

struct VECTOR3
{
    VECTOR3() : VECTOR3(0, 0, 0) {}
    VECTOR3(const GLFix x, const GLFix y, const GLFix z)
        : x(x), y(y), z(z) {}

    void print() const { printf("(%d %d %d)\n", x.toInteger<int>(), y.toInteger<int>(), z.toInteger<int>()); }

    GLFix x, y, z;
};

struct VERTEX
{
    VERTEX() : VERTEX(0, 0, 0, 0, 0, 0) {}
    VERTEX(const GLFix x, const GLFix y, const GLFix z, const GLFix u, const GLFix v, const COLOR c)
        : x(x), y(y), z(z), u(u), v(v), c(c), tex_q(0), u_over_z(0), v_over_z(0) {}

    void print() const { printf("(%d %d %d) (0x%x) (%d %d)\n", x.toInteger<int>(), y.toInteger<int>(), z.toInteger<int>(), c, u.toInteger<int>(), v.toInteger<int>()); }

    GLFix x, y, z;
    GLFix u, v;
    COLOR c;
    // Populated after camera-space clipping; these interpolate linearly on screen.
    TexFix tex_q, u_over_z, v_over_z;
};

// Calculate the texture attributes required by perspective-correct rasterization.
// Aligned chunk meshes use cached projected positions, so they must explicitly
// initialize these fields just like nglPerspective(VERTEX*) does.
inline void nglSetPerspectiveTextureAttributes(VERTEX &vertex, GLFix near_depth)
{
    vertex.tex_q = perspectiveReciprocalDepth(vertex.z, near_depth);
    vertex.u_over_z = perspectiveProduct(vertex.u, vertex.tex_q);
    vertex.v_over_z = perspectiveProduct(vertex.v, vertex.tex_q);
}

// Interpolate fixed-point camera-space attributes without the 32-bit
// intermediate overflow in Fix::operator*. This is used at the near plane,
// where large camera-space coordinates can otherwise wrap before projection.
inline GLFix nglClipLerp(GLFix from, GLFix to, GLFix t)
{
    GLFix result;
    const int64_t delta = static_cast<int64_t>(to.value) - from.value;
    result.value = static_cast<int32_t>(
        static_cast<int64_t>(from.value)
        + (delta * t.value) / GLFix::scaleFactor());
    return result;
}

inline VERTEX nglIntersectNearPlane(const VERTEX &from, const VERTEX &to, GLFix clip_depth)
{
    const int64_t z_delta = static_cast<int64_t>(to.z.value) - from.z.value;
    VERTEX result;
    if(z_delta == 0)
        return result;

    GLFix t;
    const int64_t numerator =
        (static_cast<int64_t>(clip_depth.value) - from.z.value) * GLFix::scaleFactor();
    int64_t t_value = numerator / z_delta;
    if(t_value < 0)
        t_value = 0;
    else if(t_value > GLFix::scaleFactor())
        t_value = GLFix::scaleFactor();
    t.value = static_cast<int32_t>(t_value);

    result.x = nglClipLerp(from.x, to.x, t);
    result.y = nglClipLerp(from.y, to.y, t);
    result.z = clip_depth;
    result.u = nglClipLerp(from.u, to.u, t);
    result.v = nglClipLerp(from.v, to.v, t);
#ifdef INTERPOLATE_COLORS
    const int r0 = (from.c >> 11) & 31, r1 = (to.c >> 11) & 31;
    const int g0 = (from.c >> 5) & 63, g1 = (to.c >> 5) & 63;
    const int b0 = from.c & 31, b1 = to.c & 31;
    const int r = r0 + static_cast<int>((static_cast<int64_t>(r1 - r0) * t.value) / GLFix::scaleFactor());
    const int g = g0 + static_cast<int>((static_cast<int64_t>(g1 - g0) * t.value) / GLFix::scaleFactor());
    const int b = b0 + static_cast<int>((static_cast<int64_t>(b1 - b0) * t.value) / GLFix::scaleFactor());
    result.c = static_cast<COLOR>((r << 11) | (g << 5) | b);
#else
    // In textured mode c contains face flags rather than an interpolated color.
    result.c = from.c;
#endif
    return result;
}

// Sutherland-Hodgman clipping of a triangle against the camera-space near
// plane. A clipped triangle becomes a 3- or 4-vertex polygon, still in its
// original winding order. The caller projects it only after this step.
inline unsigned int nglClipTriangleToNearPlane(
    const VERTEX triangle[3], VERTEX clipped[4],
    GLFix clip_depth = GLFix(CLIP_PLANE))
{
    unsigned int count = 0;
    const VERTEX *previous = &triangle[2];
    bool previous_inside = previous->z >= clip_depth;

    for(unsigned int i = 0; i < 3; ++i)
    {
        const VERTEX *current = &triangle[i];
        const bool current_inside = current->z >= clip_depth;

        // Vertices exactly on the plane are already emitted as inside
        // vertices; do not add the same point again as an intersection.
        if(previous_inside != current_inside
           && previous->z != clip_depth && current->z != clip_depth)
            clipped[count++] = nglIntersectNearPlane(*previous, *current, clip_depth);

        if(current_inside)
            clipped[count++] = *current;

        previous = current;
        previous_inside = current_inside;
    }

    return count;
}

// Clip camera-space lines with the same geometric near plane used by triangles.
// near_plane is a projection scale, not the camera-space clipping distance.
inline bool nglClipLineToNearPlane(VERTEX &a, VERTEX &b, GLFix clip_depth)
{
    const bool a_inside = a.z >= clip_depth;
    const bool b_inside = b.z >= clip_depth;

    if(!a_inside && !b_inside)
        return false;
    if(a_inside != b_inside)
    {
        const VERTEX intersection = nglIntersectNearPlane(a, b, clip_depth);
        if(!a_inside)
            a = intersection;
        else
            b = intersection;
    }
    return true;
}

struct TEXTURE
{
    uint16_t width; uint16_t height;
    bool has_transparency; COLOR transparent_color;
    COLOR *bitmap;
};

class MATRIX {
public:
    MATRIX() {}
    GLFix data[3][4] = {};
};

#define GL_COLOR_BUFFER_BIT 1<<0
#define GL_DEPTH_BUFFER_BIT 1<<1

enum GLDrawMode
{
    GL_TRIANGLES,
    GL_QUADS,
    GL_QUAD_STRIP, //Not really tested
    GL_LINE_STRIP
};

//Range [0-1]
struct RGB
{
    RGB() : RGB(0,0,0) {}
    RGB(const GLFix r, const GLFix g, const GLFix b) : r(r), g(g), b(b) {}
    GLFix r, g, b;
};

#ifdef FPS_COUNTER
    extern volatile unsigned int fps;
#endif
extern MATRIX *transformation;

RGB rgbColor(const COLOR c);
COLOR colorRGB(const RGB rgb);
COLOR colorRGB(const GLFix r, const GLFix g, const GLFix b);

//Invoke once before using any other functions
void nglInit();
void nglUninit();
//The buffer to render to
void nglSetBuffer(COLOR *screenBuf);
void nglSetNearPlane(const GLFix near_plane);
// Display gamma as a percentage: 100 is neutral, 50 is darker, 200 is brighter.
void nglSetGamma(unsigned int percent);
GLFix nglGetNearPlane();
GLFix nglZBufferAt(const unsigned int x, const unsigned int y);
//Display the buffer
void nglDisplay();
void nglSetColor(const COLOR c);
void nglRotateX(const GLFix a);
void nglRotateY(const GLFix a);
void nglRotateZ(const GLFix a);
//To add nGL VERTEX instances directly without using old gl*3f calls
void nglAddVertices(const VERTEX *buffer, unsigned int length);
void nglAddVertex(const VERTEX &vertex);
void nglAddVertex(const VERTEX *vertex);
//Warning: The nglDraw*-Functions apply perspective projection!
//Returns whether the triangle is front-facing
bool nglDrawTriangle(const VERTEX *low, const VERTEX *middle, const VERTEX *high, bool backface_culling = true);
bool nglIsBackface(const VERTEX *v1, const VERTEX *v2, const VERTEX *v3);
void nglDrawTriangleZClipped(const VERTEX *low, const VERTEX *middle, const VERTEX *high);
void nglInterpolateVertexZ(const VERTEX *from, const VERTEX *to, VERTEX *res);
void nglDrawLine3D(const VERTEX *v1, const VERTEX *v2);

void nglPerspective(VERTEX *v);
void nglPerspective(VECTOR3 *v);
void nglMultMatVectRes(const MATRIX *mat1, const VERTEX *vect, VERTEX *res);
void nglMultMatVectRes(const MATRIX *mat1, const VECTOR3 *vect, VECTOR3 *res);
void nglMultMatMat(MATRIX *mat1, const MATRIX *mat2);
const TEXTURE *nglGetTexture();

void glLoadIdentity();
void glBegin(const GLDrawMode mode);
inline void glEnd() { }
void glClear(const int buffers);
//This has effectively only integer precision
void glTranslatef(const GLFix x, const GLFix y, const GLFix z);

void glBindTexture(const TEXTURE *tex);
void glTexCoord2f(const GLFix nu, const GLFix nv);
void glColor3f(const GLFix r, const GLFix g, const GLFix b);
void glVertex3f(const GLFix x, const GLFix y, const GLFix z);
void glScale3f(const GLFix x, const GLFix y, const GLFix z);
void glPushMatrix();
void glPopMatrix();

#endif
