#ifndef LIGHTING_H
#define LIGHTING_H

#include <cstddef>

#include "gl.h"

// Day/night is done by drawing each chunk with a darkened copy of the terrain
// texture, chosen per chunk. Level 7 is full brightness; 0 is darkest.
constexpr int BRIGHTNESS_LEVELS = 8;

// Scale each RGB565 channel by (level + 1) / 8, rounding down. If the result
// lands on the transparent key but the source wasn't transparent, nudge it so
// a dark pixel never turns into a hole.
inline COLOR darkenColor(COLOR c, int level, COLOR transparent)
{
    if(level >= BRIGHTNESS_LEVELS - 1)
        return c;

    const unsigned r = (c >> 11) & 0x1F, g = (c >> 5) & 0x3F, b = c & 0x1F;
    const unsigned scale = static_cast<unsigned>(level + 1);
    const COLOR out = static_cast<COLOR>(((r * scale / 8) << 11) | ((g * scale / 8) << 5) | (b * scale / 8));

    if(out == transparent && c != transparent)
        return 0x0020;   // darkest non-key green

    return out;
}

// Combine a block light value (0-15) with ambient brightness (0-7).
// Attenuation is Manhattan distance after accounting for the target chunk's
// coarse 8-block footprint. Each two light points add one brightness level.
inline int combineDynamicLight(int ambient_level, int emission, int attenuation)
{
    if(ambient_level < 0) ambient_level = 0;
    if(ambient_level >= BRIGHTNESS_LEVELS) ambient_level = BRIGHTNESS_LEVELS - 1;
    if(emission <= attenuation)
        return ambient_level;

    const int remaining = emission - attenuation;
    int lit_level = (remaining + 1) / 2;
    if(lit_level >= BRIGHTNESS_LEVELS)
        lit_level = BRIGHTNESS_LEVELS - 1;
    return lit_level > ambient_level ? lit_level : ambient_level;
}

// Build the darkened copies after terrainInit(). false on allocation failure,
// in which case every level falls back to the originals.
bool lightingInit();
void lightingUninit();

TEXTURE *terrainAtLevel(int level);
TEXTURE *terrainQuadAtLevel(int level);
size_t lightingMemoryBytes();

#endif // LIGHTING_H
