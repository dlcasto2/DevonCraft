#include <ctime>
#include <utility>
#include <algorithm>
#include <new>

#ifdef _TINSPIRE
#include <libndls.h>
#else
#include <SDL/SDL.h>
#include <assert.h>
#include <signal.h>
static SDL_Surface *scr;
#endif

#include "gl.h"
#include "fastmath.h"
#include "gamma.h"

#define M(m, y, x) (m.data[y][x])
#define P(m, y, x) (m->data[y][x])

MATRIX *transformation;
static COLOR color;
static GLFix u, v;
static COLOR *screen;
static uint16_t *z_buffer;
static GLFix near_plane = 256;
static const TEXTURE *texture;
static unsigned int vertices_count = 0;
static VERTEX vertices[4];
static GLDrawMode draw_mode = GL_TRIANGLES;
static bool is_monochrome;
static COLOR *screen_inverted; //Also used as a gamma-corrected display buffer
static GammaTable gamma_table;
static unsigned int gamma_percent = 100;
#ifdef FPS_COUNTER
    volatile unsigned int fps;
#endif
static int matrix_stack_left = MATRIX_STACK_SIZE;

void nglInit()
{
    init_fastmath();
    transformation = new MATRIX[MATRIX_STACK_SIZE];

    //C++ <3
    z_buffer = new std::remove_reference<decltype(*z_buffer)>::type[SCREEN_WIDTH*SCREEN_HEIGHT];
    glLoadIdentity();
    color = colorRGB(0, 0, 0); //Black
    u = v = 0;

    texture = nullptr;
    vertices_count = 0;
    draw_mode = GL_TRIANGLES;

    #ifdef _TINSPIRE
        is_monochrome = lcd_type() == SCR_320x240_4;

        if(is_monochrome)
        {
            screen_inverted = new COLOR[SCREEN_WIDTH*SCREEN_HEIGHT];
            lcd_init(SCR_320x240_16);
        }
        else
            lcd_init(SCR_320x240_565);
    #else
        SDL_Init(SDL_INIT_VIDEO);
        scr = SDL_SetVideoMode(SCREEN_WIDTH, SCREEN_HEIGHT, 16, SDL_SWSURFACE);
        signal(SIGINT, SIG_DFL);
        assert(scr);
    #endif

    matrix_stack_left = MATRIX_STACK_SIZE;
}

void nglUninit()
{
    uninit_fastmath();
    delete[] transformation;
    delete[] z_buffer;

    delete[] screen_inverted;

    #ifdef _TINSPIRE
        lcd_init(SCR_TYPE_INVALID);
    #else
        //TODO
    #endif
}

void nglMultMatMat(MATRIX *mat1, MATRIX *mat2)
{
    GLFix a00 = P(mat1, 0, 0), a01 = P(mat1, 0, 1), a02 = P(mat1, 0, 2);
    GLFix a10 = P(mat1, 1, 0), a11 = P(mat1, 1, 1), a12 = P(mat1, 1, 2);
    GLFix a20 = P(mat1, 2, 0), a21 = P(mat1, 2, 1), a22 = P(mat1, 2, 2);
    // Multiplying GLFix with GLFix uses a Fix<16> intermediate value,
    // which limits the result to 16 bits, while Fix * int does not lose
    // width. Allow for greater range by treating this column as integers.
    // This way, glTranslatef(x) + glTranslatef(-x) cancels each other
    // out even for large x.
    int a03 = P(mat1, 0, 3), a13 = P(mat1, 1, 3), a23 = P(mat1, 2, 3);

    GLFix b00 = P(mat2, 0, 0), b01 = P(mat2, 0, 1), b02 = P(mat2, 0, 2);
    GLFix b10 = P(mat2, 1, 0), b11 = P(mat2, 1, 1), b12 = P(mat2, 1, 2);
    GLFix b20 = P(mat2, 2, 0), b21 = P(mat2, 2, 1), b22 = P(mat2, 2, 2);
    int b03 = P(mat2, 0, 3), b13 = P(mat2, 1, 3), b23 = P(mat2, 2, 3);

    P(mat1, 0, 0) = a00*b00 + a01*b10 + a02*b20;
    P(mat1, 0, 1) = a00*b01 + a01*b11 + a02*b21;
    P(mat1, 0, 2) = a00*b02 + a01*b12 + a02*b22;
    P(mat1, 0, 3) = a00*b03 + a01*b13 + a02*b23 + a03;
    P(mat1, 1, 0) = a10*b00 + a11*b10 + a12*b20;
    P(mat1, 1, 1) = a10*b01 + a11*b11 + a12*b21;
    P(mat1, 1, 2) = a10*b02 + a11*b12 + a12*b22;
    P(mat1, 1, 3) = a10*b03 + a11*b13 + a12*b23 + a13;
    P(mat1, 2, 0) = a20*b00 + a21*b10 + a22*b20;
    P(mat1, 2, 1) = a20*b01 + a21*b11 + a22*b21;
    P(mat1, 2, 2) = a20*b02 + a21*b12 + a22*b22;
    P(mat1, 2, 3) = a20*b03 + a21*b13 + a22*b23 + a23;
}

