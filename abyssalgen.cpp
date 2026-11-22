#include "abyssalgen.h"

#include <cmath>

#include "water.h"

namespace {

GLFix absFix(GLFix v) { return v < GLFix(0) ? -v : v; }
GLFix minFix(GLFix a, GLFix b) { return a < b ? a : b; }
GLFix maxFix(GLFix a, GLFix b) { return a > b ? a : b; }
GLFix clampFix(GLFix v, GLFix lo, GLFix hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

GLFix lerpFix(GLFix a, GLFix b, GLFix t)
{
    return a + (b - a) * t;
}

GLFix smooth(GLFix edge0, GLFix edge1, GLFix x)
{
    GLFix t = clampFix((x - edge0) / (edge1 - edge0), GLFix(0), GLFix(1));
    return t * t * (GLFix(3) - GLFix(2) * t);
}

GLFix length2D(GLFix x, GLFix z)
{
    // Convert before squaring: GLFix stores 32-bit fixed-point values, so
    // squaring world-space offsets directly can overflow far from the origin.
    const float fx = static_cast<float>(x);
    const float fz = static_cast<float>(z);
    return GLFix(std::sqrt(fx * fx + fz * fz));
}

// A bounded, inexpensive approximation of exp(-x), used only for broad
// continent-anchor and moisture falloffs.
GLFix expNegative(GLFix x)
{
    x = maxFix(x, GLFix(0));
    // Beyond this range the regional falloff is negligible. Capping also
    // keeps the fixed-point square inside its 32-bit intermediate range.
    if(x >= GLFix(32))
        return GLFix(0);
    const GLFix x2 = x * x;
    return GLFix(1) / (GLFix(1) + x + x2 * GLFix(0.5f));
}

GLFix fbm2(const AbyssalNoise &noise, unsigned int field, GLFix x, GLFix z, int octaves)
{
    GLFix amplitude(1), frequency(1), sum(0), total(0);
    for(int i = 0; i < octaves; ++i)
    {
        sum += noise.noise2(field, x * frequency, z * frequency) * amplitude;
        total += amplitude;
        amplitude *= GLFix(0.5f);
        frequency *= GLFix(2.03f);
    }
    return total == GLFix(0) ? GLFix(0) : sum / total;
}

uint32_t mix32(uint32_t x)
{
    x ^= x >> 16; x *= 0x7FEB352Du;
    x ^= x >> 15; x *= 0x846CA68Bu;
    x ^= x >> 16;
    return x;
}

AbyssalNoise::Anchor ringAnchor(uint32_t seed, uint32_t salt, int min_radius, int max_radius, int radius)
{
    static const int directions[16][2] = {
        {1000, 0}, {924, 383}, {707, 707}, {383, 924},
        {0, 1000}, {-383, 924}, {-707, 707}, {-924, 383},
        {-1000, 0}, {-924, -383}, {-707, -707}, {-383, -924},
        {0, -1000}, {383, -924}, {707, -707}, {924, -383}
    };
    const uint32_t h = mix32(seed ^ salt);
    const int *d = directions[h & 15u];
    const int distance = min_radius + static_cast<int>((h >> 8) % (max_radius - min_radius + 1));
    return Anchor{d[0] * distance / 1000, d[1] * distance / 1000, radius};
}

} // namespace

AbyssalNoise::AbyssalNoise(unsigned int seed)
    : nA(0), nB(0), nC(0), nD(0), nE(0), nF(0)
{
    setSeed(seed);
}

void AbyssalNoise::setSeed(unsigned int seed)
{
    nA.setSeed(seed ^ 0x1111u);
    nB.setSeed(seed ^ 0x2222u);
    nC.setSeed(seed ^ 0x3333u);
    nD.setSeed(seed ^ 0x4444u);
    nE.setSeed(seed ^ 0x5555u);
    nF.setSeed(seed ^ 0x6666u);

    // The source world places one abyss, one dead region and two lakes on
    // deterministic rings around the starting continent.
    abyss_anchor = ringAnchor(seed, 0xAB7551u, 135, 205, 30);
    dead_anchor = ringAnchor(seed, 0xDEA2u, 215, 305, 120);
    lake_anchors[0] = ringAnchor(seed, 0x1A6E1u, 34, 58, 15);
    lake_anchors[1] = ringAnchor(seed, 0x1A6E2u, 78, 112, 22);
}

const PerlinNoise &AbyssalNoise::field(unsigned int index) const
{
    switch(index % 6)
    {
    case 0: return nA;
    case 1: return nB;
    case 2: return nC;
    case 3: return nD;
    case 4: return nE;
    default: return nF;
    }
}

GLFix AbyssalNoise::noise2(unsigned int index, GLFix x, GLFix z) const
{
    return field(index).noise(x, z, GLFix(0)) * 2 - 1;
}

GLFix AbyssalNoise::noise3(unsigned int index, GLFix x, GLFix y, GLFix z) const
{
    return field(index).noise(x, y, z) * 2 - 1;
}

AbyssalColumn abyssalColumnAt(const AbyssalNoise &noise, int x, int z)
{
    const GLFix X(x), Z(z);
    const GLFix sea(SEA_LEVEL);

    // Source parameters for the Origin world, with vertical amplitude scaled
    // from 96 to DevonCraft's 40-block world.
    GLFix continent = fbm2(noise, 0, X / 700, Z / 700, 3) + GLFix(0.05f);
    const float anchor_x = static_cast<float>(X) / 120.0f;
    const float anchor_z = static_cast<float>(Z) / 120.0f;
    const GLFix anchor_q(anchor_x * anchor_x + anchor_z * anchor_z);
    continent += GLFix(0.55f) * expNegative(anchor_q);

    const GLFix hills = fbm2(noise, 1, X / 170, Z / 170, 3);
    const GLFix detail = noise.noise2(1, X / 26, Z / 26) * GLFix(0.6f);

    GLFix ridge = GLFix(1) - absFix(fbm2(noise, 2, X / 300, Z / 300, 2));
    ridge = ridge * ridge * ridge;
    const GLFix mount = smooth(GLFix(0.55f), GLFix(0.85f),
        noise.noise2(3, X / 900 + GLFix(11.3f), Z / 900 - GLFix(7.1f)) * GLFix(0.5f) + GLFix(0.5f));

    GLFix height = sea
        + continent * GLFix(14)
        + hills * GLFix(3)
        + detail * GLFix(0.6f)
        + ridge * mount * GLFix(17);

    GLFix moisture = noise.noise2(4, X / 330 + GLFix(3.1f), Z / 330 + GLFix(9.7f)) * GLFix(0.5f) + GLFix(0.5f);
    moisture += noise.noise2(4, X / 90, Z / 90) * GLFix(0.08f);
    const float moisture_x = static_cast<float>(X) / 90.0f;
    const float moisture_z = static_cast<float>(Z) / 90.0f;
    moisture += GLFix(0.2f) * expNegative(GLFix(moisture_x * moisture_x + moisture_z * moisture_z));

    // River valleys follow a narrow band in the fourth source noise field.
    const GLFix river = absFix(noise.noise2(3, X / 430 + 50, Z / 430 - 20));
    if(river < GLFix(0.055f) && height > sea - 1)
        height = lerpFix(sea - GLFix(3.5f), height, smooth(GLFix(0), GLFix(0.055f), river));

    // Broad natural lakes plus two seed-placed lakes from the Origin world.
    const GLFix lake_noise = noise.noise2(5, X / 140 + GLFix(7.7f), Z / 140 - GLFix(3.3f));
    if(lake_noise > GLFix(0.55f) && height > sea - 3)
    {
        const GLFix lake_floor = minFix(height, sea - GLFix(2.5f) - (lake_noise - GLFix(0.55f)) * 14);
        height = lerpFix(height, lake_floor, smooth(GLFix(0.55f), GLFix(0.66f), lake_noise));
    }

    for(unsigned int i = 0; i < 2; ++i)
    {
        const AbyssalNoise::Anchor &lake = noise.lake(i);
        const GLFix dx = X - lake.x, dz = Z - lake.z;
        const GLFix distance = length2D(dx, dz) + noise.noise2(1, X / 18, Z / 18) * 5;
        if(distance < GLFix(lake.radius + 7))
        {
            const GLFix lake_floor = sea - GLFix(1.5f) - smooth(GLFix(lake.radius), GLFix(0), distance) * 6;
            height = lerpFix(height, lake_floor, smooth(GLFix(lake.radius + 7), GLFix(lake.radius - 3), distance));
        }
    }

    // The Dead Zone is a broad, noisy region with shallow craters.
    const AbyssalNoise::Anchor &dead_anchor = noise.deadZone();
    const GLFix dead_dx = X - dead_anchor.x, dead_dz = Z - dead_anchor.z;
    const GLFix dead_distance = length2D(dead_dx, dead_dz) + noise.noise2(0, X / 70, Z / 70) * 30;
    GLFix dead = smooth(GLFix(dead_anchor.radius), GLFix(dead_anchor.radius) * GLFix(0.6f), dead_distance);
    if(dead > GLFix(0))
    {
        height = lerpFix(height, sea + GLFix(5) + hills * GLFix(1.8f), dead * GLFix(0.5f));
        const GLFix crater = absFix(noise.noise2(2, X / 48, Z / 48));
        if(crater < GLFix(0.04f))
            height -= (GLFix(1) - crater / GLFix(0.04f)) * GLFix(3.3f) * dead;
    }

    // Abyssal crater and its raised rim.
    const AbyssalNoise::Anchor &abyss_anchor = noise.abyss();
    const GLFix abyss_dx = X - abyss_anchor.x, abyss_dz = Z - abyss_anchor.z;
    const GLFix abyss_distance = length2D(abyss_dx, abyss_dz)
        - GLFix(abyss_anchor.radius) * (GLFix(1) + noise.noise2(1, X / 22, Z / 22) * GLFix(0.18f));
    if(abyss_distance < GLFix(16))
        height += (GLFix(16) - maxFix(abyss_distance, GLFix(0))) * GLFix(0.35f);

    Biome biome;
    if(abyss_distance < GLFix(0))
        biome = Biome::ABYSSAL_ABYSS;
    else if(abyss_distance < GLFix(9))
        biome = Biome::ABYSSAL_RIM;
    else if(dead > GLFix(0.5f))
        biome = Biome::ABYSSAL_DEAD;
    else if(height < sea - GLFix(1.2f))
        biome = Biome::ABYSSAL_OCEAN;
    else if(height < sea + GLFix(1.8f))
        biome = Biome::ABYSSAL_BEACH;
    else if(height > sea + GLFix(16))
        biome = Biome::ABYSSAL_SNOW;
    else if(ridge * mount > GLFix(0.3f) && height > sea + GLFix(6.5f))
        biome = Biome::ABYSSAL_HIGHLAND;
    else if(moisture > GLFix(0.6f))
        biome = Biome::ABYSSAL_DEEPWOOD;
    else if(moisture > GLFix(0.43f))
        biome = Biome::ABYSSAL_WILDWOOD;
    else
        biome = Biome::ABYSSAL_MEADOW;

    GLFix overhang(0.10f);
    if(biome == Biome::ABYSSAL_HIGHLAND || biome == Biome::ABYSSAL_CRYSTAL)
        overhang = GLFix(0.25f);
    else if(biome == Biome::ABYSSAL_DEEPWOOD || biome == Biome::ABYSSAL_WILDWOOD
        || biome == Biome::ABYSSAL_MEADOW || biome == Biome::ABYSSAL_RUINS
        || biome == Biome::ABYSSAL_BEACH)
        overhang = GLFix(0.03f);
    if(abyss_distance < GLFix(20))
        overhang = GLFix(0);

    int ground = static_cast<int>(height.round());
    if(abyss_distance < GLFix(0))
        ground = 4; // A safe, dark floor below the open Abyss.
    ground = ground < 3 ? 3 : ground > 36 ? 36 : ground;

    return AbyssalColumn{ground, biome, dead, abyss_distance, overhang, moisture};
}

bool abyssalCaveAt(const AbyssalNoise &noise, int x, int y, int z)
{
    const GLFix X(x), Y(y), Z(z);
    const GLFix a = noise.noise3(2, X / 40, Y / 22, Z / 40);
    const GLFix b = noise.noise3(3, X / 40 + 100, Y / 22, Z / 40);
    const GLFix ceiling = noise.noise3(4, X / 70, Y / 35, Z / 70);
    return a * a + b * b < GLFix(0.012f) || ceiling > GLFix(0.62f);
}
