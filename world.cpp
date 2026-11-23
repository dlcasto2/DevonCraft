#include <random>
#include <new>
#include <cstdlib>
#include <libndls.h>

#include "blockrenderer.h"
#include "blockdefs.h"
#include "world.h"
#include "chunk.h"
#include "settingstask.h"
#include "platform_time.h"

World world;

World::World() : perlin_noise(0), seed(0)
{
    generateSeed();
}

World::~World()
{
    for(auto&& c : all_chunks)
        delete c.second;
}

void World::generateSeed()
{
    // The previous implementation read an uninitialized word returned by
    // malloc as the seed. Store it directly and initialize it from the RTC.
    seed = platformRtcSeconds() ^ 0x9E3779B9u;
    printf("Seed: %u\n", seed);
    perlin_noise.setSeed(seed);
}

constexpr int getLocal(const int global)
{
     static_assert(Chunk::SIZE == 8, "Update the bit operations accordingly!");

     return global & 0b111;
}

constexpr int getChunk(const int global)
{
    return global >> 3;
}

BLOCK_WDATA World::getBlock(const int x, const int y, const int z) const
{
    int chunk_x = getChunk(x), chunk_y = getChunk(y), chunk_z = getChunk(z);

    Chunk *c = findChunk(chunk_x, chunk_y, chunk_z);
    if(!c)
    {
        //Don't render world edges except for the top
        if(chunk_y == World::HEIGHT)
            return BLOCK_AIR;
        else
            return BLOCK_STONE;
    }

    return c->getLocalBlock(getLocal(x), getLocal(y), getLocal(z));
}

void World::setBlock(const int x, const int y, const int z, const BLOCK_WDATA block, bool set_dirty)
{
    int chunk_x = getChunk(x), chunk_y = getChunk(y), chunk_z = getChunk(z);
    int local_x = getLocal(x), local_y = getLocal(y), local_z = getLocal(z);

    Chunk *c = findChunk(chunk_x, chunk_y, chunk_z);
    if(c)
        c->setLocalBlock(local_x, local_y, local_z, block, set_dirty);
    else
        pending_block_changes.push_back({chunk_x, chunk_y, chunk_z, local_x, local_y, local_z, block});
}

void World::changeBlock(const int x, const int y, const int z, const BLOCK_WDATA block)
{
    int chunk_x = getChunk(x), chunk_y = getChunk(y), chunk_z = getChunk(z);
    int local_x = getLocal(x), local_y = getLocal(y), local_z = getLocal(z);

    Chunk *c = findChunk(chunk_x, chunk_y, chunk_z);
    if(c)
        c->changeLocalBlock(local_x, local_y, local_z, block);
    else
        pending_block_changes.push_back({chunk_x, chunk_y, chunk_z, local_x, local_y, local_z, block});
}

bool World::setChunkVisible(int x, int y, int z)
{
    Chunk *c = findChunk(x, y, z);
    if(!c)
        c = generateChunk(x, y, z);

    if(!c)
        return false;

    visible_chunks.push_back(c);
    return true;
}

void World::setPosition(int x, int y, int z)
{
    int chunk_x = getChunk(positionToBlock(x)),
        chunk_y = getChunk(positionToBlock(y)),
        chunk_z = getChunk(positionToBlock(z));

    chunk_y = std::max(0, std::min(chunk_y, World::HEIGHT - 1));

    if(!loaded || (chunk_x != cen_x || chunk_y != cen_y || chunk_z != cen_z))
    {
        // The original monochrome CAS has a much tighter memory budget than
        // CX hardware. Keep its active radius bounded even when Low graphics
        // is switched off; this prevents large synchronous startup allocations.
        const bool low_graphics = settings_task.getValue(SettingsTask::FAST_MODE) != 0;
        // Preserve the classic calculator's tighter memory cap; CX Low graphics
        // uses the radius-3 preset from main.
        const int dist = is_classic ? std::min(field_of_view, 2)
            : (low_graphics ? std::min(field_of_view, 3) : field_of_view);
        int distsq = dist*dist;

        // Turn the + shape into a cube to load the directly adjacent corners.
        // This avoids worse graphics and bad possibly collision issues.
        if(distsq == 1)
            distsq = 3;

        // Release derived geometry only for chunks that leave the next active
        // region. Their block arrays remain resident so all world edits survive.
        for(Chunk *old_chunk : visible_chunks)
        {
            const int dx = old_chunk->x - chunk_x;
            const int dy = old_chunk->y - chunk_y;
            const int dz = old_chunk->z - chunk_z;
            if(dx*dx + dy*dy + dz*dz > distsq
               || old_chunk->y < 0 || old_chunk->y >= World::HEIGHT)
                old_chunk->releaseGeometry();
        }
        visible_chunks.clear();

        bool all_chunks_ready = true;
        for(int x = -dist; x <= dist; ++x)
            for(int y = -dist; y <= dist; ++y)
                for(int z = -dist; z <= dist; ++z)
                {
                    if(chunk_y + y < 0 || chunk_y + y >= World::HEIGHT)
                        continue;

                    if(x*x + y*y + z*z > distsq)
                        continue;

                    if(!setChunkVisible(chunk_x + x, chunk_y + y, chunk_z + z))
                        all_chunks_ready = false;
                }

        cen_x = chunk_x;
        cen_y = chunk_y;
        cen_z = chunk_z;

        // If an allocation failed, retry the missing chunks on the next
        // logic pass instead of permanently accepting an incomplete region.
        loaded = all_chunks_ready;
        dynamic_light_sources_dirty = true;
    }
}