void nglMultMatVectRes(const MATRIX *mat1, const VERTEX *vect, VERTEX *res)
{
    GLFix x = vect->x, y = vect->y, z = vect->z;

    res->x = P(mat1, 0, 0)*x + P(mat1, 0, 1)*y + P(mat1, 0, 2)*z + P(mat1, 0, 3);
    res->y = P(mat1, 1, 0)*x + P(mat1, 1, 1)*y + P(mat1, 1, 2)*z + P(mat1, 1, 3);
    res->z = P(mat1, 2, 0)*x + P(mat1, 2, 1)*y + P(mat1, 2, 2)*z + P(mat1, 2, 3);
}

void nglMultMatVectRes(const MATRIX *mat1, const VECTOR3 *vect, VECTOR3 *res)
{
    GLFix x = vect->x, y = vect->y, z = vect->z;

    res->x = P(mat1, 0, 0)*x + P(mat1, 0, 1)*y + P(mat1, 0, 2)*z + P(mat1, 0, 3);
    res->y = P(mat1, 1, 0)*x + P(mat1, 1, 1)*y + P(mat1, 1, 2)*z + P(mat1, 1, 3);
    res->z = P(mat1, 2, 0)*x + P(mat1, 2, 1)*y + P(mat1, 2, 2)*z + P(mat1, 2, 3);
}

void nglPerspective(VERTEX *v)
{
#ifdef BETTER_PERSPECTIVE
    float new_z = v->z;
    decltype(new_z) new_x = v->x, new_y = v->y;
    decltype(new_z) div = decltype(new_z)(near_plane)/new_z;

    new_x *= div;
    new_y *= div;

    v->x = new_x;
    v->y = new_y;
#else
    auto div = Fix<12, int32_t>(near_plane)/v->z.toInteger<int>();

    //Round to integers, as we don't lose the topmost bits with integer multiplication
    v->x = div * v->x.toInteger<int>();
    v->y = div * v->y.toInteger<int>();
#endif

    // (0/0) is in the center of the screen
    v->x += SCREEN_WIDTH/2;
    v->y += SCREEN_HEIGHT/2;

    v->y = GLFix(SCREEN_HEIGHT - 1) - v->y;

    //TODO: Move this somewhere else
    if(!texture)
        return;

    if(v->u > GLFix(texture->width))
    {
        printf("Warning: Texture coord out of bounds!\n");
        v->u = texture->width;
    }
    else if(v->u < GLFix(0))
    {
        printf("Warning: Texture coord out of bounds!\n");
        v->u = 0;
    }

    if(v->v > GLFix(texture->height))
    {
        printf("Warning: Texture coord out of bounds!\n");
        v->v = texture->height;
    }
    else if(v->v < GLFix(0))
    {
        printf("Warning: Texture coord out of bounds!\n");
        v->v = 0;
    }

    nglSetPerspectiveTextureAttributes(*v, near_plane);
}

void nglPerspective(VECTOR3 *v)
{
#ifdef BETTER_PERSPECTIVE
    float new_z = v->z;
    decltype(new_z) new_x = v->x, new_y = v->y;
    decltype(new_z) div = decltype(new_z)(near_plane)/new_z;

    new_x *= div;
    new_y *= div;

    v->x = new_x;
    v->y = new_y;
#else
    auto div = Fix<12, int32_t>(near_plane)/v->z.toInteger<int>();

    //Round to integers, as we don't lose the topmost bits with integer multiplication
    v->x = div * v->x.toInteger<int>();
    v->y = div * v->y.toInteger<int>();
#endif

    // (0/0) is in the center of the screen
    v->x += SCREEN_WIDTH/2;
    v->y += SCREEN_HEIGHT/2;

    v->y = GLFix(SCREEN_HEIGHT - 1) - v->y;
}

