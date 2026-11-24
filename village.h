#ifndef VILLAGE_H
#define VILLAGE_H

#include <cstdint>

#include "biome.h"

// Villages occupy one cell in a deterministic 40-block grid. The small
// jitter keeps the layout natural while preserving a guaranteed gap between
// neighbouring settlement footprints.
constexpr int VILLAGE_CELL_SIZE = 40;

struct VillageSite
{
    int x, z;
};

// Returns the same candidate for a cell and seed every time. Not every cell
// contains a village.
bool villageSiteForCell(int cell_x, int cell_z, uint32_t seed, VillageSite &site);

// Villages only use the temperate and dry overworld biomes supported by the
// current block palette.
bool villageBiomeAllowed(Biome biome);

// Rejects sites with water or sharply changing elevation across the village.
bool villageSiteSuitable(const PerlinNoise &noise, const VillageSite &site);

#endif // VILLAGE_H
