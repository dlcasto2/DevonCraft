#include "doctest.h"
#include "village.h"

TEST_CASE("village candidates are deterministic and stay inside their grid cells") {
    for(int x = -40; x <= 40; ++x)
        for(int z = -40; z <= 40; ++z)
        {
            VillageSite a = {}, b = {};
            const bool found_a = villageSiteForCell(x, z, 12345u, a);
            const bool found_b = villageSiteForCell(x, z, 12345u, b);
            CHECK(found_a == found_b);
            if(found_a)
            {
                CHECK(a.x == b.x);
                CHECK(a.z == b.z);
                CHECK(a.x >= x * VILLAGE_CELL_SIZE + 18);
                CHECK(a.x <= x * VILLAGE_CELL_SIZE + 22);
                CHECK(a.z >= z * VILLAGE_CELL_SIZE + 18);
                CHECK(a.z <= z * VILLAGE_CELL_SIZE + 22);
            }
        }
}

TEST_CASE("village candidates have room between neighbouring settlements") {
    for(unsigned seed : {1u, 42u, 123456u})
        for(int x = -20; x < 20; ++x)
            for(int z = -20; z < 20; ++z)
            {
                VillageSite a = {}, b = {};
                if(villageSiteForCell(x, z, seed, a)
                   && villageSiteForCell(x + 1, z, seed, b))
                    CHECK(b.x - a.x >= 36);
                if(villageSiteForCell(x, z, seed, a)
                   && villageSiteForCell(x, z + 1, seed, b))
                    CHECK(b.z - a.z >= 36);
            }
}

TEST_CASE("village placement is limited to supported overworld biomes") {
    CHECK(villageBiomeAllowed(Biome::PLAINS));
    CHECK(villageBiomeAllowed(Biome::FOREST));
    CHECK(villageBiomeAllowed(Biome::BIRCH_FOREST));
    CHECK(villageBiomeAllowed(Biome::TAIGA));
    CHECK(villageBiomeAllowed(Biome::ICE_PLAINS));
    CHECK(villageBiomeAllowed(Biome::DESERT));
    CHECK_FALSE(villageBiomeAllowed(Biome::EXTREME_HILLS));
    CHECK_FALSE(villageBiomeAllowed(Biome::ABYSSAL_ABYSS));
}

TEST_CASE("village candidate frequency is controlled by the seeded grid") {
    int candidates = 0;
    for(int x = -50; x < 50; ++x)
        for(int z = -50; z < 50; ++z)
        {
            VillageSite site = {};
            if(villageSiteForCell(x, z, 9876u, site))
                ++candidates;
        }
    CHECK(candidates > 5600);
    CHECK(candidates < 6400);
}

TEST_CASE("village foundations accept gentle terrain and reject steep ground") {
    CHECK(villageTerrainVariationAllowed(12, 12));
    CHECK(villageTerrainVariationAllowed(12, 16));
    CHECK(villageTerrainVariationAllowed(12, 20));
    CHECK_FALSE(villageTerrainVariationAllowed(12, 21));
    CHECK_FALSE(villageTerrainVariationAllowed(20, 12));
}

TEST_CASE("seeded overworld terrain produces suitable village sites") {
    PerlinNoise noise(12345u);
    int suitable = 0;
    for(int x = -20; x <= 20; ++x)
        for(int z = -20; z <= 20; ++z)
        {
            VillageSite site = {};
            if(villageSiteForCell(x, z, 12345u, site)
               && villageSiteSuitable(noise, site))
                ++suitable;
        }
    CHECK(suitable > 0);
}