void nglSetBuffer(COLOR *screenBuf)
{
    screen = screenBuf;
}

void nglSetGamma(unsigned int percent)
{
    if(percent < 50) percent = 50;
    if(percent > 200) percent = 200;

#ifdef _TINSPIRE
    // Classic monochrome calculators already own this 16-bit staging buffer
    // for LCD polarity inversion, so gamma adds no extra RAM on the target.
    // Color calculators allocate it only when a non-neutral curve is needed.
    if(!is_monochrome && percent != 100 && !screen_inverted)
    {
        screen_inverted = new(std::nothrow) COLOR[SCREEN_WIDTH * SCREEN_HEIGHT];
        if(!screen_inverted)
            percent = 100; // Keep rendering safely if the optional buffer cannot fit.
    }
#endif

    gamma_percent = percent;
    gamma_table.set(percent);
}

void nglDisplay()
{
    #ifdef _TINSPIRE
        if(is_monochrome)
        {
            //Flip everything, as 0xFFFF is white on CX, but black on classic.
            //Correct gamma before inversion so the LCD polarity operation remains exact.
            COLOR *ptr = screen + SCREEN_HEIGHT*SCREEN_WIDTH;
            COLOR *ptr_inv = screen_inverted + SCREEN_HEIGHT*SCREEN_WIDTH;
            if(gamma_percent == 100)
            {
                // Preserve the original fast path at the default setting.
                while(--ptr >= screen)
                    *--ptr_inv = static_cast<COLOR>(~*ptr);
            }
            else
            {
                while(--ptr >= screen)
                    *--ptr_inv = static_cast<COLOR>(~gamma_table.apply(*ptr));
            }

            lcd_blit(screen_inverted, SCR_320x240_16);
        }
        else if(gamma_percent != 100 && screen_inverted)
        {
            const unsigned int count = SCREEN_WIDTH * SCREEN_HEIGHT;
            for(unsigned int i = 0; i < count; ++i)
                screen_inverted[i] = gamma_table.apply(screen[i]);
            lcd_blit(screen_inverted, SCR_320x240_565);
        }
        else
            lcd_blit(screen, SCR_320x240_565);
    #else
        SDL_LockSurface(scr);
        COLOR *display = reinterpret_cast<COLOR*>(scr->pixels);
        const unsigned int count = SCREEN_WIDTH * SCREEN_HEIGHT;
        if(gamma_percent == 100)
            std::copy(screen, screen + count, display);
        else
            for(unsigned int i = 0; i < count; ++i)
                display[i] = gamma_table.apply(screen[i]);
        SDL_UnlockSurface(scr);
        SDL_UpdateRect(scr, 0, 0, 0, 0);
    #endif

    #ifdef FPS_COUNTER
        static unsigned int frames = 0;
        ++frames;

        static time_t last = 0;
        time_t now = time(nullptr);
        if(now != last)
        {
            fps = frames;
            printf("FPS: %u\n", frames);
            last = now;
            frames = 0;
        }
    #endif
}

void nglRotateX(const GLFix a)
{
    MATRIX rot;

    const GLFix sina = fast_sin(a), cosa = fast_cos(a);

    M(rot, 0, 0) = 1;
    M(rot, 1, 1) = cosa;
    M(rot, 1, 2) = -sina;
    M(rot, 2, 1) = sina;
    M(rot, 2, 2) = cosa;

    nglMultMatMat(transformation, &rot);
}

void nglRotateY(const GLFix a)
{
    MATRIX rot;

    const GLFix sina = fast_sin(a), cosa = fast_cos(a);

    M(rot, 0, 0) = cosa;
    M(rot, 0, 2) = sina;
    M(rot, 1, 1) = 1;
    M(rot, 2, 0) = -sina;
    M(rot, 2, 2) = cosa;

    nglMultMatMat(transformation, &rot);
}

void nglRotateZ(const GLFix a)
{
    MATRIX rot;

    const GLFix sina = fast_sin(a), cosa = fast_cos(a);

    M(rot, 0, 0) = cosa;
    M(rot, 0, 1) = -sina;
    M(rot, 1, 0) = sina;
    M(rot, 1, 1) = cosa;
    M(rot, 2, 2) = 1;

    nglMultMatMat(transformation, &rot);
}

