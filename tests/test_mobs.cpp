#include "doctest.h"
#include "mobs.h"
#include "terrain.h"
#include "grid_world.h"
#include "gl.h"

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


TEST_CASE("mob collision does not trigger terrain auto-jump") {
    GridWorld terrain;
    EntityPool pool;
    REQUIRE(spawnPassive(pool, MOB_PIG,
        AABB{GLFix(64), GLFix(128), GLFix(120),
             GLFix(160), GLFix(224), GLFix(216)}) != nullptr);
    const MobCollisionQuery collision(terrain, pool);
    const AABB player{GLFix(64 - 51), GLFix(128), GLFix(64 - 51),
                      GLFix(64 + 51), GLFix(128 + 230), GLFix(64 + 51)};

    CHECK(collision.intersects(AABB{player.low_x, player.low_y, player.low_z + GLFix(20),
                                   player.high_x, player.high_y, player.high_z + GLFix(20)}));
    CHECK_FALSE(shouldAutoJump(collision, player, GLFix(0), GLFix(20), true, false));

    // Real block steps still trigger the normal auto-jump check.
    terrain.solid.insert(std::make_tuple(0, 1, 1));
    CHECK(shouldAutoJump(collision, player, GLFix(0), GLFix(20), true, false));
}


TEST_CASE("damaging a mob lowers its health and briefly marks it as hurt") {
    EntityPool pool;
    Entity *pig = spawnPassive(pool, MOB_PIG, AABB());
    REQUIRE(pig != nullptr);
    CHECK(pig->health == 10);
    CHECK_FALSE(hurtMob(*pig, MOB_HIT_DAMAGE));
    CHECK(pig->health == 8);
    CHECK(pig->hurt_ticks == MOB_HURT_FLASH_TICKS);

    GridWorld world;
    pool.tick(world);
    CHECK(pig->hurt_ticks == MOB_HURT_FLASH_TICKS - 1);
}

TEST_CASE("lethal mob damage clamps health to zero and leaves traders immune") {
    EntityPool pool;
    Entity *pig = spawnPassive(pool, MOB_PIG, AABB());
    Entity *trader = spawnTrader(pool, AABB());
    REQUIRE(pig != nullptr);
    REQUIRE(trader != nullptr);

    CHECK(hurtMob(*pig, 100));
    CHECK(pig->health == 0);
    CHECK_FALSE(hurtMob(*pig, 2));
    CHECK(pig->health == 0);

    CHECK_FALSE(hurtMob(*trader, 100));
    CHECK(trader->health == 0);
}

TEST_CASE("invalid damage does not hurt mobs") {
    EntityPool pool;
    Entity *cow = spawnPassive(pool, MOB_COW, AABB());
    REQUIRE(cow != nullptr);
    CHECK_FALSE(hurtMob(*cow, 0));
    CHECK_FALSE(hurtMob(*cow, -5));
    CHECK(cow->health == mobMaxHealth(MOB_COW));
    CHECK(cow->hurt_ticks == 0);
}


TEST_CASE("sheep have the same health as pigs and cows and drop mutton and wool")
{
    EntityPool pool;
    Entity *sheep = spawnPassive(pool, MOB_SHEEP, AABB());
    REQUIRE(sheep != nullptr);
    CHECK(sheep->health == 10);
    CHECK(mobMaxHealth(MOB_SHEEP) == mobMaxHealth(MOB_PIG));
    CHECK(mobMaxHealth(MOB_SHEEP) == mobMaxHealth(MOB_COW));
    CHECK(mobDrop(MOB_SHEEP).id == ITEM_MUTTON_RAW);
    CHECK(mobWoolDrop(*sheep).id == BLOCK_WOOL);
    CHECK(mobWoolDrop(*sheep).count == 1);
}

