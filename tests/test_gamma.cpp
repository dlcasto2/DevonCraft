#include "doctest.h"
#include "gamma.h"

TEST_CASE("100 percent gamma leaves RGB565 values unchanged")
{
    GammaTable gamma;
    CHECK(gamma.getPercent() == 100);
    CHECK(gamma.apply(0x0000) == 0x0000);
    CHECK(gamma.apply(0x7BEF) == 0x7BEF);
    CHECK(gamma.apply(0xFFFF) == 0xFFFF);
}

TEST_CASE("gamma above 100 percent brightens midtones without changing endpoints")
{
    GammaTable gamma;
    gamma.set(200);
    CHECK(gamma.getPercent() == 200);
    CHECK(gamma.apply(0x0000) == 0x0000);
    CHECK(gamma.apply(0x7BEF) > 0x7BEF);
    CHECK(gamma.apply(0xFFFF) == 0xFFFF);
}

TEST_CASE("gamma below 100 percent darkens midtones without changing endpoints")
{
    GammaTable gamma;
    gamma.set(50);
    CHECK(gamma.getPercent() == 50);
    CHECK(gamma.apply(0x0000) == 0x0000);
    CHECK(gamma.apply(0x7BEF) < 0x7BEF);
    CHECK(gamma.apply(0xFFFF) == 0xFFFF);
}

TEST_CASE("gamma percentage is clamped to the supported 50 to 200 range")
{
    GammaTable low, high;
    low.set(0);
    high.set(500);
    CHECK(low.getPercent() == 50);
    CHECK(high.getPercent() == 200);
}