inline void pixel(const int x, const int y, const GLFix z, const COLOR c)
{
    if(x < 0 || y < 0 || x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT)
        return;

    const int pitch = x + y*SCREEN_WIDTH;

    if(z <= GLFix(CLIP_PLANE) || GLFix(z_buffer[pitch]) <= z)
        return;

    z_buffer[pitch] = z;

    screen[pitch] = c;
}

RGB rgbColor(const COLOR c)
{
    const GLFix r = (c >> 11) & 0b11111;
    const GLFix g = (c >> 5) & 0b111111;
    const GLFix b = (c >> 0) & 0b11111;

    return {r / GLFix(0b11111), g / GLFix(0b111111), b / GLFix(0b11111)};
}

COLOR colorRGB(const RGB rgb)
{
    return colorRGB(rgb.r, rgb.g, rgb.b);
}

COLOR colorRGB(const GLFix r, const GLFix g, const GLFix b)
{
    const int r1 = (r * GLFix(0b11111)).round();
    const int g1 = (g * GLFix(0b111111)).round();
    const int b1 = (b * GLFix(0b11111)).round();

    return ((r1 & 0b11111) << 11) | ((g1 & 0b111111) << 5) | (b1 & 0b11111);
}

GLFix nglZBufferAt(const unsigned int x, const unsigned int y)
{
    if(x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT)
        return 0;

    return z_buffer[x + y * SCREEN_WIDTH];
}


namespace {

// Liang-Barsky clipping in projected screen space. Parameters are stored
// as Q8 raw values so even almost-parallel off-screen edges cannot overflow
// a GLFix division before they are rejected.
bool clipLineParameter(GLFix p, GLFix q, int64_t &t0, int64_t &t1)
{
    if(p.value == 0)
        return q >= GLFix(0);

    const int64_t r = (static_cast<int64_t>(q.value) * GLFix::scaleFactor()) / p.value;
    if(p.value < 0)
    {
        if(r > t1)
            return false;
        if(r > t0)
            t0 = r;
    }
    else
    {
        if(r < t0)
            return false;
        if(r < t1)
            t1 = r;
    }
    return true;
}

VERTEX interpolateProjectedLine(const VERTEX &from, const VERTEX &to, int64_t t_raw)
{
    // Do not use GLFix multiplication here: its intermediate is int32_t,
    // while projected endpoints can be many screen widths apart after
    // near-plane clipping. Keep the delta-times-parameter product in int64_t.
    const auto lerp = [t_raw](GLFix a, GLFix b) {
        GLFix result;
        result.value = static_cast<int32_t>(
            static_cast<int64_t>(a.value)
            + ((static_cast<int64_t>(b.value) - a.value) * t_raw)
                / GLFix::scaleFactor());
        return result;
    };

    VERTEX result = from;
    result.x = lerp(from.x, to.x);
    result.y = lerp(from.y, to.y);
    result.z = lerp(from.z, to.z);
    return result;
}

bool clipLineToViewport(VERTEX &a, VERTEX &b)
{
    const GLFix dx = b.x - a.x;
    const GLFix dy = b.y - a.y;
    int64_t t0 = 0;
    int64_t t1 = GLFix::scaleFactor();

    if(!clipLineParameter(-dx, a.x, t0, t1)
       || !clipLineParameter(dx, GLFix(SCREEN_WIDTH - 1) - a.x, t0, t1)
       || !clipLineParameter(-dy, a.y, t0, t1)
       || !clipLineParameter(dy, GLFix(SCREEN_HEIGHT - 1) - a.y, t0, t1))
        return false;

    const VERTEX original_a = a;
    const VERTEX original_b = b;
    if(t1 < GLFix::scaleFactor())
        b = interpolateProjectedLine(original_a, original_b, t1);
    if(t0 > 0)
        a = interpolateProjectedLine(original_a, original_b, t0);
    return true;
}

} // namespace