bool World::blockAction(const int x, const int y, const int z)
{
    int chunk_x = getChunk(x), chunk_y = getChunk(y), chunk_z = getChunk(z);
    int local_x = getLocal(x), local_y = getLocal(y), local_z = getLocal(z);

    Chunk *c = findChunk(chunk_x, chunk_y, chunk_z);
    if(!c)
        return false;

    return global_block_renderer.action(c->getLocalBlock(local_x, local_y, local_z), local_x, local_y, local_z, *c);
}

bool World::intersect(AABB &other) const
{
    for(Chunk *c : visible_chunks)
        if(c->intersects(other))
            return true;

    return false;
}

bool World::intersectsRay(GLFix x, GLFix y, GLFix z, GLFix dx, GLFix dy, GLFix dz, VECTOR3 &result, AABB::SIDE &side, GLFix &dist, bool hit_water_sources) const
{
    dist = GLFix::maxValue();
    VECTOR3 pos;
    for(Chunk *c : visible_chunks)
    {
        GLFix new_dist;
        AABB::SIDE new_side = AABB::NONE;

        if(c->intersectsRay(x, y, z, dx, dy, dz, new_dist, pos, new_side, hit_water_sources))
        {
            if(new_dist > dist)
                continue;

            result.x = pos.x + c->x*Chunk::SIZE;
            result.y = pos.y + c->y*Chunk::SIZE;
            result.z = pos.z + c->z*Chunk::SIZE;
            side = new_side;
            dist = new_dist;
        }
    }

    return dist != GLFix::maxValue();
}

const PerlinNoise &World::noiseGenerator() const
{
    return perlin_noise;
}


void World::clear()
{
    for(auto &&c : all_chunks)
        delete c.second;

    all_chunks.clear();
    visible_chunks.clear();
    pending_block_changes.clear();

    cen_x = cen_y = cen_z = 0;
    loaded = false;
    dynamic_light_count = 0;
    dynamic_light_refresh_ticks = 0;
    dynamic_light_sources_dirty = true;
}

void World::setDirty()
{
    for(auto &&c : all_chunks)
        c.second->setDirty();
}

#define LOAD_FROM_FILE(var) if(gzfread(&(var), sizeof(var), 1, file) != 1) return false;
#define SAVE_TO_FILE(var) if(gzfwrite(&(var), sizeof(var), 1, file) != 1) return false;

bool World::loadFromFile(gzFile file)
{
    drawLoadingtext(1);

    clear();

    LOAD_FROM_FILE(seed)
    perlin_noise.setSeed(seed);

    unsigned int block_changes;
    LOAD_FROM_FILE(block_changes)
    pending_block_changes.resize(block_changes);

    if(gzfread(pending_block_changes.data(), sizeof(BLOCK_CHANGE), block_changes, file) != block_changes)
        return false;

    LOAD_FROM_FILE(field_of_view);

    for(;;)
    {
        int x, y, z;
        if(gzfread(&x, sizeof(x), 1, file) != 1)
            return gzeof(file);

        LOAD_FROM_FILE(y)
        LOAD_FROM_FILE(z)

        Chunk *c = new (std::nothrow) Chunk(x, y, z);
        if(!c)
            return false;

        if(!c->loadFromFile(file))
        {
            delete c;
            return false;
        }

        const auto inserted = all_chunks.insert({std::tuple<int,int,int>(x, y, z), c});
        if(!inserted.second)
        {
            delete c;
            return false;
        }
    }
}

