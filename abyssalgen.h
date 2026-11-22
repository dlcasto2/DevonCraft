#ifndef ABYSSALGEN_H
#define ABYSSALGEN_H

#include <cstdint>

#include "perlinnoise.h"
#include "biome.h"

// Seeded noise fields and deterministic landmarks adapted from
// db-v2/public/games/abyssal-transcendence/index.html.
class AbyssalNoise
{
public:
    struct Anchor {
        int x, z, radius;
    };

    explicit AbyssalNoise(unsigned int seed = 0);
    void setSeed(unsigned int seed);

    // Field indices match the six source generator fields (nA..nF).
    GLFix noise2(unsigned int field, GLFix x, GLFix z) const;
    GLFix noise3(unsigned int field, GLFix x, GLFix y, GLFix z) const;

    const Anchor &abyss() const { return abyss_anchor; }
    const Anchor &deadZone() const { return dead_anchor; }
    const Anchor &lake(unsigned int i) const { return lake_anchors[i & 1]; }

private:
    const PerlinNoise &field(unsigned int index) const;

    PerlinNoise nA, nB, nC, nD, nE, nF;
    Anchor abyss_anchor{}, dead_anchor{}, lake_anchors[2]{};
};

struct AbyssalColumn {
    int height;
    Biome biome;
    GLFix dead;
    GLFix abyss_distance;
    GLFix overhang;
    GLFix moisture;
};

// Height, biome and regional fields for one world column. World height is
// compressed for DevonCraft's 40-block build limit; horizontal scales and
// the source's seeded continent/ridge/rivers/lakes/region logic are retained.
AbyssalColumn abyssalColumnAt(const AbyssalNoise &noise, int x, int z);

// The source's 3D cave-density test, sampled directly instead of building
// the browser generator's larger trilinear density grids.
bool abyssalCaveAt(const AbyssalNoise &noise, int x, int y, int z);

#endif // ABYSSALGEN_H