// Doesn't interpolate colors even if enabled.
void nglDrawLine3D(const VERTEX *v1, const VERTEX *v2)
{
    VERTEX v1_p = *v1, v2_p = *v2;
    if(!nglClipLineToNearPlane(v1_p, v2_p, GLFix(CLIP_PLANE)))
        return;

    nglPerspective(&v1_p);
    nglPerspective(&v2_p);

    // Keep the raster loops bounded even when a near-plane intersection
    // projects to a very distant screen coordinate.
    if(!clipLineToViewport(v1_p, v2_p))
        return;

    const GLFix diff_x = v2_p.x - v1_p.x;
    const COLOR c = v1_p.c;

    // A perfectly vertical projected line has no X delta. Handle it before
    // calculating dy/dx, which would otherwise divide by zero on the calculator.
    if(diff_x == GLFix(0))
    {
        if(v2_p.y < v1_p.y)
            std::swap(v1_p, v2_p);

        const GLFix diff_y = v2_p.y - v1_p.y;
        if(diff_y == GLFix(0))
        {
            pixel(v1_p.x, v1_p.y, v1_p.z, c);
            return;
        }

        const GLFix dz = (v2_p.z - v1_p.z) / diff_y;
        int end_y = v2_p.y;
        if(end_y >= SCREEN_HEIGHT)
            end_y = SCREEN_HEIGHT - 1;

        for(; v1_p.y <= GLFix(end_y); ++v1_p.y)
        {
            pixel(v1_p.x, v1_p.y, v1_p.z, c);
            v1_p.z += dz;
        }
        return;
    }

    const GLFix dy = (v2_p.y - v1_p.y) / diff_x;

    //Height > width? -> Interpolate X
    if(dy > GLFix(1) || dy < GLFix(-1))
    {
        if(v2_p.y < v1_p.y)
            std::swap(v1_p, v2_p);

        const GLFix diff_y = v2_p.y - v1_p.y;

        const GLFix dx = (v2_p.x - v1_p.x) / diff_y;
        const GLFix dz = (v2_p.z - v1_p.z) / diff_y;

        int end_y = v2_p.y;

        if(end_y >= SCREEN_HEIGHT)
            end_y = SCREEN_HEIGHT - 1;

        for(; v1_p.y <= GLFix(end_y); ++v1_p.y)
        {
            pixel(v1_p.x, v1_p.y, v1_p.z, c);

            v1_p.x += dx;
            v1_p.z += dz;
        }
    }
    else
    {
        if(v2_p.x < v1_p.x)
            std::swap(v1_p, v2_p);

        const GLFix dz = (v2_p.z - v1_p.z) / diff_x;

        int end_x = v2_p.x;

        if(end_x >= SCREEN_WIDTH)
            end_x = SCREEN_WIDTH - 1;

        for(; v1_p.x <= GLFix(end_x); ++v1_p.x)
        {
            pixel(v1_p.x, v1_p.y, v1_p.z, c);

            v1_p.y += dy;
            v1_p.z += dz;
        }
    }
}

//I hate code duplication more than macros and includes
#ifdef TEXTURE_SUPPORT
    #define TRANSPARENCY
    #include "triangle.inc.h"
    #undef TRANSPARENCY
    #undef TEXTURE_SUPPORT
    #define FORCE_COLOR
    #include "triangle.inc.h"
    #define TEXTURE_SUPPORT
    #undef FORCE_COLOR
#endif
#include "triangle.inc.h"

static void interpolateVertexXLeft(const VERTEX *from, const VERTEX *to, VERTEX *res)
{
    GLFix diff = to->x - from->x;
    GLFix end = 0;
    GLFix t = (end - from->x) / diff;

    res->x = end;
    res->y = from->y + (to->y - from->y) * t;
    res->z = from->z + (to->z - from->z) * t;

#ifdef TEXTURE_SUPPORT
    res->u = from->u + (to->u - from->u) * t;
    res->u = res->u.wholes();
    res->v = from->v + (to->v - from->v) * t;
    res->v = res->v.wholes();
    const TexFix tex_t(t);
    res->tex_q = perspectiveLerp(from->tex_q, to->tex_q, tex_t);
    res->u_over_z = perspectiveLerp(from->u_over_z, to->u_over_z, tex_t);
    res->v_over_z = perspectiveLerp(from->v_over_z, to->v_over_z, tex_t);
#endif

#ifdef INTERPOLATE_COLORS
    RGB c_from = rgbColor(from->c);
    RGB c_to = rgbColor(to->c);

    res->c = colorRGB(c_from.r + (c_to.r - c_from.r) * t, c_from.r + (c_to.r - c_from.r) * t, c_from.r + (c_to.r - c_from.r) * t);
#else
    res->c = from->c;
#endif
}

