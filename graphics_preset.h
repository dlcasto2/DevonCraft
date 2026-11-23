#ifndef GRAPHICS_PRESET_H
#define GRAPHICS_PRESET_H

// Combined Options preset. The underlying settings remain independently editable.
enum GraphicsChoice : unsigned int {
    GRAPHICS_NORMAL = 0,
    GRAPHICS_LOW,
    GRAPHICS_CHOICE_COUNT
};

struct GraphicsPreset {
    unsigned int fast_mode;
    unsigned int distance;
};

GraphicsPreset graphicsPreset(unsigned int choice);

#endif // GRAPHICS_PRESET_H
