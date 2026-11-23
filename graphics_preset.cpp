#include "graphics_preset.h"

namespace {

// Keep the Normal baseline aligned with a fresh world's default draw distance.
// Low mode is capped at radius 3 on CX and radius 2 on the monochrome CAS by
// World::setPosition; choosing 3 here preserves the current CX Low preset.
constexpr unsigned int NORMAL_DISTANCE = 3;
constexpr unsigned int LOW_DISTANCE = 3;

} // namespace

GraphicsPreset graphicsPreset(unsigned int choice)
{
    if(choice == GRAPHICS_LOW)
        return GraphicsPreset{ 1, LOW_DISTANCE };
    return GraphicsPreset{ 0, NORMAL_DISTANCE };
}
