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
