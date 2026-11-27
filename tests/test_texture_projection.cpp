#include <cmath>
#include "doctest.h"
#include "gl.h"

TEST_CASE("aligned mesh vertices receive perspective texture coordinates")
{
    VERTEX vertex{0, 0, GLFix(512), GLFix(40), GLFix(24), 0xFFFF};

    nglSetPerspectiveTextureAttributes(vertex, GLFix(256));

    CHECK(vertex.tex_q.value == TexFix(0.5f).value);
    CHECK(vertex.u_over_z.value == TexFix(20).value);
    CHECK(vertex.v_over_z.value == TexFix(12).value);
}

TEST_CASE("perspective texture coordinates shrink with depth")
{
    VERTEX near_vertex{0, 0, GLFix(256), GLFix(32), GLFix(16), 0xFFFF};
    VERTEX far_vertex{0, 0, GLFix(512), GLFix(32), GLFix(16), 0xFFFF};

    nglSetPerspectiveTextureAttributes(near_vertex, GLFix(256));
    nglSetPerspectiveTextureAttributes(far_vertex, GLFix(256));

    CHECK(near_vertex.u_over_z.value == TexFix(32).value);
    CHECK(far_vertex.u_over_z.value == TexFix(16).value);
    CHECK(near_vertex.v_over_z.value == TexFix(16).value);
    CHECK(far_vertex.v_over_z.value == TexFix(8).value);
}

TEST_CASE("near-plane clipping trims triangles before projection and preserves UV precision")
{
    const GLFix near_depth(CLIP_PLANE);
    const VERTEX triangle[3] = {
        VERTEX{GLFix(0), GLFix(0), GLFix(10), GLFix(0), GLFix(0), 0xFFFF},
        VERTEX{GLFix(20), GLFix(0), GLFix(50), GLFix(40), GLFix(0), 0xFFFF},
        VERTEX{GLFix(0), GLFix(20), GLFix(50), GLFix(0), GLFix(40), 0xFFFF}
    };
    VERTEX clipped[4];

    const unsigned int count = nglClipTriangleToNearPlane(triangle, clipped, near_depth);

    CHECK(count == 4);
    for(unsigned int i = 0; i < count; ++i)
        CHECK(clipped[i].z >= near_depth);

    // Both new edge intersections lie halfway between texel coordinates;
    // they must not be truncated to whole UV units during clipping.
    CHECK(clipped[0].y == GLFix(7.5f));
    CHECK(clipped[0].v == GLFix(15.0f));
    CHECK(clipped[1].x == GLFix(7.5f));
    CHECK(clipped[1].u == GLFix(15.0f));
}

TEST_CASE("near-plane clipping handles two vertices behind the camera")
{
    const GLFix near_depth(CLIP_PLANE);
    const VERTEX triangle[3] = {
        VERTEX{GLFix(-8), GLFix(0), GLFix(10), GLFix(0), GLFix(0), 0xFFFF},
        VERTEX{GLFix(16), GLFix(0), GLFix(20), GLFix(32), GLFix(0), 0xFFFF},
        VERTEX{GLFix(0), GLFix(24), GLFix(50), GLFix(16), GLFix(32), 0xFFFF}
    };
    VERTEX clipped[4];

    const unsigned int count = nglClipTriangleToNearPlane(triangle, clipped, near_depth);

    CHECK(count == 3);
    unsigned int intersections_on_plane = 0;
    for(unsigned int i = 0; i < count; ++i)
    {
        CHECK(clipped[i].z >= near_depth);
        if(clipped[i].z == near_depth)
            ++intersections_on_plane;
    }
    CHECK(intersections_on_plane == 2);
}

TEST_CASE("near-plane clipping rejects fully hidden triangles and preserves visible ones")
{
    const GLFix near_depth(CLIP_PLANE);
    const VERTEX hidden[3] = {
        VERTEX{0, 0, GLFix(5), 0, 0, 0xFFFF},
        VERTEX{8, 0, GLFix(10), 8, 0, 0xFFFF},
        VERTEX{0, 8, GLFix(15), 0, 8, 0xFFFF}
    };
    const VERTEX visible[3] = {
        VERTEX{0, 0, GLFix(30), 0, 0, 0xFFFF},
        VERTEX{8, 0, GLFix(40), 8, 0, 0xFFFF},
        VERTEX{0, 8, GLFix(50), 0, 8, 0xFFFF}
    };
    VERTEX clipped[4];

    CHECK(nglClipTriangleToNearPlane(hidden, clipped, near_depth) == 0);
    CHECK(nglClipTriangleToNearPlane(visible, clipped, near_depth) == 3);
    CHECK(clipped[0].x == visible[0].x);
    CHECK(clipped[1].y == visible[1].y);
    CHECK(clipped[2].z == visible[2].z);
}

