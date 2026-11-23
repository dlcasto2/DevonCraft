#include "doctest.h"
#include "fix.h"

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