//Left X clipping
void nglDrawTriangleXRightZClipped(const VERTEX *low, const VERTEX *middle, const VERTEX *high)
{
    const VERTEX* invisible[3];
    const VERTEX* visible[3];
    int count_invisible = -1, count_visible = -1;

    if(low->x < GLFix(0))
        invisible[++count_invisible] = low;
    else
        visible[++count_visible] = low;

    if(middle->x < GLFix(0))
        invisible[++count_invisible] = middle;
    else
        visible[++count_visible] = middle;

    if(high->x < GLFix(0))
        invisible[++count_invisible] = high;
    else
        visible[++count_visible] = high;

    //Interpolated vertices
    VERTEX v1, v2;

    switch(count_visible)
    {
    case -1:
        break;
    case 0:
        interpolateVertexXLeft(invisible[0], visible[0], &v1);
        interpolateVertexXLeft(invisible[1], visible[0], &v2);
        nglDrawTriangleXZClipped(visible[0], &v1, &v2);
        break;
    case 1:
        interpolateVertexXLeft(invisible[0], visible[0], &v1);
        interpolateVertexXLeft(invisible[0], visible[1], &v2);
        nglDrawTriangleXZClipped(visible[0], visible[1], &v1);
        nglDrawTriangleXZClipped(visible[1], &v1, &v2);
        break;
    case 2:
        nglDrawTriangleXZClipped(visible[0], visible[1], visible[2]);
        break;
    }
}

static void interpolateVertexXRight(const VERTEX *from, const VERTEX *to, VERTEX *res)
{
    GLFix diff = to->x - from->x;
    GLFix end = (SCREEN_WIDTH - 1);
    GLFix t = (end - from->x) / diff;

    res->x = end;
    res->y = from->y + (to->y - from->y) * t;
    res->z = from->z + (to->z - from->z) * t;

#ifdef TEXTURE_SUPPORT
    res->u = from->u + (to->u - from->u) * t;
    res->u = res->u.wholes();
    res->v = from->v + (to->v - from->v) * t;
    res->v = res->v.wholes();
    const TexFix tex_t(t);
    res->tex_q = perspectiveLerp(from->tex_q, to->tex_q, tex_t);
    res->u_over_z = perspectiveLerp(from->u_over_z, to->u_over_z, tex_t);
    res->v_over_z = perspectiveLerp(from->v_over_z, to->v_over_z, tex_t);
#endif

#ifdef INTERPOLATE_COLORS
    RGB c_from = rgbColor(from->c);
    RGB c_to = rgbColor(to->c);

    res->c = colorRGB(c_from.r + (c_to.r - c_from.r) * t, c_from.r + (c_to.r - c_from.r) * t, c_from.r + (c_to.r - c_from.r) * t);
#else
    res->c = from->c;
#endif
}

//Right X clipping
void nglDrawTriangleZClipped(const VERTEX *low, const VERTEX *middle, const VERTEX *high)
{
    //If not on screen, skip
    if((low->x < GLFix(0) && middle->x < GLFix(0) && high->x < GLFix(0))
       || (low->x >= GLFix(SCREEN_WIDTH) && middle->x >= GLFix(SCREEN_WIDTH) && high->x >= GLFix(SCREEN_WIDTH))
       || (low->y < GLFix(0) && middle->y < GLFix(0) && high->y < GLFix(0))
       || (low->y >= GLFix(SCREEN_HEIGHT) && middle->y >= GLFix(SCREEN_HEIGHT) && high->y >= GLFix(SCREEN_HEIGHT)))
        return;

    const VERTEX* invisible[3];
    const VERTEX* visible[3];
    int count_invisible = -1, count_visible = -1;

    if(low->x > GLFix(SCREEN_WIDTH-1))
        invisible[++count_invisible] = low;
    else
        visible[++count_visible] = low;

    if(middle->x > GLFix(SCREEN_WIDTH-1))
        invisible[++count_invisible] = middle;
    else
        visible[++count_visible] = middle;

    if(high->x > GLFix(SCREEN_WIDTH-1))
        invisible[++count_invisible] = high;
    else
        visible[++count_visible] = high;

    //Interpolated vertices
    VERTEX v1, v2;

    switch(count_visible)
    {
    case -1:
        break;
    case 0:
        interpolateVertexXRight(invisible[0], visible[0], &v1);
        interpolateVertexXRight(invisible[1], visible[0], &v2);
        nglDrawTriangleXRightZClipped(visible[0], &v1, &v2);
        break;
    case 1:
        interpolateVertexXRight(invisible[0], visible[0], &v1);
        interpolateVertexXRight(invisible[0], visible[1], &v2);
        nglDrawTriangleXRightZClipped(visible[0], visible[1], &v1);
        nglDrawTriangleXRightZClipped(visible[1], &v1, &v2);
        break;
    case 2:
        nglDrawTriangleXRightZClipped(visible[0], visible[1], visible[2]);
        break;
    }
}