TEST_CASE("shearing gives one to three wool, cannot repeat, and fleece regrows only after grazing")
{
    EntityPool pool;
    Entity *sheep = spawnPassive(pool, MOB_SHEEP, AABB());
    REQUIRE(sheep != nullptr);
    uint32_t rng = 12345;
    const int wool = shearSheep(*sheep, rng);
    CHECK(wool >= 1);
    CHECK(wool <= 3);
    CHECK(sheep->sheared);
    CHECK(sheep->wool_regrow_ticks == MOB_SHEEP_REGROW_TICKS);
    CHECK(mobWoolDrop(*sheep).empty());
    CHECK(shearSheep(*sheep, rng) == 0);

    // Waiting away from grass does not advance wool regrowth.
    for(int i = 0; i < 5; ++i)
        CHECK_FALSE(tickSheepRegrowth(*sheep, false));
    CHECK(sheep->wool_regrow_ticks == MOB_SHEEP_REGROW_TICKS);

    for(int i = 0; i < MOB_SHEEP_REGROW_TICKS - 1; ++i)
        CHECK_FALSE(tickSheepRegrowth(*sheep, true));
    CHECK(tickSheepRegrowth(*sheep, true));
    CHECK_FALSE(sheep->sheared);
    CHECK(sheep->wool_regrow_ticks == 0);
    CHECK(mobWoolDrop(*sheep).id == BLOCK_WOOL);
}

TEST_CASE("dead or non-sheep mobs cannot be sheared")
{
    EntityPool pool;
    Entity *pig = spawnPassive(pool, MOB_PIG, AABB());
    Entity *dead_sheep = spawnPassive(pool, MOB_SHEEP, AABB());
    REQUIRE(pig != nullptr);
    REQUIRE(dead_sheep != nullptr);
    uint32_t rng = 7;
    CHECK(shearSheep(*pig, rng) == 0);
    CHECK(hurtMob(*dead_sheep, 10));
    CHECK(shearSheep(*dead_sheep, rng) == 0);
}


TEST_CASE("sheep take five one-heart hits just like pigs and cows")
{
    EntityPool pool;
    Entity *sheep = spawnPassive(pool, MOB_SHEEP, AABB());
    REQUIRE(sheep != nullptr);
    for(int i = 0; i < 4; ++i)
    {
        CHECK_FALSE(hurtMob(*sheep, MOB_HIT_DAMAGE));
        CHECK(sheep->health == 10 - (i + 1) * 2);
    }
    CHECK(hurtMob(*sheep, MOB_HIT_DAMAGE));
    CHECK(sheep->health == 0);
}


TEST_CASE("nGL line segments are clipped before unsafe perspective depths")
{
    VERTEX behind{GLFix(-2), GLFix(1), GLFix(0.25f), GLFix(0), GLFix(0), 0x07FF};
    VERTEX ahead{GLFix(2), GLFix(5), GLFix(2), GLFix(0), GLFix(0), 0x07FF};

    // The configured near plane can be below one, but the integer projector
    // requires a depth whose integer part is at least one.
    CHECK(nglClipLineToNearPlane(behind, ahead, GLFix(0.5f)));
    CHECK(behind.z == GLFix(1));
    CHECK(behind.z.toInteger<int>() >= 1);

    VERTEX hidden_a{GLFix(0), GLFix(0), GLFix(0.1f), GLFix(0), GLFix(0), 0x07FF};
    VERTEX hidden_b{GLFix(1), GLFix(1), GLFix(0.9f), GLFix(0), GLFix(0), 0x07FF};
    CHECK_FALSE(nglClipLineToNearPlane(hidden_a, hidden_b, GLFix(0.5f)));

    VERTEX visible_a{GLFix(0), GLFix(0), GLFix(2), GLFix(0), GLFix(0), 0x07FF};
    VERTEX visible_b{GLFix(1), GLFix(1), GLFix(3), GLFix(0), GLFix(0), 0x07FF};
    CHECK(nglClipLineToNearPlane(visible_a, visible_b, GLFix(1)));
    CHECK(visible_a.z == GLFix(2));
    CHECK(visible_b.z == GLFix(3));
}
