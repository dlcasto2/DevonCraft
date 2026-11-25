#ifndef MOBS_H
#define MOBS_H

#include <cstdint>

#include "entity.h"
#include "items.h"

// Passive animals: pigs, cows and sheep that wander on grass by day. A mob is
// an Entity of type MOB whose mob_kind says which animal it is. Mobs are not
// saved, so a reload starts without them, the same as dropped items.

enum MobKind : uint8_t { MOB_PIG = 0, MOB_COW, MOB_SHEEP, MOB_TRADER, MOB_KIND_COUNT };

constexpr int MOB_MAX_PASSIVE = 6;             // most passive mobs in the world at once
constexpr int MOB_SPAWN_INTERVAL_TICKS = 200;  // ten seconds between spawn tries
constexpr int MOB_HIT_COOLDOWN_TICKS = 10;     // half a second between hits
constexpr int MOB_HIT_DAMAGE = 2;              // one heart per hit with bare hands
constexpr int MOB_HURT_FLASH_TICKS = 8;         // briefly show a red hit outline
constexpr int MOB_SHEEP_REGROW_TICKS = 200;      // ten seconds of grazing to regrow wool
constexpr int MOB_DESPAWN_BLOCKS = 48;         // mobs farther than this from the player go away

// Unit steps for headings 0-3: +x, +z, -x, -z.
constexpr int MOB_DIR_DX[4] = { 1, 0, -1, 0 };
constexpr int MOB_DIR_DZ[4] = { 0, 1, 0, -1 };

int mobMaxHealth(uint8_t kind);
ItemStack mobDrop(uint8_t kind);
ItemStack mobWoolDrop(const Entity &e);

// Shearing uses the world RNG and returns 1-3 wool blocks, or zero if unavailable.
int shearSheep(Entity &e, uint32_t &rng);
// Returns true exactly when a sheared sheep regrows its fleece while grazing.
bool tickSheepRegrowth(Entity &e, bool grazing);

// Deterministic LCG so a run is reproducible from its seed.
uint32_t mobRandom(uint32_t &rng);

// Spawns a mob of this kind in the box; nullptr when the pool is full.
Entity *spawnPassive(EntityPool &pool, uint8_t kind, const AABB &box);
int countPassive(const EntityPool &pool);
 
// Wandering traders share the passive mob wandering and rendering path.
Entity *spawnTrader(EntityPool &pool, const AABB &box);
int countTraders(const EntityPool &pool);

// Takes damage; true when the mob dies.
bool hurtMob(Entity &e, int damage);

// Chooses the walk or rest for this tick and sets the mob's velocity.
void steerMob(Entity &e, uint32_t &rng);

// Passive animals only spawn on grass, in daylight (daylightFactor is 0-256).
bool canSpawnPassive(bool on_grass, int daylight);

// Adds every mob's existing bounding box to player collision while delegating
// block collision and water detection to the normal world query.
class MobCollisionQuery : public CollisionQuery
{
public:
    MobCollisionQuery(const CollisionQuery &terrain, const EntityPool &entities)
        : terrain_(terrain), entities_(entities) {}

    bool intersects(const AABB &box) const override;
    bool intersectsForAutoJump(const AABB &box) const override;
    bool touchesWater(const AABB &box) const override;

private:
    const CollisionQuery &terrain_;
    const EntityPool &entities_;
};

// If a mob walks into the player between movement ticks, move the player just
// outside the overlapping hitbox before applying the next movement input.
bool separatePlayerFromMobs(AABB &player, const EntityPool &entities, const CollisionQuery &terrain);

#endif // MOBS_H
