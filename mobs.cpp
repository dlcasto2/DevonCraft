#include "mobs.h"

#include "terrain.h"

namespace {

constexpr int PIG_HEALTH = 10, COW_HEALTH = 10;   // five hearts each, as in 1.8.8
constexpr int WALK_TICKS_MIN = 40, WALK_TICKS_RANGE = 80;
constexpr int STAND_HEADING = 4;
constexpr int DAYLIGHT_FOR_SPAWNING = 128;        // half of full daylight (daylightFactor is 0-256)

// Walking pace: about a twentieth of a block a tick, slower than a player.
GLFix walkSpeed()
{
    return GLFix(BLOCK_SIZE) / 20;
}

} // namespace

int mobMaxHealth(uint8_t kind)
{
    return kind == MOB_COW ? COW_HEALTH : PIG_HEALTH;
}

ItemStack mobDrop(uint8_t kind)
{
    return kind == MOB_COW ? ItemStack(ITEM_BEEF_RAW, 1) : ItemStack(ITEM_PORK_RAW, 1);
}

uint32_t mobRandom(uint32_t &rng)
{
    rng = rng * 1664525u + 1013904223u;
    return rng >> 8;
}

Entity *spawnPassive(EntityPool &pool, uint8_t kind, const AABB &box)
{
    Entity *e = pool.spawn(EntityType::MOB, box);
    if(e)
    {
        e->mob_kind = kind;
        e->health = static_cast<int16_t>(mobMaxHealth(kind));
    }
    return e;
}

int countPassive(const EntityPool &pool)
{
    int n = 0;
    pool.forEach([&n](const Entity &e) { if(e.type == EntityType::MOB) ++n; });
    return n;
}

bool hurtMob(Entity &e, int damage)
{
    e.health = static_cast<int16_t>(e.health - damage);
    return e.health <= 0;
}

void steerMob(Entity &e, uint32_t &rng)
{
    if(e.mob_timer == 0)
    {
        // Stand still a third of the time; otherwise walk one way for a few seconds.
        const uint32_t r = mobRandom(rng);
        e.mob_heading = r % 3 == 0 ? STAND_HEADING : static_cast<uint8_t>((r >> 4) % 4);
        e.mob_timer = static_cast<uint16_t>(WALK_TICKS_MIN + (r >> 12) % WALK_TICKS_RANGE);
    }
    --e.mob_timer;

    if(e.mob_heading == STAND_HEADING)
    {
        e.vx = 0;
        e.vz = 0;
        return;
    }
    e.vx = GLFix(MOB_DIR_DX[e.mob_heading]) * walkSpeed();
    e.vz = GLFix(MOB_DIR_DZ[e.mob_heading]) * walkSpeed();
}

bool canSpawnPassive(bool on_grass, int daylight)
{
    return on_grass && daylight >= DAYLIGHT_FOR_SPAWNING;
}
