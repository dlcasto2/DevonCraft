#include "village.h"

#include <cstdlib>
#include <initializer_list>

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

namespace {

// A square of columns a village building occupies: its centre and half sizes.
struct Footprint
{
    int x, z, half_x, half_z;
};

constexpr int MAX_LEVELLED_FOOTPRINTS = 5;
constexpr int MAX_RESERVED_FOOTPRINTS = 6;

int floorDiv(int value, int divisor)
{
    int quotient = value / divisor;
    if(value % divisor < 0)
        --quotient;
    return quotient;
}

// Cache the result of checking a candidate cell. Tree placement asks about
// every block in a tree shape, so recomputing the five foundation footprints
// for each block would be wasteful. A small direct-mapped cache keeps this
// check bounded and also caches cells that do not contain a village.
struct VillageSiteCacheEntry
{
    uint32_t seed;
    int cell_x, cell_z;
    VillageSite site;
    bool valid;
    bool suitable;
};

constexpr unsigned int VILLAGE_SITE_CACHE_SIZE = 64;
static VillageSiteCacheEntry village_site_cache[VILLAGE_SITE_CACHE_SIZE] = {};

bool cachedSuitableVillageSite(const PerlinNoise &noise, int cell_x, int cell_z,
                               uint32_t seed, VillageSite &site)
{
    uint32_t hash = seed;
    hash ^= static_cast<uint32_t>(cell_x) * 0x9E3779B1u;
    hash ^= static_cast<uint32_t>(cell_z) * 0x85EBCA77u;
    VillageSiteCacheEntry &entry = village_site_cache[hash & (VILLAGE_SITE_CACHE_SIZE - 1)];

    if(!entry.valid || entry.seed != seed || entry.cell_x != cell_x || entry.cell_z != cell_z)
    {
        entry.seed = seed;
        entry.cell_x = cell_x;
        entry.cell_z = cell_z;
        entry.valid = true;
        entry.suitable = villageSiteForCell(cell_x, cell_z, seed, entry.site)
                      && villageSiteSuitable(noise, entry.site);
    }

    if(entry.suitable)
        site = entry.site;
    return entry.suitable;
}

// The four houses (the blacksmith is one of them) and the well, as villagegen
// levels them before it builds. Returns how many it wrote.
int levelledFootprints(const VillageSite &site, Footprint *out)
{
    int n = 0;
    for(int dx : {-8, 8})
        for(int dz : {-8, 8})
            out[n++] = Footprint{site.x + dx, site.z + dz, 2, 2};
    out[n++] = Footprint{site.x, site.z, 1, 1};
    return n;
}

// Every column a village building may occupy: the levelled footprints, the
// well's 5x5 roof and the farm 14 blocks to the south.
int reservedFootprints(const VillageSite &site, Footprint *out)
{
    int n = 0;
    for(int dx : {-8, 8})
        for(int dz : {-8, 8})
            out[n++] = Footprint{site.x + dx, site.z + dz, 2, 2};
    out[n++] = Footprint{site.x, site.z, 2, 2};
    out[n++] = Footprint{site.x, site.z + 14, 3, 2};
    return n;
}

} // namespace

bool villageSiteSuitable(const PerlinNoise &noise, const VillageSite &site)
{
    if(!villageBiomeAllowed(biomeAt(noise, site.x, site.z)))
        return false;

    Footprint footprints[MAX_LEVELLED_FOOTPRINTS];
    const int count = levelledFootprints(site, footprints);
    for(int i = 0; i < count; ++i)
    {
        const Footprint &f = footprints[i];
        int minimum = MAX_GROUND;
        int maximum = 0;
        for(int dx = -f.half_x; dx <= f.half_x; ++dx)
            for(int dz = -f.half_z; dz <= f.half_z; ++dz)
            {
                const int height = groundHeight(noise, f.x + dx, f.z + dz);
                // groundHeight is the first air block; the block below it is soil.
                if(height <= SEA_LEVEL + 1)
                    return false;
                if(height < minimum) minimum = height;
                if(height > maximum) maximum = height;
            }

        if(!villageTerrainVariationAllowed(minimum, maximum) || maximum > MAX_GROUND)
            return false;
    }
    return true;
}

bool villageReserved(const PerlinNoise &noise, uint32_t seed, int x, int z)
{
    // Every reserved footprint lies within this many columns of its site.
    constexpr int REACH = 24;
    for(int cell_x = floorDiv(x - REACH, VILLAGE_CELL_SIZE); cell_x <= floorDiv(x + REACH, VILLAGE_CELL_SIZE); ++cell_x)
        for(int cell_z = floorDiv(z - REACH, VILLAGE_CELL_SIZE); cell_z <= floorDiv(z + REACH, VILLAGE_CELL_SIZE); ++cell_z)
        {
            VillageSite site;
            if(!cachedSuitableVillageSite(noise, cell_x, cell_z, seed, site))
                continue;

            Footprint footprints[MAX_RESERVED_FOOTPRINTS];
            const int count = reservedFootprints(site, footprints);
            for(int i = 0; i < count; ++i)
            {
                const Footprint &f = footprints[i];
                if(std::abs(x - f.x) <= f.half_x && std::abs(z - f.z) <= f.half_z)
                    return true;
            }
        }
    return false;
}
