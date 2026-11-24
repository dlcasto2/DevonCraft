#include "village.h"

#include "water.h"

namespace {

constexpr uint32_t SALT_VILLAGE = 0x71A6E5u;
constexpr uint32_t VILLAGE_CHANCE_PERMILLE = 600;
constexpr int VILLAGE_JITTER = 2;

} // namespace

bool villageSiteForCell(int cell_x, int cell_z, uint32_t seed, VillageSite &site)
{
    const uint32_t hash = columnHash(cell_x, cell_z, seed, SALT_VILLAGE);
    if(hash % 1000u >= VILLAGE_CHANCE_PERMILLE)
        return false;

    site.x = cell_x * VILLAGE_CELL_SIZE + VILLAGE_CELL_SIZE / 2
           + static_cast<int>((hash >> 8) % (VILLAGE_JITTER * 2 + 1)) - VILLAGE_JITTER;
    site.z = cell_z * VILLAGE_CELL_SIZE + VILLAGE_CELL_SIZE / 2
           + static_cast<int>((hash >> 16) % (VILLAGE_JITTER * 2 + 1)) - VILLAGE_JITTER;
    return true;
}

bool villageBiomeAllowed(Biome biome)
{
    switch(biome)
    {
    case Biome::PLAINS:
    case Biome::FOREST:
    case Biome::BIRCH_FOREST:
    case Biome::TAIGA:
    case Biome::ICE_PLAINS:
    case Biome::DESERT:
        return true;
    default:
        return false;
    }
}

bool villageSiteSuitable(const PerlinNoise &noise, const VillageSite &site)
{
    if(!villageBiomeAllowed(biomeAt(noise, site.x, site.z)))
        return false;

    int minimum = MAX_GROUND;
    int maximum = 0;
    for(int dx = -16; dx <= 16; dx += 8)
        for(int dz = -16; dz <= 16; dz += 8)
        {
            const int height = groundHeight(noise, site.x + dx, site.z + dz);
            // groundHeight is the first air block; the block below it is soil.
            if(height <= SEA_LEVEL + 1)
                return false;
            if(height < minimum) minimum = height;
            if(height > maximum) maximum = height;
        }

    // A settlement needs a broadly level clearing, not a cliff or shoreline.
    return maximum - minimum <= 4 && maximum <= MAX_GROUND;
}