#ifdef Z_CLIPPING
    void nglInterpolateVertexZ(const VERTEX *from, const VERTEX *to, VERTEX *res)
    {
        *res = nglIntersectNearPlane(*from, *to, GLFix(CLIP_PLANE));
    }
#endif

bool nglIsBackface(const VERTEX *v1, const VERTEX *v2, const VERTEX *v3)
{
    int x1 = v2->x - v1->x, x2 = v3->x - v1->x;
    int y1 = v2->y - v1->y, y2 = v3->y - v1->y;

    return x1 * y2 < x2 * y1;
}

bool nglIsBackface(const VECTOR3 *v1, const VECTOR3 *v2, const VECTOR3 *v3)
{
    int x1 = v2->x - v1->x, x2 = v3->x - v1->x;
    int y1 = v2->y - v1->y, y2 = v3->y - v1->y;

    return x1 * y2 < x2 * y1;
}

bool nglDrawTriangle(const VERTEX *low, const VERTEX *middle, const VERTEX *high, bool backface_culling)
{
#ifndef Z_CLIPPING
    if(low->z < GLFix(CLIP_PLANE) || middle->z < GLFix(CLIP_PLANE) || high->z < GLFix(CLIP_PLANE))
        return true;

    VERTEX low_p = *low, middle_p = *middle, high_p = *high;

    nglPerspective(&low_p);
    nglPerspective(&middle_p);
    nglPerspective(&high_p);

    if(backface_culling && nglIsBackface(&low_p, &middle_p, &high_p))
        return false;

    nglDrawTriangleZClipped(&low_p, &middle_p, &high_p);
    return true;
#else
    const VERTEX source[3] = {*low, *middle, *high};
    VERTEX clipped[4];
    const unsigned int clipped_count =
        nglClipTriangleToNearPlane(source, clipped, GLFix(CLIP_PLANE));

    if(clipped_count < 3)
        return true;

    // Projection must happen after clipping: a vertex behind the camera is
    // never divided by z and cannot wrap across the screen.
    for(unsigned int i = 0; i < clipped_count; ++i)
        nglPerspective(&clipped[i]);

    // Clipping preserves polygon winding. Test the first three projected
    // vertices for both triangles and quads before fan-triangulating.
    if(backface_culling
       && nglIsBackface(&clipped[0], &clipped[1], &clipped[2]))
        return false;

    // A triangle clipped by one plane can become a quad. Fan-triangulate it
    // only after perspective projection and preserve its original winding.
    for(unsigned int i = 1; i + 1 < clipped_count; ++i)
        nglDrawTriangleZClipped(&clipped[0], &clipped[i], &clipped[i + 1]);

    return true;
#endif
}

void nglSetColor(const COLOR c)
{
    color = c;
}

void nglAddVertices(const VERTEX *buffer, unsigned int length)
{
    while(length--)
        nglAddVertex(buffer++);
}

void nglAddVertex(const VERTEX &vertex)
{
    nglAddVertex(&vertex);
}

