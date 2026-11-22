#include "biome.h"

#include "terrain.h"

namespace {

// Field scales in blocks: biomes are a few hundred blocks across, hills smaller.
constexpr int TEMPERATURE_SCALE = 192, HUMIDITY_SCALE = 144, HILLINESS_SCALE = 96, CRAG_SCALE = 14;

// Slices of the noise that nothing else samples.
const GLFix TEMPERATURE_Z = 50.5f, HUMIDITY_Z = 80.5f, HILLINESS_Z = 120.5f, CRAG_Z = 160.5f;

const GLFix HILLS_START = 0.67f;                     // Extreme Hills from here...
const GLFix HILLS_RAMP_LOW = 0.62f, HILLS_RAMP_HIGH = 0.76f;   // ...rising over this range
const GLFix ICE_BELOW = 0.33f, TAIGA_BELOW = 0.42f, DESERT_FROM = 0.60f;
const GLFix DRY = 0.45f, WET = 0.60f;

GLFix field(const PerlinNoise &noise, int x, int z, int scale, GLFix slice)
{
    return noise.noise(GLFix(x) / scale, GLFix(z) / scale, slice);
}

// the original 1.3 terrain, sampled exactly as Chunk::generate always has.
int originalHeight(const PerlinNoise &noise, int x, int z)
{
    const int cx = x >> 3, cz = z >> 3, lx = x & 7, lz = z & 7;
    const GLFix n = noise.noise((GLFix(lx) / 8 + cx) / 4, (GLFix(lz) / 8 + cz) / 4, 10);
    constexpr int world_gen_min = 8, world_gen_max = 28;   // 70% of the 40-block world
    return world_gen_min + (n * (world_gen_max - world_gen_min)).round();
}

// Forests are about two trees per chunk: twice the original 1.3 rate, and few
// enough leaves for the calculator to draw.
const BiomeFeatures features[static_cast<int>(Biome::COUNT)] = {
    //  trees, birch, spruce, flowers, cacti, bushes, frozen
    {    3,    0,    0,  25,  0,  0, false },  // Plains: open grass, lots of flowers
    {   32,  200,    0,   6,  0,  0, false },  // Forest: oak with some birch
    {   32, 1000,    0,   6,  0,  0, false },  // Birch Forest
    {   28,    0, 1000,   0,  0,  0, false },  // Taiga: spruce
    {    3,    0, 1000,   0,  0,  0, true  },  // Ice Plains: bare and frozen
    {    0,    0,    0,   0,  8, 10, false },  // Desert: cacti and dead bushes
    {   10,    0,  500,   2,  0,  0, false },  // Extreme Hills: a few oak and spruce
    // Abyssal Transcendence biomes: source density translated to an 8x8 chunk.
    {    0,    0,    0,   0,  0,  0, false },  // Open Water
    {    2,    0,    0,   2,  0,  0, false },  // Shoreline
    {    5,  200,    0,   8,  0,  0, false },  // Windmeadow
    {   30,  250,    0,   5,  0,  0, false },  // Wildwood
    {   48,  100,    0,   2,  0,  0, false },  // The Deepwood
    {   12,    0,  600,   0,  0,  0, false },  // Highlands
    {   10,    0, 1000,   0,  0,  0, true  },  // Frostpeaks
    {    0,    0,    0,   0,  0, 90, false },  // The Dead Zone
    {    0,    0,    0,   0,  0,  0, false },  // The Abyss
    {    6,    0,  300,   1,  0,  0, false },  // Crystal Fields
    {    2,    0,    0,   0,  0, 30, false },  // Ashlands
    {    0,    0,    0,   0,  0,  0, false },  // The Void
    {    0,    0,    0,   0,  0,  0, false },  // The Place That Came Before
    {    5,  250,    0,   1,  0,  0, false },  // Old Ruins
    {    0,    0,    0,   0,  0,  0, false },  // Abyssal Rim
};

} // namespace

const char *biomeName(Biome b)
{
    static const char *const names[static_cast<int>(Biome::COUNT)] = {
        "Plains", "Forest", "Birch Forest", "Taiga", "Ice Plains", "Desert", "Extreme Hills",
        "Open Water", "Shoreline", "Windmeadow", "Wildwood", "The Deepwood",
        "Highlands", "Frostpeaks", "The Dead Zone", "The Abyss", "Crystal Fields",
        "Ashlands", "The Void", "The Place That Came Before", "Old Ruins", "Abyssal Rim"
    };
    return names[static_cast<int>(b)];
}

Biome biomeFor(const Climate &c)
{
    if(c.hilliness >= HILLS_START)
        return Biome::EXTREME_HILLS;
    if(c.temperature < ICE_BELOW)
        return Biome::ICE_PLAINS;
    if(c.temperature < TAIGA_BELOW)
        return Biome::TAIGA;
    if(c.humidity < DRY)
        return c.temperature >= DESERT_FROM ? Biome::DESERT : Biome::PLAINS;
    return c.humidity < WET ? Biome::FOREST : Biome::BIRCH_FOREST;
}

Climate climateAt(const PerlinNoise &noise, int x, int z)
{
    return Climate{ field(noise, x, z, TEMPERATURE_SCALE, TEMPERATURE_Z),
                    field(noise, x, z, HUMIDITY_SCALE, HUMIDITY_Z),
                    field(noise, x, z, HILLINESS_SCALE, HILLINESS_Z) };
}

