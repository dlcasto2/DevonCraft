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
