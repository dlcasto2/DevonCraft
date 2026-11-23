#include "doctest.h"

#include "abyssalgen.h"
#include "water.h"

TEST_CASE("Abyssal terrain is deterministic and stays inside calculator world height") {
    AbyssalNoise a(31337), b(31337);
    for(int x = -360; x <= 360; x += 37)
        for(int z = -360; z <= 360; z += 41)
        {
            const AbyssalColumn ca = abyssalColumnAt(a, x, z);
            const AbyssalColumn cb = abyssalColumnAt(b, x, z);
            CHECK(ca.height == cb.height);
            CHECK(ca.biome == cb.biome);
            CHECK(ca.height >= 3);
            CHECK(ca.height <= 36);
        }
}

TEST_CASE("far terrain coordinates do not overflow smoothstep into the Dead Zone") {
    AbyssalNoise noise(123456);
    const AbyssalColumn at_30000 = abyssalColumnAt(noise, 30000, 0);
    const AbyssalColumn at_33000 = abyssalColumnAt(noise, 33000, 0);
    CHECK(at_30000.dead < GLFix(0.5f));
    CHECK(at_33000.dead < GLFix(0.5f));
    CHECK(at_30000.biome != Biome::ABYSSAL_DEAD);
    CHECK(at_33000.biome != Biome::ABYSSAL_DEAD);
}

TEST_CASE("Abyss and Dead Zone landmarks are generated from the world seed") {
    AbyssalNoise noise(123456);
    const AbyssalNoise::Anchor &abyss = noise.abyss();
    const AbyssalNoise::Anchor &dead = noise.deadZone();

    const AbyssalColumn pit = abyssalColumnAt(noise, abyss.x, abyss.z);
    CHECK(pit.biome == Biome::ABYSSAL_ABYSS);
    CHECK(pit.abyss_distance < GLFix(0));
    CHECK(pit.height == 4);

    const AbyssalColumn wasteland = abyssalColumnAt(noise, dead.x, dead.z);
    const bool dead_zone_is_present =
        wasteland.dead > GLFix(0.5f) || wasteland.biome == Biome::ABYSSAL_DEAD;
    CHECK(dead_zone_is_present);
}

TEST_CASE("Abyssal terrain has multiple elevations and the source biome families") {
    AbyssalNoise noise(42);
    int min_height = 100, max_height = -1;
    bool ocean = false, wood = false, highland = false;
    for(int x = -600; x <= 600; x += 8)
        for(int z = -600; z <= 600; z += 8)
        {
            const AbyssalColumn c = abyssalColumnAt(noise, x, z);
            if(c.height < min_height) min_height = c.height;
            if(c.height > max_height) max_height = c.height;
            ocean = ocean || c.biome == Biome::ABYSSAL_OCEAN;
            wood = wood || c.biome == Biome::ABYSSAL_WILDWOOD || c.biome == Biome::ABYSSAL_DEEPWOOD;
            highland = highland || c.biome == Biome::ABYSSAL_HIGHLAND || c.biome == Biome::ABYSSAL_SNOW;
        }
    CHECK(max_height - min_height >= 8);
    CHECK(ocean);
    CHECK(wood);
    CHECK(highland);
}