Biome biomeAt(const PerlinNoise &noise, int x, int z)
{
    return biomeFor(climateAt(noise, x, z));
}

int groundHeight(const PerlinNoise &noise, int x, int z)
{
    const int base = originalHeight(noise, x, z);
    const GLFix hilliness = field(noise, x, z, HILLINESS_SCALE, HILLINESS_Z);
    if(hilliness <= HILLS_RAMP_LOW)
        return base;

    // Smoothly from nothing at the ramp's foot to full height at its top, so
    // hills rise out of the land around them instead of starting at a cliff.
    GLFix r = (hilliness - HILLS_RAMP_LOW) / (HILLS_RAMP_HIGH - HILLS_RAMP_LOW);
    if(r > GLFix(1))
        r = 1;
    const GLFix rise = r * r * (GLFix(3) - r * 2);

    // Craggy: a fast field adds up to 6 more blocks on top of the 9.
    const GLFix crag = field(noise, x, z, CRAG_SCALE, CRAG_Z);
    const int extra = (rise * (GLFix(9) + crag * 6)).round();
    const int h = base + extra;
    return h < MAX_GROUND ? h : MAX_GROUND;
}

Surface surfaceFor(Biome b, int ground_height, int steepness)
{
    switch(b)
    {
    case Biome::DESERT:
        return Surface{ BLOCK_SAND, BLOCK_SAND, 3, BLOCK_SANDSTONE, 3 };
    case Biome::ICE_PLAINS:
        return Surface{ BLOCK_SNOWY_GRASS, BLOCK_DIRT, 3, 0, 0 };
    case Biome::EXTREME_HILLS:
        if(steepness >= 3)
            return Surface{ BLOCK_STONE, BLOCK_STONE, 0, 0, 0 };   // bare rock on the cliffs
        if(ground_height > SNOW_LINE)
            return Surface{ BLOCK_SNOW, BLOCK_STONE, 0, 0, 0 };
        return Surface{ BLOCK_GRASS, BLOCK_DIRT, 2, 0, 0 };
    case Biome::ABYSSAL_OCEAN:
    case Biome::ABYSSAL_BEACH:
        return Surface{ BLOCK_SAND, BLOCK_SAND, 3, BLOCK_SANDSTONE, 2 };
    case Biome::ABYSSAL_MEADOW:
    case Biome::ABYSSAL_WILDWOOD:
        return Surface{ BLOCK_GRASS, BLOCK_DIRT, 4, 0, 0 };
    case Biome::ABYSSAL_DEEPWOOD:
        return Surface{ BLOCK_GRASS, BLOCK_DIRT, 3, BLOCK_COBBLESTONE, 1 };
    case Biome::ABYSSAL_HIGHLAND:
        if(steepness >= 3)
            return Surface{ BLOCK_STONE, BLOCK_STONE, 1, 0, 0 };
        if(ground_height >= SNOW_LINE + 1)
            return Surface{ BLOCK_SNOW, BLOCK_STONE, 1, 0, 0 };
        return Surface{ BLOCK_GRASS, BLOCK_DIRT, 2, 0, 0 };
    case Biome::ABYSSAL_SNOW:
        return Surface{ BLOCK_SNOW, BLOCK_STONE, 2, 0, 0 };
    case Biome::ABYSSAL_DEAD:
        return Surface{ BLOCK_NETHERRACK, BLOCK_COBBLESTONE, 2, BLOCK_STONE, 1 };
    case Biome::ABYSSAL_ABYSS:
    case Biome::ABYSSAL_RIM:
        return Surface{ BLOCK_OBSIDIAN, BLOCK_NETHERRACK, 3, BLOCK_STONE, 2 };
    case Biome::ABYSSAL_CRYSTAL:
        return Surface{ BLOCK_DIAMOND, BLOCK_STONE, 2, BLOCK_STONE, 2 };
    case Biome::ABYSSAL_ASHLAND:
        return Surface{ BLOCK_NETHERRACK, BLOCK_COBBLESTONE, 3, BLOCK_STONE, 1 };
    case Biome::ABYSSAL_VOID:
        return Surface{ BLOCK_OBSIDIAN, BLOCK_OBSIDIAN, 2, 0, 0 };
    case Biome::ABYSSAL_BETWEEN:
        return Surface{ BLOCK_WOOL, BLOCK_STONE, 2, 0, 0 };
    case Biome::ABYSSAL_RUINS:
        return Surface{ BLOCK_COBBLESTONE, BLOCK_COBBLESTONE, 3, BLOCK_STONE, 1 };
    default:
        return Surface{ BLOCK_GRASS, BLOCK_DIRT, 4, 0, 0 };
    }
}

const BiomeFeatures &biomeFeatures(Biome b)
{
    const unsigned int index = static_cast<unsigned int>(b);
    return features[index < static_cast<unsigned int>(Biome::COUNT) ? index : 0];
}

uint32_t columnHash(int x, int z, uint32_t seed, uint32_t salt)
{
    uint32_t h = seed ^ (salt * 0x9E3779B9u);
    h ^= static_cast<uint32_t>(x) * 0x8DA6B343u;
    h ^= static_cast<uint32_t>(z) * 0xCB1AB31Fu;
    h ^= h >> 15; h *= 0x2C1B3C6Du;
    h ^= h >> 12; h *= 0x297A2D39u;
    h ^= h >> 15;
    return h;
}
