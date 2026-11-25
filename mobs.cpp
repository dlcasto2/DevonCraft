#include "mobs.h"

#include "terrain.h"

#include <algorithm>

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
    if(kind == MOB_TRADER)
        return 0; // Traders are peaceful and cannot be killed.
    return kind == MOB_COW ? COW_HEALTH : PIG_HEALTH;
}

ItemStack mobDrop(uint8_t kind)
{
    if(kind == MOB_TRADER)
        return ItemStack();
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

Entity *spawnTrader(EntityPool &pool, const AABB &box)
{
    Entity *e = pool.spawn(EntityType::MOB, box);
    if(e)
    {
        e->mob_kind = MOB_TRADER;
        e->health = 0;
        e->mob_heading = 4;
    }
    return e;
}

int countTraders(const EntityPool &pool)
{
    int n = 0;
    pool.forEach([&n](const Entity &e) {
        if(e.type == EntityType::MOB && e.mob_kind == MOB_TRADER)
            ++n;
    });
    return n;
}

bool hurtMob(Entity &e, int damage)
{
    if(e.mob_kind == MOB_TRADER)
        return false;
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


namespace {

bool hitboxesOverlap(const AABB &a, const AABB &b)
{
    // Touching faces are not overlap; this lets the player stand beside/on top
    // of a mob without the hitbox behaving like an extra wall.
    return a.high_x > b.low_x && a.low_x < b.high_x
        && a.high_y > b.low_y && a.low_y < b.high_y
        && a.high_z > b.low_z && a.low_z < b.high_z;
}

AABB offsetBox(const AABB &box, GLFix dx, GLFix dz)
{
    return AABB(box.low_x + dx, box.low_y, box.low_z + dz,
                box.high_x + dx, box.high_y, box.high_z + dz);
}

bool overlapsAnotherMob(const AABB &box, const EntityPool &entities, const Entity *ignored)
{
    bool blocked = false;
    entities.forEach([&](const Entity &other) {
        if(!blocked && other.type == EntityType::MOB && &other != ignored
           && hitboxesOverlap(box, other.box))
            blocked = true;
    });
    return blocked;
}

} // namespace

bool MobCollisionQuery::intersects(const AABB &box) const
{
    if(terrain_.intersects(box))
        return true;

    bool blocked = false;
    entities_.forEach([&](const Entity &entity) {
        if(!blocked && entity.type == EntityType::MOB && hitboxesOverlap(box, entity.box))
            blocked = true;
    });
    return blocked;
}

bool MobCollisionQuery::intersectsForAutoJump(const AABB &box) const
{
    return terrain_.intersects(box);
}

bool MobCollisionQuery::touchesWater(const AABB &box) const
{
    return terrain_.touchesWater(box);
}

bool separatePlayerFromMobs(AABB &player, const EntityPool &entities, const CollisionQuery &terrain)
{
    bool moved = false;
    const GLFix epsilon = GLFix::minStep();

    // Two passes are enough for the small passive-mob cap, and avoid an
    // unbounded resolution loop if several entities overlap in a tight pen.
    for(int pass = 0; pass < 2; ++pass)
    {
        bool moved_this_pass = false;
        entities.forEach([&](const Entity &mob) {
            if(mob.type != EntityType::MOB || !hitboxesOverlap(player, mob.box))
                return;

            const GLFix overlap_x = std::min(player.high_x, mob.box.high_x)
                                  - std::max(player.low_x, mob.box.low_x);
            const GLFix overlap_z = std::min(player.high_z, mob.box.high_z)
                                  - std::max(player.low_z, mob.box.low_z);
            const GLFix player_cx = (player.low_x + player.high_x) / 2;
            const GLFix player_cz = (player.low_z + player.high_z) / 2;
            const GLFix mob_cx = (mob.box.low_x + mob.box.high_x) / 2;
            const GLFix mob_cz = (mob.box.low_z + mob.box.high_z) / 2;

            const auto tryMove = [&](GLFix dx, GLFix dz) -> bool {
                const AABB candidate = offsetBox(player, dx, dz);
                if(terrain.intersects(candidate) || overlapsAnotherMob(candidate, entities, &mob))
                    return false;
                player = candidate;
                moved_this_pass = true;
                moved = true;
                return true;
            };

            const auto tryAxis = [&](bool x_axis) -> bool {
                const GLFix overlap = (x_axis ? overlap_x : overlap_z) + epsilon;
                const GLFix center = x_axis ? player_cx : player_cz;
                const GLFix mob_center = x_axis ? mob_cx : mob_cz;
                const GLFix preferred = center >= mob_center ? overlap : -overlap;
                const GLFix dx = x_axis ? preferred : GLFix(0);
                const GLFix dz = x_axis ? GLFix(0) : preferred;
                if(tryMove(dx, dz))
                    return true;
                return tryMove(-dx, -dz);
            };

            // Push along the shallower overlap first; if terrain blocks that
            // side, try the opposite side and then the other horizontal axis.
            if(overlap_x <= overlap_z)
            {
                if(!tryAxis(true))
                    tryAxis(false);
            }
            else if(!tryAxis(false))
                tryAxis(true);
        });

        if(!moved_this_pass)
            break;
    }
    return moved;
}
