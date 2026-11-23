#ifndef ABYSSALGEN_H
#define ABYSSALGEN_H

#include <cstdint>

#include "gl.h"
#include "biome.h"

// Seeded Simplex fields and deterministic Origin-world landmarks adapted from
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
    struct NoiseField {
        void setSeed(uint32_t seed);
        GLFix noise2(GLFix x, GLFix z) const;
        GLFix noise3(GLFix x, GLFix y, GLFix z) const;

        uint8_t perm[512];
        uint8_t grad[512];
    };

    const NoiseField &field(unsigned int index) const;

    NoiseField nA, nB, nC, nD, nE, nF;
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

// Height, biome and regional fields for one world column. DevonCraft keeps
// its 8x8 chunks and 40-block height, so vertical amplitudes are compressed.
AbyssalColumn abyssalColumnAt(const AbyssalNoise &noise, int x, int z);

// Source 3D Simplex density tests, sampled directly to avoid allocating the
// browser generator's larger trilinear density grids on the calculator.
bool abyssalCaveAt(const AbyssalNoise &noise, int x, int y, int z);
bool abyssalVoidPocketAt(const AbyssalNoise &noise, int x, int y, int z);

#endif // ABYSSALGEN_H
