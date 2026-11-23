#ifndef MOBS_H
#define MOBS_H

#include <cstdint>

#include "entity.h"
#include "items.h"

// Passive animals: pigs and cows that wander on grass by day. A mob is an Entity
// of type MOB whose mob_kind says which animal it is. Mobs are not saved, so a
// reload starts without them, the same as dropped items.

enum MobKind : uint8_t { MOB_PIG = 0, MOB_COW, MOB_KIND_COUNT };

constexpr int MOB_MAX_PASSIVE = 6;             // most passive mobs in the world at once
constexpr int MOB_SPAWN_INTERVAL_TICKS = 200;  // ten seconds between spawn tries
constexpr int MOB_HIT_COOLDOWN_TICKS = 10;     // half a second between hits
constexpr int MOB_HIT_DAMAGE = 2;              // one heart per hit with bare hands
constexpr int MOB_DESPAWN_BLOCKS = 48;         // mobs farther than this from the player go away

// Unit steps for headings 0-3: +x, +z, -x, -z.
constexpr int MOB_DIR_DX[4] = { 1, 0, -1, 0 };
constexpr int MOB_DIR_DZ[4] = { 0, 1, 0, -1 };

int mobMaxHealth(uint8_t kind);
ItemStack mobDrop(uint8_t kind);

// Deterministic LCG so a run is reproducible from its seed.
uint32_t mobRandom(uint32_t &rng);

// Spawns a mob of this kind in the box; nullptr when the pool is full.
Entity *spawnPassive(EntityPool &pool, uint8_t kind, const AABB &box);
int countPassive(const EntityPool &pool);

// Takes damage; true when the mob dies.
bool hurtMob(Entity &e, int damage);

// Chooses the walk or rest for this tick and sets the mob's velocity.
void steerMob(Entity &e, uint32_t &rng);

// Pigs and cows only spawn on grass, in daylight (daylightFactor is 0-256).
bool canSpawnPassive(bool on_grass, int daylight);

#endif // MOBS_H
