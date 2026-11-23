#include "doctest.h"
#include "mobs.h"
#include "terrain.h"

TEST_CASE("a pig takes five bare-hand hits, and the fifth kills it") {
    EntityPool pool;
    Entity *pig = spawnPassive(pool, MOB_PIG, AABB());
    REQUIRE(pig != nullptr);
    CHECK(pig->health == mobMaxHealth(MOB_PIG));
    for(int i = 0; i < 4; ++i)
        CHECK_FALSE(hurtMob(*pig, MOB_HIT_DAMAGE));
    CHECK(hurtMob(*pig, MOB_HIT_DAMAGE));
}

TEST_CASE("cows have the same health as pigs but drop beef") {
    CHECK(mobMaxHealth(MOB_COW) == mobMaxHealth(MOB_PIG));
    CHECK(mobDrop(MOB_PIG).id == ITEM_PORK_RAW);
    CHECK(mobDrop(MOB_COW).id == ITEM_BEEF_RAW);
    CHECK(mobDrop(MOB_COW).count == 1);
}

TEST_CASE("spawned mobs are counted, and other entity types are not") {
    EntityPool pool;
    CHECK(countPassive(pool) == 0);
    Entity *a = spawnPassive(pool, MOB_PIG, AABB());
    Entity *b = spawnPassive(pool, MOB_COW, AABB());
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    CHECK(countPassive(pool) == 2);
    CHECK(b->mob_kind == MOB_COW);
    CHECK(b->health == mobMaxHealth(MOB_COW));
    // Other entity types don't count.
    pool.spawn(EntityType::DROPPED_ITEM, AABB());
    CHECK(countPassive(pool) == 2);
}

TEST_CASE("mobs spawn only on grass in daylight") {
    CHECK(canSpawnPassive(true, 256));
    CHECK(canSpawnPassive(true, 128));
    CHECK_FALSE(canSpawnPassive(true, 127));
    CHECK_FALSE(canSpawnPassive(false, 256));
    CHECK_FALSE(canSpawnPassive(false, 0));
}

TEST_CASE("wandering walks only along the axes, at a walking pace, and sometimes stands") {
    uint32_t rng = 4242;
    Entity e;
    e.type = EntityType::MOB;
    const GLFix walk = GLFix(BLOCK_SIZE) / 20;
    int walking = 0, standing = 0;
    for(int tick = 0; tick < 2000; ++tick)
    {
        steerMob(e, rng);
        // Never diagonal: at most one axis moves at a time.
        const bool diagonal = e.vx != GLFix(0) && e.vz != GLFix(0);
        CHECK_FALSE(diagonal);
        // Never faster than the walking pace.
        CHECK(e.vx <= walk);
        CHECK(e.vx >= -walk);
        CHECK(e.vz <= walk);
        CHECK(e.vz >= -walk);
        if(e.vx == GLFix(0) && e.vz == GLFix(0))
            ++standing;
        else
            ++walking;
    }
    CHECK(walking > 0);
    CHECK(standing > 0);
}

TEST_CASE("the same seed gives the same wandering") {
    uint32_t a = 99, b = 99;
    Entity ea, eb;
    ea.type = eb.type = EntityType::MOB;
    for(int tick = 0; tick < 500; ++tick)
    {
        steerMob(ea, a);
        steerMob(eb, b);
        CHECK(ea.vx == eb.vx);
        CHECK(ea.vz == eb.vz);
    }
}