bool World::saveToFile(gzFile file) const
{
    drawLoadingtext(1);

    SAVE_TO_FILE(seed)

    unsigned int block_changes = pending_block_changes.size();
    SAVE_TO_FILE(block_changes)

    if(gzfwrite(pending_block_changes.data(), sizeof(BLOCK_CHANGE), block_changes, file) != block_changes)
        return false;

    SAVE_TO_FILE(field_of_view);

    for(auto&& c : all_chunks)
    {
        SAVE_TO_FILE(c.second->x)
        SAVE_TO_FILE(c.second->y)
        SAVE_TO_FILE(c.second->z)

        if(!c.second->saveToFile(file))
            return false;
    }

    return true;
}

void World::render()
{
    bool ticks_enabled = settings_task.getValue(SettingsTask::TICKS_ENABLED);
    for(Chunk *c : visible_chunks)
        c->logic(ticks_enabled);

    for(Chunk *c : visible_chunks)
        c->render();
}

bool World::isLoaded(int x, int y, int z) const
{
    return findChunk(getChunk(x), getChunk(y), getChunk(z)) != nullptr;
}

Chunk* World::findChunk(int x, int y, int z) const
{
    auto&& c = all_chunks.find(std::tuple<int,int,int>{x, y, z});
    if(c == all_chunks.end())
        return nullptr;
    else
        return c->second;
}

void World::spawnDestructionParticles(int x, int y, int z)
{
    auto *c = findChunk(getChunk(x), getChunk(y), getChunk(z));
    int cx = getLocal(x), cy = getLocal(y), cz = getLocal(z);
    return c->spawnDestructionParticles(cx, cy, cz);
}

Chunk* World::generateChunk(int x, int y, int z)
{
    drawLoadingtext(2);

    if(Chunk *c = findChunk(x - 1, y, z))
        c->setDirty();
    if(Chunk *c = findChunk(x + 1, y, z))
        c->setDirty();
    if(Chunk *c = findChunk(x, y - 1, z))
        c->setDirty();
    if(Chunk *c = findChunk(x, y + 1, z))
        c->setDirty();
    if(Chunk *c = findChunk(x, y, z - 1))
        c->setDirty();
    if(Chunk *c = findChunk(x, y, z + 1))
        c->setDirty();

    Chunk *c = new (std::nothrow) Chunk(x, y, z);
    if(c == nullptr)
        return c;

    c->generate();
    all_chunks.insert({std::tuple<int,int,int>(x, y, z), c});

    for(auto it = pending_block_changes.begin(); it != pending_block_changes.end();)
    {
        BLOCK_CHANGE &block_change = *it;
        if(c->x == block_change.chunk_x && c->y == block_change.chunk_y && c->z == block_change.chunk_z)
        {
            c->setLocalBlock(block_change.local_x, block_change.local_y, block_change.local_z, block_change.block);
            it = pending_block_changes.erase(it);
        }
        else
            ++it;
    }

    return c;
}

WorldCollision world_collision;

bool WorldCollision::intersects(const AABB &box) const
{
    AABB copy = box;   // World::intersect takes a non-const reference
    return world.intersect(copy);
}

bool WorldCollision::touchesWater(const AABB &box) const
{
    // Every block the box overlaps; a box that only touches a block doesn't count.
    const int x0 = (box.low_x / BLOCK_SIZE).floor(), x1 = ((box.high_x - GLFix(1)) / BLOCK_SIZE).floor();
    const int y0 = (box.low_y / BLOCK_SIZE).floor(), y1 = ((box.high_y - GLFix(1)) / BLOCK_SIZE).floor();
    const int z0 = (box.low_z / BLOCK_SIZE).floor(), z1 = ((box.high_z - GLFix(1)) / BLOCK_SIZE).floor();
    for(int x = x0; x <= x1; ++x)
        for(int y = y0; y <= y1; ++y)
            for(int z = z0; z <= z1; ++z)
                if(getBLOCK(world.getBlock(x, y, z)) == BLOCK_WATER)
                    return true;
    return false;
}

void World::setBrightnessAll(int level)
{
    for(Chunk *c : visible_chunks)
        c->setBrightness(level);
}

