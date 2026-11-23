#include "doctest.h"
#include "gl.h"

TEST_CASE("GLFix multiplies whole numbers") {
    CHECK((Fix<8, int32_t>(3) * Fix<8, int32_t>(2)).floor() == 6);
}

TEST_CASE("negative fixed-point scaling and division avoid signed left shifts") {
    typedef Fix<8, int32_t> TestFix;
    const TestFix negative(-2);
    const TestFix positive(3);
    CHECK(negative.toInteger<int>() == -2);
    CHECK((negative / positive) < TestFix(0));
    CHECK((negative - -1).toInteger<int>() == -1);
    CHECK((negative << 2).toInteger<int>() == -8);
    TestFix from_integer;
    from_integer.fromInteger(-3);
    CHECK(from_integer.toInteger<int>() == -3);
    const Fix<4, int32_t> low_precision(-2);
    const TestFix converted(low_precision);
    CHECK(converted.toInteger<int>() == -2);
}

TEST_CASE("perspective-correct texture coordinates compensate for depth") {
    const GLFix near_depth(256), far_depth(1024);
    const TexFix q_near = perspectiveReciprocalDepth(near_depth, near_depth);
    const TexFix q_far = perspectiveReciprocalDepth(far_depth, near_depth);
    const TexFix halfway(0.5f);

    CHECK(q_near > q_far);
    const TexFix q_mid = perspectiveLerp(q_near, q_far, halfway);
    const TexFix u_mid_over_z = perspectiveLerp(
        perspectiveProduct(GLFix(0), q_near),
        perspectiveProduct(GLFix(100), q_far), halfway);
    const GLFix u_mid = perspectiveRecoverCoordinate(u_mid_over_z, q_mid);

    CHECK(u_mid.toInteger<int>() == 20);
    CHECK(u_mid.toInteger<int>() != 50); // 50 would be affine screen-space mapping.
}
