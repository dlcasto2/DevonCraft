#ifndef PLAYER_MOTION_H
#define PLAYER_MOTION_H

#include <cstdint>

#include "physics.h"

// The original 1.3 controller moved the player per frame; the game now ticks at 20/s. These
// scale the per-frame distances by the old frame rate so motion feels the same.
GLFix velocityPerTick(GLFix per_frame, int fps);   // per_frame * fps / 20
GLFix accelPerTick(GLFix per_frame, int fps);      // per_frame * fps * fps / 400

struct PlayerInput {
    int8_t forward;   // -1, 0, 1
    int8_t strafe;
    bool jump;
    bool sprint;      // second press of forward key held after a double-tap
};

struct PlayerMotion {
    GLFix vy = 0;
    bool on_ground = false;
    bool swimming = false;   // the body was in water this tick
    bool sprinting = false;  // set by playerTick: sprint boost applied this tick
    bool sprint_stopped = false; // set by playerTick: a sprinting player ran into a wall
    // Momentum from a sprint jump, in world units per tick, carried forward and decayed.
    GLFix boost_x = 0, boost_z = 0;
};

// Sprint field of view: 1.8.8 widens the view by 15% while sprinting. The projection
// scales by the near plane, so widening the view means shrinking it. The ramp takes
// SPRINT_FOV_TICKS ticks to go from walking to full sprint, and back.
constexpr int SPRINT_FOV_TICKS = 5;
GLFix sprintNearPlane(GLFix base_near, int ramp);

// The part of the player that has to be in water to swim: as in 1.8.8, the
// box less 0.4 blocks at the top and bottom. Rising past the surface ends
// the swim with your feet about 0.4 blocks under it.
AABB swimBox(const AABB &player_box);

// A forward double-tap activates sprint while the second press stays held.
// The tap window is measured in simulation ticks (0.6 seconds at 20 TPS).
class DoubleTapSprint {
public:
    bool update(bool key_down)
    {
        const bool pressed = key_down && !key_was_down;
        key_was_down = key_down;
        if(pressed)
        {
            if(tap_window_ticks > 0)
            {
                sprinting = true;
                tap_window_ticks = 0;
            }
            else
                tap_window_ticks = 12;
        }
        if(!key_down)
            sprinting = false;
        return sprinting;
    }

    void tick()
    {
        if(tap_window_ticks > 0)
            --tap_window_ticks;
    }

    // Ends the sprint without waiting for the key, as when running into a wall.
    // Holding forward doesn't restart it: the next sprint needs a fresh double-tap.
    void stop()
    {
        sprinting = false;
        tap_window_ticks = 0;
    }

    void reset()
    {
        tap_window_ticks = 0;
        key_was_down = false;
        sprinting = false;
    }

private:
    uint8_t tap_window_ticks = 0;
    bool key_was_down = false;
    bool sprinting = false;
};

// One tick of player physics: walk by (forward, strafe) at yaw, jump, gravity,
// then collision. auto_jump hops one-block steps (Task 12). In water the
// player swims instead (see SWIM_SINK_PER_TICK), and pushing against a bank
// up to 0.6 blocks above the feet steps onto it, as 1.8.8 entities do.
void playerTick(const CollisionQuery &world, AABB &box, PlayerMotion &m, const PlayerInput &in,
                GLFix yaw, GLFix walk_per_tick, bool auto_jump);

// Collision-aware creative flight. Ascend and descend are independent controls;
// horizontal steering follows the same yaw-relative movement as walking.
void playerFlyTick(const CollisionQuery &world, AABB &box, int8_t forward, int8_t strafe,
                   bool ascend, bool descend, GLFix yaw, GLFix fly_per_tick);

#endif // PLAYER_MOTION_H
