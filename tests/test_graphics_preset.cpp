#include "doctest.h"
#include "graphics_preset.h"

TEST_CASE("Normal graphics preset selects the default renderer and distance") {
    const GraphicsPreset p = graphicsPreset(GRAPHICS_NORMAL);
    CHECK(p.fast_mode == 0);
    CHECK(p.distance == 3);
}

TEST_CASE("Low graphics preset enables Fast mode and retains the current CX radius cap") {
    const GraphicsPreset p = graphicsPreset(GRAPHICS_LOW);
    CHECK(p.fast_mode == 1);
    CHECK(p.distance == 3);
}

TEST_CASE("an unknown graphics preset safely falls back to Normal") {
    const GraphicsPreset p = graphicsPreset(GRAPHICS_CHOICE_COUNT + 5);
    const GraphicsPreset normal = graphicsPreset(GRAPHICS_NORMAL);
    CHECK(p.fast_mode == normal.fast_mode);
    CHECK(p.distance == normal.distance);
}
