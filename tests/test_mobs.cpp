#include "doctest.h"
#include "mobs.h"
#include "terrain.h"
#include "grid_world.h"

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


TEST_CASE("mob hitboxes block player movement but not nearby empty space") {
    GridWorld terrain;
    EntityPool pool;
    const AABB mob_box{GLFix(100), GLFix(128), GLFix(100),
                       GLFix(196), GLFix(224), GLFix(196)};
    REQUIRE(spawnPassive(pool, MOB_PIG, mob_box) != nullptr);
    const MobCollisionQuery collision(terrain, pool);

    CHECK(collision.intersects(AABB{GLFix(120), GLFix(140), GLFix(120),
                                    GLFix(150), GLFix(180), GLFix(150)}));
    CHECK_FALSE(collision.intersects(AABB{GLFix(10), GLFix(140), GLFix(10),
                                          GLFix(50), GLFix(180), GLFix(50)}));
    // Merely touching the mob's top face is not an overlap.
    CHECK_FALSE(collision.intersects(AABB{GLFix(120), GLFix(224), GLFix(120),
                                          GLFix(150), GLFix(250), GLFix(150)}));
}

TEST_CASE("mob collision query keeps normal world and water collision behavior") {
    GridWorld terrain;
    terrain.solid.insert(std::make_tuple(0, 0, 0));
    terrain.water.insert(std::make_tuple(1, 1, 1));
    EntityPool pool;
    const MobCollisionQuery collision(terrain, pool);

    CHECK(collision.intersects(AABB{GLFix(10), GLFix(10), GLFix(10),
                                    GLFix(20), GLFix(20), GLFix(20)}));
    CHECK(collision.touchesWater(AABB{GLFix(BLOCK_SIZE + 1), GLFix(BLOCK_SIZE + 1), GLFix(BLOCK_SIZE + 1),
                                      GLFix(BLOCK_SIZE + 20), GLFix(BLOCK_SIZE + 20), GLFix(BLOCK_SIZE + 20)}));
}

TEST_CASE("a player already overlapping a mob is moved to a clear side") {
    GridWorld terrain;
    EntityPool pool;
    const AABB mob_box{GLFix(100), GLFix(128), GLFix(100),
                       GLFix(196), GLFix(224), GLFix(196)};
    REQUIRE(spawnPassive(pool, MOB_COW, mob_box) != nullptr);
    AABB player{GLFix(130), GLFix(128), GLFix(130),
                GLFix(232), GLFix(358), GLFix(232)};

    const MobCollisionQuery collision(terrain, pool);
    REQUIRE(collision.intersects(player));
    CHECK(separatePlayerFromMobs(player, pool, terrain));
    CHECK_FALSE(collision.intersects(player));
    CHECK(player.low_x != GLFix(130) || player.low_z != GLFix(130));
}

TEST_CASE("dropped items do not become player collision hitboxes") {
    GridWorld terrain;
    EntityPool pool;
    pool.spawn(EntityType::DROPPED_ITEM,
               AABB{GLFix(100), GLFix(128), GLFix(100),
                    GLFix(196), GLFix(224), GLFix(196)});
    const MobCollisionQuery collision(terrain, pool);
    CHECK_FALSE(collision.intersects(AABB{GLFix(120), GLFix(140), GLFix(120),
                                          GLFix(150), GLFix(180), GLFix(150)}));
}
