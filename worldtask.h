#ifndef WORLDTASK_H
#define WORLDTASK_H

#include "task.h"
#include "world.h"
#include "gl.h"
#include "aabb.h"
#include "mining.h"

struct Entity;

class WorldTask : public Task
{
public:
    virtual void makeCurrent() override;

    virtual void logic() override;
    virtual void tick() override;
    virtual void render() override;

    void resetWorld();

    GLFix x, y = World::HEIGHT * Chunk::SIZE * BLOCK_SIZE, z, xr, yr;

    // The player occupies two full blocks; eye level stays 0.2 blocks below the head.
    static constexpr GLFix player_width = BLOCK_SIZE*0.8f, player_height = BLOCK_SIZE*2.0f, eye_pos = BLOCK_SIZE*1.8f;

    void setMessage(const char *message);

    // Runs every furnace in a loaded chunk for one tick (also while the furnace screen is open).
    void tickFurnaces();

    unsigned int frameCount() { return frame_counter; }

private:
    void crosshairPixel(int x, int y);
    void renderSky();
    void tickMining(bool on_ground);
    void tickMobs();
    void trySpawnPassive();
    void hitMob(Entity &e);
    Entity *aimedMob(GLFix ox, GLFix oy, GLFix oz, GLFix dx, GLFix dy, GLFix dz, GLFix block_dist);
    bool useBlock(int bx, int by, int bz);          // 7 on a chest, furnace, crafting table, smithing table or bed
    bool useBucket();                               // 7 with a bucket: scoop or pour water
    VECTOR3 besideSelection() const;                // the block against the face being aimed at
    bool placeBed(int fx, int fy, int fz);
    void removeBlock(int bx, int by, int bz, BLOCK_WDATA target);   // with chest contents and bed halves

    void getForward(GLFix *x, GLFix *z);
    void getRight(GLFix *x, GLFix *z);

    GLFix speed();

    //Player position and movement
    AABB aabb;
    bool can_jump = false, tp_had_contact = false;
    int tp_last_x = 0, tp_last_y = 0;
    GLFix vy = 0; //Y-Velocity for gravity and jumps
    bool in_water = false;
    bool creative_flight = false;
    bool jump_key_was_down = false;
    unsigned char jump_tap_ticks = 0; // Window for the second press of 5.

    int8_t input_forward = 0, input_strafe = 0; //Set by logic(), used by tick()
    bool input_jump = false, input_sprint = false, input_descend = false, input_mine = false;

    MiningState mining;
    uint32_t drop_rng = 0x5eed;
    uint32_t mob_rng = 0x6d6f62;
    int mob_spawn_timer = 0, mob_hit_cooldown = 0;
    Entity *aim_mob = nullptr;

    static constexpr int SLEEP_FADE_TICKS = 40;   // two seconds of fading
    int sleep_ticks = 0, sleep_x = 0, sleep_y = 0, sleep_z = 0;

    #ifdef STRESS_TEST
        bool stress_spawned = false;
        uint32_t stress_tick_count = 0, stress_tps = 0, stress_ticks_this_second = 0, stress_rtc_second = 0;
    #endif

    static constexpr unsigned int blockselection_frames = 2;
    unsigned int blockselection_frame = 0, blockselection_frame_fraction = 0;

    VECTOR3 selection_pos; AABB::SIDE selection_side; VECTOR3 selection_pos_abs; bool do_test = true; //For intersectsRay

    char message[40]; unsigned int message_timeout = 0;

    bool draw_inventory = true;
    unsigned int frame_counter = 0; // Incremented after each render
};

extern WorldTask world_task;

#endif // WORLDTASK_H
