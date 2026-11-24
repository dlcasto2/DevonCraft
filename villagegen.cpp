#include "villagegen.h"

#include <algorithm>
#include <cstdlib>

#include "chunk.h"
#include "containers.h"
#include "terrain.h"
#include "water.h"
#include "world.h"
#include "village.h"

namespace {

struct Palette
{
    BLOCK log;
    BLOCK wall;
    BLOCK floor;
    BLOCK roof;
    BLOCK foundation;
    BLOCK road;
};

Palette paletteFor(Biome biome)
{
    if(biome == Biome::DESERT)
        return Palette{BLOCK_WOOD, BLOCK_SANDSTONE, BLOCK_SANDSTONE,
                       BLOCK_SANDSTONE, BLOCK_SANDSTONE, BLOCK_SANDSTONE};
    if(biome == Biome::TAIGA || biome == Biome::ICE_PLAINS)
        return Palette{BLOCK_SPRUCE_LOG, BLOCK_PLANKS_DARK, BLOCK_PLANKS_DARK,
                       BLOCK_PLANKS_DARK, BLOCK_COBBLESTONE, BLOCK_COBBLESTONE};
    if(biome == Biome::BIRCH_FOREST)
        return Palette{BLOCK_BIRCH_LOG, BLOCK_PLANKS_NORMAL, BLOCK_PLANKS_NORMAL,
                       BLOCK_PLANKS_DARK, BLOCK_COBBLESTONE, BLOCK_COBBLESTONE};
    return Palette{BLOCK_WOOD, BLOCK_PLANKS_NORMAL, BLOCK_PLANKS_NORMAL,
                   BLOCK_PLANKS_DARK, BLOCK_COBBLESTONE, BLOCK_COBBLESTONE};
}

int floorDiv(int value, int divisor)
{
    int quotient = value / divisor;
    if(value % divisor < 0)
        --quotient;
    return quotient;
}

int surfaceY(const PerlinNoise &noise, int x, int z)
{
    return groundHeight(noise, x, z) - 1;
}

void put(Chunk &chunk, int x, int y, int z, BLOCK_WDATA block)
{
    if(y < 0 || y >= World::HEIGHT * Chunk::SIZE)
        return;

    chunk.setGlobalBlockRelative(x - chunk.x * Chunk::SIZE,
                                 y - chunk.y * Chunk::SIZE,
                                 z - chunk.z * Chunk::SIZE,
                                 block);
}

bool platformLevel(const PerlinNoise &noise, int center_x, int center_z,
                   int half_x, int half_z, int &floor_y)
{
    int minimum = World::HEIGHT * Chunk::SIZE;
    int maximum = -1;
    for(int dx = -half_x; dx <= half_x; ++dx)
        for(int dz = -half_z; dz <= half_z; ++dz)
        {
            const int top = surfaceY(noise, center_x + dx, center_z + dz);
            if(top <= SEA_LEVEL || top < 0)
                return false;
            minimum = std::min(minimum, top);
            maximum = std::max(maximum, top);
        }

    floor_y = maximum;
    return maximum - minimum <= 4 && floor_y + 6 < World::HEIGHT * Chunk::SIZE;
}

void preparePlatform(Chunk &chunk, const PerlinNoise &noise,
                     int center_x, int center_z, int half_x, int half_z,
                     int floor_y, BLOCK foundation, BLOCK floor)
{
    for(int dx = -half_x; dx <= half_x; ++dx)
        for(int dz = -half_z; dz <= half_z; ++dz)
        {
            const int x = center_x + dx, z = center_z + dz;
            const int top = surfaceY(noise, x, z);
            for(int y = top + 1; y < floor_y; ++y)
                put(chunk, x, y, z, foundation);
            put(chunk, x, floor_y, z, floor);
        }
}

void clearInterior(Chunk &chunk, const PerlinNoise &noise,
                   int x, int z, int floor_y, int height)
{
    const int top = surfaceY(noise, x, z);
    for(int y = std::max(top + 1, floor_y + 1); y <= floor_y + height; ++y)
        put(chunk, x, y, z, BLOCK_AIR);
}

bool buildHouse(Chunk &chunk, const PerlinNoise &noise, const Palette &palette,
                int center_x, int center_z)
{
    int floor_y;
    if(!platformLevel(noise, center_x, center_z, 2, 2, floor_y))
        return false;

    preparePlatform(chunk, noise, center_x, center_z, 2, 2,
                    floor_y, palette.foundation, palette.floor);

    // Clear the full footprint above the ground first. This removes trees
    // even when an adjacent chunk was generated before the village anchor.
    for(int dx = -2; dx <= 2; ++dx)
        for(int dz = -2; dz <= 2; ++dz)
            clearInterior(chunk, noise, center_x + dx, center_z + dz, floor_y, 6);

    for(int dx = -2; dx <= 2; ++dx)
        for(int dz = -2; dz <= 2; ++dz)
        {
            const bool boundary = std::abs(dx) == 2 || std::abs(dz) == 2;
            if(!boundary)
                continue;

            const bool corner = std::abs(dx) == 2 && std::abs(dz) == 2;
            const bool doorway = dx == 0 && dz == 2;
            for(int h = 1; h <= 3; ++h)
            {
                if(doorway && h <= 2)
                    continue;

                BLOCK block = palette.wall;
                if(corner)
                    block = palette.log;
                else if(h == 2 && ((std::abs(dx) == 2 && dz == 0)
                                   || (dz == -2 && dx == 0)))
                    block = BLOCK_GLASS;

                put(chunk, center_x + dx, floor_y + h, center_z + dz, block);
            }
        }

    // A south-facing two-block door.
    put(chunk, center_x, floor_y + 1, center_z + 2,
        getBLOCKWDATA(BLOCK_DOOR, BLOCK_FRONT));
    put(chunk, center_x, floor_y + 2, center_z + 2,
        getBLOCKWDATA(BLOCK_DOOR, BLOCK_FRONT | 8));

    // Low, stepped roofs fit the compact calculator-scale village layout.
    for(int dx = -2; dx <= 2; ++dx)
        for(int dz = -2; dz <= 2; ++dz)
            put(chunk, center_x + dx, floor_y + 4, center_z + dz, palette.roof);
    for(int dx = -1; dx <= 1; ++dx)
        for(int dz = -1; dz <= 1; ++dz)
            put(chunk, center_x + dx, floor_y + 5, center_z + dz, palette.roof);

    // Basic furnishings: a usable bed, crafting table and furnace.
    put(chunk, center_x - 1, floor_y + 1, center_z - 1,
        getBLOCKWDATA(BLOCK_BED, bedData(BLOCK_FRONT, false)));
    put(chunk, center_x - 1, floor_y + 1, center_z,
        getBLOCKWDATA(BLOCK_BED, bedData(BLOCK_FRONT, true)));
    put(chunk, center_x + 1, floor_y + 1, center_z - 1,
        getBLOCKWDATA(BLOCK_CRAFTING_TABLE, BLOCK_FRONT));
    put(chunk, center_x + 1, floor_y + 1, center_z,
        getBLOCKWDATA(BLOCK_FURNACE, BLOCK_FRONT));
    return true;
}

bool buildBlacksmith(Chunk &chunk, const PerlinNoise &noise,
                    const Palette &palette, int center_x, int center_z)
{
    int floor_y;
    if(!platformLevel(noise, center_x, center_z, 2, 2, floor_y))
        return false;

    preparePlatform(chunk, noise, center_x, center_z, 2, 2,
                    floor_y, palette.foundation, palette.floor);

    // Clear the footprint first so the forge replaces any natural trees.
    for(int dx = -2; dx <= 2; ++dx)
        for(int dz = -2; dz <= 2; ++dz)
            clearInterior(chunk, noise, center_x + dx, center_z + dz, floor_y, 6);

    // Keep the same compact house footprint as the other village buildings.
    // The front (south) doorway stays open for the two-block door.
    for(int dx = -2; dx <= 2; ++dx)
        for(int dz = -2; dz <= 2; ++dz)
        {
            const bool boundary = std::abs(dx) == 2 || std::abs(dz) == 2;
            if(!boundary)
                continue;

            const bool corner = std::abs(dx) == 2 && std::abs(dz) == 2;
            const bool doorway = dx == 0 && dz == 2;
            for(int h = 1; h <= 3; ++h)
            {
                if(doorway && h <= 2)
                    continue;

                BLOCK block = palette.wall;
                if(corner)
                    block = palette.log;
                else if(h == 2 && ((std::abs(dx) == 2 && dz == 0)
                                   || (dz == -2 && dx == 0)))
                    block = BLOCK_GLASS;

                put(chunk, center_x + dx, floor_y + h, center_z + dz, block);
            }
        }

    put(chunk, center_x, floor_y + 1, center_z + 2,
        getBLOCKWDATA(BLOCK_DOOR, BLOCK_FRONT));
    put(chunk, center_x, floor_y + 2, center_z + 2,
        getBLOCKWDATA(BLOCK_DOOR, BLOCK_FRONT | 8));

    // A stepped roof and a stone chimney make the workshop easy to identify.
    for(int dx = -2; dx <= 2; ++dx)
        for(int dz = -2; dz <= 2; ++dz)
            put(chunk, center_x + dx, floor_y + 4, center_z + dz, palette.roof);
    for(int dx = -1; dx <= 1; ++dx)
        for(int dz = -1; dz <= 1; ++dz)
            put(chunk, center_x + dx, floor_y + 5, center_z + dz, palette.roof);
    for(int h = 4; h <= 6; ++h)
        put(chunk, center_x - 1, floor_y + h, center_z - 1, BLOCK_COBBLESTONE);

    // Functional workshop stations: two furnaces, a smithing table and
    // a crafting table. The iron block acts as a visible metal work surface.
    put(chunk, center_x, floor_y + 1, center_z - 1,
        getBLOCKWDATA(BLOCK_FURNACE, BLOCK_FRONT));
    put(chunk, center_x - 1, floor_y + 1, center_z,
        getBLOCKWDATA(BLOCK_FURNACE, BLOCK_FRONT));
    put(chunk, center_x + 1, floor_y + 1, center_z,
        getBLOCKWDATA(BLOCK_SMITHING_TABLE, BLOCK_FRONT));
    put(chunk, center_x + 1, floor_y + 1, center_z - 1,
        getBLOCKWDATA(BLOCK_CRAFTING_TABLE, BLOCK_FRONT));
    put(chunk, center_x - 1, floor_y + 1, center_z - 1, BLOCK_IRON);
    return true;
}

void roadTile(Chunk &chunk, const PerlinNoise &noise, const Palette &palette,
              int x, int z)
{
    const int y = surfaceY(noise, x, z);
    if(y <= SEA_LEVEL)
        return;
    for(int clear_y = 1; clear_y <= 4; ++clear_y)
        put(chunk, x, y + clear_y, z, BLOCK_AIR);
    put(chunk, x, y, z, palette.road);
}

void buildRoads(Chunk &chunk, const PerlinNoise &noise, const Palette &palette,
                const VillageSite &site)
{
    // Four paths run from the houses towards the central well.
    for(int side_x : {-8, 8})
    {
        for(int z = -6; z <= -2; ++z)
            roadTile(chunk, noise, palette, site.x + side_x, site.z + z);
        for(int x = std::min(side_x, 0); x <= std::max(side_x, 0); ++x)
            roadTile(chunk, noise, palette, site.x + x, site.z - 2);

        for(int z = 2; z <= 10; ++z)
            roadTile(chunk, noise, palette, site.x + side_x, site.z + z);
        for(int x = std::min(side_x, 0); x <= std::max(side_x, 0); ++x)
            roadTile(chunk, noise, palette, site.x + x, site.z + 2);
    }
}

void buildWell(Chunk &chunk, const PerlinNoise &noise, const Palette &palette,
               const VillageSite &site)
{
    int floor_y;
    if(!platformLevel(noise, site.x, site.z, 1, 1, floor_y))
        return;

    preparePlatform(chunk, noise, site.x, site.z, 1, 1,
                    floor_y, palette.foundation, BLOCK_COBBLESTONE);

    for(int dx = -1; dx <= 1; ++dx)
        for(int dz = -1; dz <= 1; ++dz)
            clearInterior(chunk, noise, site.x + dx, site.z + dz, floor_y, 4);

    for(int dx = -1; dx <= 1; ++dx)
        for(int dz = -1; dz <= 1; ++dz)
        {
            const bool center = dx == 0 && dz == 0;
            if(center)
                put(chunk, site.x, floor_y + 1, site.z, WATER_SOURCE);
            else
                put(chunk, site.x + dx, floor_y + 1, site.z + dz, BLOCK_COBBLESTONE);
        }

    for(int dx : {-1, 1})
        for(int dz : {-1, 1})
            for(int h = 2; h <= 3; ++h)
                put(chunk, site.x + dx, floor_y + h, site.z + dz, palette.log);

    for(int dx = -2; dx <= 2; ++dx)
        for(int dz = -2; dz <= 2; ++dz)
            put(chunk, site.x + dx, floor_y + 4, site.z + dz, palette.roof);
}

void buildFarm(Chunk &chunk, const PerlinNoise &noise, const Palette &palette,
               const VillageSite &site)
{
    const int farm_x = site.x, farm_z = site.z + 14;
    int floor_y;
    if(!platformLevel(noise, farm_x, farm_z, 3, 2, floor_y))
        return;

    preparePlatform(chunk, noise, farm_x, farm_z, 3, 2,
                    floor_y, palette.foundation, BLOCK_DIRT);

    for(int dx = -3; dx <= 3; ++dx)
        for(int dz = -2; dz <= 2; ++dz)
        {
            const int x = farm_x + dx, z = farm_z + dz;
            const int top = surfaceY(noise, x, z);
            for(int y = std::max(top + 1, floor_y + 1); y <= floor_y + 4; ++y)
                put(chunk, x, y, z, BLOCK_AIR);

            const bool edge = std::abs(dx) == 3 || std::abs(dz) == 2;
            if(edge)
                put(chunk, x, floor_y + 1, z, BLOCK_COBBLESTONE);
            else if(dx == 0 && dz == 0)
                put(chunk, x, floor_y + 1, z, WATER_SOURCE);
            else
                put(chunk, x, floor_y + 1, z, getBLOCKWDATA(BLOCK_WHEAT, 0));
        }
}

void buildVillage(Chunk &chunk, const PerlinNoise &noise,
                  const VillageSite &site, Biome biome)
{
    const Palette palette = paletteFor(biome);

    buildRoads(chunk, noise, palette, site);
    buildWell(chunk, noise, palette, site);
    buildFarm(chunk, noise, palette, site);

    buildBlacksmith(chunk, noise, palette, site.x - 8, site.z - 8);
    buildHouse(chunk, noise, palette, site.x + 8, site.z - 8);
    buildHouse(chunk, noise, palette, site.x - 8, site.z + 8);
    buildHouse(chunk, noise, palette, site.x + 8, site.z + 8);
}

} // namespace

