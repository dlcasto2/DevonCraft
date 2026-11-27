// Renders a textured floor through the real nGL pipeline and checks every
// covered pixel against a perspective-correct reference computed from the
// same projected vertices. Each texel encodes its own (x, y) so the error is
// read straight back from the framebuffer, in texels.
#include <cstdio>
#include <cmath>
#include <algorithm>
#include "gl.h"

static COLOR fb[SCREEN_WIDTH * SCREEN_HEIGHT];
static COLOR texbits[64 * 64];

struct Tri { VERTEX v[3]; };

int main()
{
    nglInit();
    nglSetBuffer(fb);
    nglSetNearPlane(GLFix(1));

    for(int y = 0; y < 64; ++y)
        for(int x = 0; x < 64; ++x)
            texbits[y * 64 + x] = (((x >> 5) & 1) << 11) | (y << 5) | (x & 31);
    TEXTURE tex{64, 64, false, 0, texbits};
    glBindTexture(&tex);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Floor below the camera. u runs across x, v runs with depth.
    auto V = [](float x, float y, float z, float u, float v) {
        return VERTEX(GLFix(x), GLFix(y), GLFix(z), GLFix(u), GLFix(v), 0);
    };
    VERTEX A = V(-2400, -2400, 25, 12, 4), B = V(2400, -2400, 25, 52, 4);
    VERTEX C = V(2400, -2400, 120, 52, 51.5f), D = V(-2400, -2400, 120, 12, 51.5f);

    nglDrawTriangle(&A, &B, &C, false);
    nglDrawTriangle(&A, &C, &D, false);

    // Reference: project the vertices with the engine's own nglPerspective, then
    // interpolate perspective-correctly in screen space.
    Tri tris[2] = {{{A, B, C}}, {{A, C, D}}};
    double sum_err = 0, max_err = 0;
    long checked = 0, bad = 0, missing = 0;
    for(const Tri &t : tris)
    {
        VERTEX p[3];
        for(int i = 0; i < 3; ++i) { p[i] = t.v[i]; nglPerspective(&p[i]); }
        for(int py = 0; py < SCREEN_HEIGHT; ++py)
            for(int px = 0; px < SCREEN_WIDTH; ++px)
            {
                const double x0 = p[0].x.toFloat(), y0 = p[0].y.toFloat();
                const double x1 = p[1].x.toFloat(), y1 = p[1].y.toFloat();
                const double x2 = p[2].x.toFloat(), y2 = p[2].y.toFloat();
                const double den = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2);
                if(std::fabs(den) < 1e-9) continue;
                const double l0 = ((y1 - y2) * (px - x2) + (x2 - x1) * (py - y2)) / den;
                const double l1 = ((y2 - y0) * (px - x2) + (x0 - x2) * (py - y2)) / den;
                const double l2 = 1 - l0 - l1;
                const double eps = 1e-4;
                if(l0 < -eps || l1 < -eps || l2 < -eps) continue;

                // Perspective-correct: interpolate u/z, v/z and 1/z linearly on screen.
                const double iz[3] = {1.0 / t.v[0].z.toFloat(), 1.0 / t.v[1].z.toFloat(), 1.0 / t.v[2].z.toFloat()};
                const double l[3] = {l0, l1, l2};
                double inv = 0, ou = 0, ov = 0;
                for(int i = 0; i < 3; ++i)
                {
                    inv += l[i] * iz[i];
                    ou  += l[i] * t.v[i].u.toFloat() * iz[i];
                    ov  += l[i] * t.v[i].v.toFloat() * iz[i];
                }
                const double u_ref = ou / inv, v_ref = ov / inv;

                const COLOR c = fb[py * SCREEN_WIDTH + px];
                if(c == 0) { ++missing; continue; }
                const int xd = (((c >> 11) & 1) << 5) | (c & 31);
                const int yd = (c >> 5) & 63;
                const double eu = std::fabs(xd - std::floor(u_ref));
                const double ev = std::fabs(yd - std::floor(v_ref));
                const double e = std::max(eu, ev);
                ++checked;
                sum_err += e;
                if(e > max_err) max_err = e;
                if(e > 1.0) ++bad;
            }
    }
    std::printf("checked=%ld mean_err=%.3f texels max_err=%.1f texels bad(>1 texel)=%.2f%% missing_in_tri=%ld\n",
                checked, checked ? sum_err / checked : 0.0, max_err,
                checked ? 100.0 * bad / checked : 0.0, missing);
    return 0;
}