void World::updateDynamicLighting(int ambient_level, int player_x, int player_y, int player_z, int hand_light_level)
{
    if(ambient_level < 0) ambient_level = 0;
    if(ambient_level >= BRIGHTNESS_LEVELS) ambient_level = BRIGHTNESS_LEVELS - 1;

    // At full daylight there is no brighter level to show. Defer source scans
    // until dusk, while still preserving invalidation from light block changes.
    if(ambient_level == BRIGHTNESS_LEVELS - 1 && hand_light_level <= 0)
    {
        for(Chunk *chunk : visible_chunks)
            chunk->setBrightness(ambient_level);
        return;
    }

    // Block changes invalidate this cache. The periodic refresh also catches
    // light state transitions that happen inside block tick handlers.
    if(dynamic_light_sources_dirty || ++dynamic_light_refresh_ticks >= 8)
    {
        dynamic_light_count = 0;
        for(Chunk *source_chunk : visible_chunks)
        {
            for(int x = 0; x < Chunk::SIZE; ++x)
                for(int y = 0; y < Chunk::SIZE; ++y)
                    for(int z = 0; z < Chunk::SIZE; ++z)
                    {
                        const BLOCK_WDATA block = source_chunk->getLocalBlock(x, y, z);
                        const int emission = blockLightLevel(block);
                        if(emission <= 0)
                            continue;

                        DynamicLightSource source = {
                            source_chunk->x * Chunk::SIZE + x,
                            source_chunk->y * Chunk::SIZE + y,
                            source_chunk->z * Chunk::SIZE + z,
                            static_cast<uint8_t>(emission)
                        };

                        if(dynamic_light_count < MAX_DYNAMIC_LIGHT_SOURCES)
                        {
                            dynamic_light_sources[dynamic_light_count++] = source;
                        }
                        else
                        {
                            // Keep the closest 128 emitters if a heavily lit
                            // build exceeds the fixed calculator-side cache.
                            unsigned int farthest = 0;
                            long long farthest_distance = -1;
                            for(unsigned int i = 0; i < dynamic_light_count; ++i)
                            {
                                const long long dx = dynamic_light_sources[i].x - player_x;
                                const long long dy = dynamic_light_sources[i].y - player_y;
                                const long long dz = dynamic_light_sources[i].z - player_z;
                                const long long distance = dx*dx + dy*dy + dz*dz;
                                if(distance > farthest_distance)
                                {
                                    farthest_distance = distance;
                                    farthest = i;
                                }
                            }

                            const long long dx = source.x - player_x;
                            const long long dy = source.y - player_y;
                            const long long dz = source.z - player_z;
                            if(dx*dx + dy*dy + dz*dz < farthest_distance)
                                dynamic_light_sources[farthest] = source;
                        }
                    }
        }

        dynamic_light_sources_dirty = false;
        dynamic_light_refresh_ticks = 0;
    }

    for(Chunk *target : visible_chunks)
    {
        int level = ambient_level;
        const int center_x = target->x * Chunk::SIZE + Chunk::SIZE / 2;
        const int center_y = target->y * Chunk::SIZE + Chunk::SIZE / 2;
        const int center_z = target->z * Chunk::SIZE + Chunk::SIZE / 2;

        for(unsigned int i = 0; i < dynamic_light_count; ++i)
        {
            const DynamicLightSource &source = dynamic_light_sources[i];
            int attenuation = 0;
            const bool same_chunk =
                source.x >= target->x * Chunk::SIZE && source.x < (target->x + 1) * Chunk::SIZE &&
                source.y >= target->y * Chunk::SIZE && source.y < (target->y + 1) * Chunk::SIZE &&
                source.z >= target->z * Chunk::SIZE && source.z < (target->z + 1) * Chunk::SIZE;

            if(!same_chunk)
            {
                const int distance = std::abs(source.x - center_x)
                                   + std::abs(source.y - center_y)
                                   + std::abs(source.z - center_z);
                attenuation = distance - (Chunk::SIZE * 3 / 4);
                if(attenuation < 0)
                    attenuation = 0;
            }
            level = combineDynamicLight(level, source.level, attenuation);
            if(level == BRIGHTNESS_LEVELS - 1)
                break;
        }

        // A selected light source follows the player and is evaluated every
        // tick; placed sources use cached block coordinates.
        if(hand_light_level > 0 && level < BRIGHTNESS_LEVELS - 1)
        {
            int attenuation = 0;
            const bool same_chunk =
                player_x >= target->x * Chunk::SIZE && player_x < (target->x + 1) * Chunk::SIZE &&
                player_y >= target->y * Chunk::SIZE && player_y < (target->y + 1) * Chunk::SIZE &&
                player_z >= target->z * Chunk::SIZE && player_z < (target->z + 1) * Chunk::SIZE;
            if(!same_chunk)
            {
                const int distance = std::abs(player_x - center_x)
                                   + std::abs(player_y - center_y)
                                   + std::abs(player_z - center_z);
                attenuation = distance - (Chunk::SIZE * 3 / 4);
                if(attenuation < 0)
                    attenuation = 0;
            }
            level = combineDynamicLight(level, hand_light_level, attenuation);
        }

        target->setBrightness(level);
    }
}