void generateChunkVillages(Chunk &chunk, const PerlinNoise &noise, uint32_t seed)
{
    const int base_x = chunk.x * Chunk::SIZE;
    const int base_y = chunk.y * Chunk::SIZE;
    const int base_z = chunk.z * Chunk::SIZE;

    const int min_cell_x = floorDiv(base_x - 18, VILLAGE_CELL_SIZE);
    const int max_cell_x = floorDiv(base_x + Chunk::SIZE + 18, VILLAGE_CELL_SIZE);
    const int min_cell_z = floorDiv(base_z - 18, VILLAGE_CELL_SIZE);
    const int max_cell_z = floorDiv(base_z + Chunk::SIZE + 18, VILLAGE_CELL_SIZE);

    for(int cell_x = min_cell_x; cell_x <= max_cell_x; ++cell_x)
        for(int cell_z = min_cell_z; cell_z <= max_cell_z; ++cell_z)
        {
            VillageSite site;
            if(!villageSiteForCell(cell_x, cell_z, seed, site))
                continue;

            // One chunk owns the whole settlement plan. This avoids duplicate
            // writes as adjacent chunks enter and leave the loaded area.
            if(floorDiv(site.x, Chunk::SIZE) != chunk.x
               || floorDiv(site.z, Chunk::SIZE) != chunk.z)
                continue;

            if(!villageSiteSuitable(noise, site))
                continue;

            const int ground_y = surfaceY(noise, site.x, site.z);
            if(floorDiv(ground_y, Chunk::SIZE) != chunk.y)
                continue;

            buildVillage(chunk, noise, site, biomeAt(noise, site.x, site.z));
        }
}