TEST_CASE("near-plane clipping avoids duplicate vertices on the clipping boundary")
{
    const GLFix near_depth(CLIP_PLANE);
    const VERTEX triangle[3] = {
        VERTEX{0, 0, near_depth, 0, 0, 0xFFFF},
        VERTEX{12, 0, GLFix(10), 12, 0, 0xFFFF},
        VERTEX{0, 12, GLFix(40), 0, 12, 0xFFFF}
    };
    VERTEX clipped[4];

    const unsigned int count = nglClipTriangleToNearPlane(triangle, clipped, near_depth);

    CHECK(count == 3);
    for(unsigned int i = 0; i < count; ++i)
        CHECK(clipped[i].z >= near_depth);
}

TEST_CASE("clipped quad preserves winding for back-face culling")
{
    const GLFix near_depth(CLIP_PLANE);
    // One vertex is behind the near plane. The other two are visible, so
    // clipping produces a quad. Its first three projected vertices must keep
    // the source triangle's winding, allowing the normal back-face test.
    const VERTEX back_facing[3] = {
        VERTEX{GLFix(0), GLFix(0), GLFix(10), GLFix(0), GLFix(0), 0xFFFF},
        VERTEX{GLFix(20), GLFix(0), GLFix(50), GLFix(32), GLFix(0), 0xFFFF},
        VERTEX{GLFix(0), GLFix(20), GLFix(50), GLFix(0), GLFix(32), 0xFFFF}
    };
    VERTEX clipped[4];

    const unsigned int count = nglClipTriangleToNearPlane(back_facing, clipped, near_depth);
    REQUIRE(count == 4);

    const auto projectX = [](const VERTEX &vertex) {
        return SCREEN_WIDTH / 2.0 + vertex.x.toFloat() * 256.0 / vertex.z.toFloat();
    };
    const auto projectY = [](const VERTEX &vertex) {
        return SCREEN_HEIGHT - 1.0
            - (SCREEN_HEIGHT / 2.0 + vertex.y.toFloat() * 256.0 / vertex.z.toFloat());
    };

    const double x0 = projectX(clipped[0]), y0 = projectY(clipped[0]);
    const double x1 = projectX(clipped[1]), y1 = projectY(clipped[1]);
    const double x2 = projectX(clipped[2]), y2 = projectY(clipped[2]);
    const double signed_area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0);

    // nglIsBackface returns true for this negative projected winding.
    CHECK(signed_area < 0.0);
}

TEST_CASE("reciprocal depth keeps precision at the clip distance")
{
    // Q10 stored near/z = 0.04 with ~2.5% error, which skewed UVs on every floor.
    VERTEX vertex{0, 0, GLFix(25), GLFix(0), GLFix(0), 0xFFFF};
    nglSetPerspectiveTextureAttributes(vertex, GLFix(1));

    CHECK(std::fabs(vertex.tex_q.toFloat() - 0.04f) < 1e-4f);
}

TEST_CASE("per-row texture coordinate steps do not truncate to zero across a floor")
{
    // Regression: with Q10 accumulators, tex_q fell by about 31 Q10 steps over
    // about 175 rows, so every row's delta integer-divided down to zero and
    // texture coordinates stopped tracking depth.
    VERTEX near_vertex{0, 0, GLFix(25), GLFix(0), GLFix(0), 0xFFFF};
    VERTEX far_vertex{0, 0, GLFix(120), GLFix(0), GLFix(0), 0xFFFF};
    nglSetPerspectiveTextureAttributes(near_vertex, GLFix(1));
    nglSetPerspectiveTextureAttributes(far_vertex, GLFix(1));

    const TexAcc per_row = TexAcc(far_vertex.tex_q - near_vertex.tex_q) / 175;
    CHECK(per_row.value != 0);
}

TEST_CASE("texel coordinates divide in full precision")
{
    // u = 40 texels at tex_q 0.5 gives uq = 20; dividing by 0.5 must return 40.
    const TexAcc uq = TexAcc(TexFix(20));
    const TexAcc q = TexAcc(TexFix(0.5f));
    const TexAcc u = texelOver(uq, q);
    CHECK(std::fabs(u.toFloat() - 40.0f) < 1e-3f);
}
