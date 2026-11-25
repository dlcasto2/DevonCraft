#ifndef VILLAGE_H
#define VILLAGE_H

#include <cstdint>

#include "biome.h"

// Villages occupy one cell in a deterministic 40-block grid. The small
// jitter keeps the layout natural while preserving a guaranteed gap between
// neighbouring settlement footprints.
constexpr int VILLAGE_CELL_SIZE = 40;

// Villages can level this much natural height variation with their foundations.
constexpr int VILLAGE_MAX_TERRAIN_VARIATION = 8;
constexpr bool villageTerrainVariationAllowed(int minimum, int maximum)
{
    return maximum >= minimum && maximum - minimum <= VILLAGE_MAX_TERRAIN_VARIATION;
}

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

// Rejects sites whose house or well footprints are wet or too uneven to level.
// Water elsewhere in the village is fine: farm and road tiles that land in
// water are skipped by the generator.
bool villageSiteSuitable(const PerlinNoise &noise, const VillageSite &site);

// Whether the column (x, z) lies in the footprint of a village that could be
// built at any candidate site nearby. Trees keep out of these columns, so a
// tree from a neighbouring chunk cannot grow into a house.
bool villageReserved(uint32_t seed, int x, int z);

#endif // VILLAGE_H