void nglAddVertex(const VERTEX* vertex)
{
    VERTEX *current_vertex = &vertices[vertices_count];

    current_vertex->c = vertex->c;
    #ifdef TEXTURE_SUPPORT
        current_vertex->u = vertex->u;
        current_vertex->v = vertex->v;
    #endif

    nglMultMatVectRes(transformation, vertex, current_vertex);

    ++vertices_count;

    switch(draw_mode)
    {
    case GL_TRIANGLES:
        if(vertices_count != 3)
            break;

        vertices_count = 0;

#ifdef WIREFRAME_MODE
        nglDrawLine3D(&vertices[0], &vertices[1]);
        nglDrawLine3D(&vertices[0], &vertices[2]);
        nglDrawLine3D(&vertices[2], &vertices[1]);
#else
        nglDrawTriangle(&vertices[0], &vertices[1], &vertices[2], !texture || (vertices[0].c & TEXTURE_DRAW_BACKFACE) != TEXTURE_DRAW_BACKFACE);
#endif
        break;

    case GL_QUADS:
        if(vertices_count != 4)
            break;

        vertices_count = 0;

#ifdef WIREFRAME_MODE
        nglDrawLine3D(&vertices[0], &vertices[1]);
        nglDrawLine3D(&vertices[1], &vertices[2]);
        nglDrawLine3D(&vertices[2], &vertices[3]);
        nglDrawLine3D(&vertices[3], &vertices[0]);
#else
        if(nglDrawTriangle(&vertices[0], &vertices[1], &vertices[2], !texture || (vertices[0].c & TEXTURE_DRAW_BACKFACE) != TEXTURE_DRAW_BACKFACE))
            nglDrawTriangle(&vertices[2], &vertices[3], &vertices[0], false);
#endif
        break;

    case GL_QUAD_STRIP:
        if(vertices_count != 4)
            break;

        vertices_count = 2;

#ifdef WIREFRAME_MODE
        nglDrawLine3D(&vertices[0], &vertices[1]);
        nglDrawLine3D(&vertices[1], &vertices[2]);
        nglDrawLine3D(&vertices[2], &vertices[3]);
        nglDrawLine3D(&vertices[3], &vertices[0]);
#else
        if(nglDrawTriangle(&vertices[0], &vertices[1], &vertices[2], !texture || (vertices[0].c & TEXTURE_DRAW_BACKFACE) != TEXTURE_DRAW_BACKFACE))
            nglDrawTriangle(&vertices[2], &vertices[3], &vertices[0], false);
#endif

        vertices[0] = vertices[2];
        vertices[1] = vertices[3];
        break;
    case GL_LINE_STRIP:
        if(vertices_count != 2)
            break;

        vertices_count = 1;

        nglDrawLine3D(&vertices[0], &vertices[1]);

        vertices[0] = vertices[1];
        break;
    }
}

const TEXTURE *nglGetTexture()
{
    return texture;
}

void glBindTexture(const TEXTURE *tex)
{
    texture = tex;

    if(tex && tex->has_transparency && tex->transparent_color != 0)
        printf("Bound texture doesn't have black as transparent color!\n");
}

void nglSetNearPlane(const GLFix new_near_plane)
{
    near_plane = new_near_plane;
}

GLFix nglGetNearPlane()
{
    return near_plane;
}

void glColor3f(const GLFix r, const GLFix g, const GLFix b)
{
    color = colorRGB(r, g, b);
}

void glTexCoord2f(const GLFix nu, const GLFix nv)
{
    u = nu;
    v = nv;
}

void glVertex3f(const GLFix x, const GLFix y, const GLFix z)
{
    const VERTEX ver{x, y, z, u, v, color};
    nglAddVertex(&ver);
}

void glBegin(GLDrawMode mode)
{
    vertices_count = 0;
    draw_mode = mode;
}

void glClear(const int buffers)
{
    if(buffers & GL_COLOR_BUFFER_BIT)
        std::fill(screen, screen + SCREEN_WIDTH*SCREEN_HEIGHT, color);

    if(buffers & GL_DEPTH_BUFFER_BIT)
        std::fill(z_buffer, z_buffer + SCREEN_WIDTH*SCREEN_HEIGHT, UINT16_MAX);
}

void glLoadIdentity()
{
    *transformation = {}; // Copy empty matrix into transformation
    P(transformation, 0, 0) = P(transformation, 1, 1) = P(transformation, 2, 2) = 1;
}

void glTranslatef(const GLFix x, const GLFix y, const GLFix z)
{
    MATRIX trans;

    M(trans, 0, 0) = M(trans, 1, 1) = M(trans, 2, 2) = 1;
    M(trans, 0, 3) = x;
    M(trans, 1, 3) = y;
    M(trans, 2, 3) = z;

    nglMultMatMat(transformation, &trans);
}

void glScale3f(const GLFix x, const GLFix y, const GLFix z)
{
    MATRIX scale;

    M(scale, 0, 0) = x;
    M(scale, 1, 1) = y;
    M(scale, 2, 2) = z;

    nglMultMatMat(transformation, &scale);
}

void glPopMatrix()
{
    if(matrix_stack_left == MATRIX_STACK_SIZE)
    {
        printf("Error: No matrix left on the stack anymore!\n");
        return;
    }
    ++matrix_stack_left;

    --transformation;
}

void glPushMatrix()
{
    if(matrix_stack_left == 0)
    {
        printf("Error: Matrix stack limit reached!\n");
        return;
    }
    matrix_stack_left--;

    ++transformation;
    *transformation = *(transformation - 1);
}
