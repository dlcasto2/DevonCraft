#include "player_motion.h"

#include "fastmath.h"
#include "terrain.h"

GLFix velocityPerTick(GLFix per_frame, int fps)
{
    return per_frame * fps / 20;
}

GLFix accelPerTick(GLFix per_frame, int fps)
{
    return per_frame * fps * fps / 400;
}

static AABB shifted(const AABB &box, GLFix dx, GLFix dy, GLFix dz)
{
    return AABB(box.low_x + dx, box.low_y + dy, box.low_z + dz, box.high_x + dx, box.high_y + dy, box.high_z + dz);
}

AABB swimBox(const AABB &b)
{
    const GLFix shrink = GLFix(BLOCK_SIZE) * 2 / 5;
    return AABB(b.low_x, b.low_y + shrink, b.low_z, b.high_x, b.high_y - shrink, b.high_z);
}

// 1.8.8 entities step up to 0.6 blocks; DevonCraft uses auto-jump on land, but
// in water this is what lets you climb out onto a bank.
static const GLFix SWIM_STEP = GLFix(BLOCK_SIZE) * 3 / 5;

static void swimTick(const CollisionQuery &world, AABB &box, PlayerMotion &m, const PlayerInput &in, GLFix dx, GLFix dz)
{
    // About half walking speed: 1.8.8 settles at 2 blocks/s in water.
    dx = dx / 2;
    dz = dz / 2;

    if(in.jump)
        m.vy += SWIM_UP_PER_TICK;

    const MoveResult r = moveWithCollision(world, box, dx, m.vy, dz);
    if(r.blocked_y)
        m.vy = 0;
    m.on_ground = r.on_ground;

    // Pushing against a bank that's at most SWIM_STEP above your feet steps onto it.
    if(r.blocked_x || r.blocked_z)
    {
        const GLFix sx = r.blocked_x ? dx : GLFix(0), sz = r.blocked_z ? dz : GLFix(0);
        const AABB up = shifted(box, 0, SWIM_STEP, 0), over = shifted(box, sx, SWIM_STEP, sz);
        if(!world.intersects(up) && !world.intersects(over))
        {
            box = over;
            m.vy = 0;
            return;
        }
    }

    m.vy = m.vy * 4 / 5 - SWIM_SINK_PER_TICK;
}

// Diagonal input is normalised, as in 1.8.8, so forward plus strafe isn't 41% faster.
static const GLFix DIAGONAL_SCALE(0.7071f);

// Forward and right, as (x, z) directions at yaw. Forward is (sin yaw, cos yaw).
static void facingAt(GLFix yaw, GLFix &fwd_x, GLFix &fwd_z, GLFix &right_x, GLFix &right_z)
{
    fwd_x = fast_sin(yaw);
    fwd_z = fast_cos(yaw);
    right_x = fast_sin((yaw + 90).normaliseAngle());
    right_z = fast_cos((yaw + 90).normaliseAngle());
}

void playerTick(const CollisionQuery &world, AABB &box, PlayerMotion &m, const PlayerInput &in,
                GLFix yaw, GLFix walk_per_tick, bool auto_jump)
{
    // Decide whether the player is swimming before applying the land sprint boost.
    m.swimming = world.touchesWater(swimBox(box));
    m.sprinting = in.sprint && in.forward > 0 && !m.swimming;
    m.sprint_stopped = false;
    const GLFix movement_speed = m.sprinting ? walk_per_tick * 13 / 10 : walk_per_tick;

    GLFix fwd_x, fwd_z, right_x, right_z;
    facingAt(yaw, fwd_x, fwd_z, right_x, right_z);

    const GLFix input_scale = in.forward != 0 && in.strafe != 0 ? DIAGONAL_SCALE : GLFix(1);
    const GLFix dx = (fwd_x * in.forward + right_x * in.strafe) * movement_speed * input_scale;
    const GLFix dz = (fwd_z * in.forward + right_z * in.strafe) * movement_speed * input_scale;
    if(m.swimming)
    {
        m.boost_x = m.boost_z = 0;
        return swimTick(world, box, m, in, dx, dz);
    }

    const bool ground_jump = in.jump && m.on_ground;
    bool jump = ground_jump;
    if(auto_jump && !jump && shouldAutoJump(world, box, dx, dz, m.on_ground, false))
        jump = true;

    if(jump)
    {
        m.vy = JUMP_VELOCITY;
        // A sprint jump carries the player forward. 1.8.8 adds 0.2 blocks a tick,
        // which is twice the walking speed of 0.1 blocks a tick at the default setting.
        if(ground_jump && m.sprinting)
        {
            m.boost_x = fwd_x * walk_per_tick * 2;
            m.boost_z = fwd_z * walk_per_tick * 2;
        }
    }

    m.vy -= GRAVITY_PER_TICK;

    const MoveResult r = moveWithCollision(world, box, dx + m.boost_x, m.vy, dz + m.boost_z);
    if(r.blocked_y)
        m.vy = 0;
    m.on_ground = r.on_ground;

    if(m.sprinting && (r.blocked_x || r.blocked_z))
        m.sprint_stopped = true;

    // Momentum decays as in 1.8.8: the air keeps 91% of it a tick, the ground friction 55%.
    m.boost_x = m.on_ground ? m.boost_x * 546 / 1000 : m.boost_x * 91 / 100;
    m.boost_z = m.on_ground ? m.boost_z * 546 / 1000 : m.boost_z * 91 / 100;
}

GLFix sprintNearPlane(GLFix base_near, int ramp)
{
    if(ramp < 0) ramp = 0;
    if(ramp > SPRINT_FOV_TICKS) ramp = SPRINT_FOV_TICKS;
    // Full sprint is 1.15 times wider, so the near plane shrinks to 100/115 of its base.
    return base_near * 100 / (100 + 15 * ramp / SPRINT_FOV_TICKS);
}

void playerFlyTick(const CollisionQuery &world, AABB &box, int8_t forward, int8_t strafe,
                   bool ascend, bool descend, GLFix yaw, GLFix fly_per_tick)
{
    // Creative flight stays horizontal relative to the player's facing direction.
    const GLFix fwd_x = fast_sin(yaw), fwd_z = fast_cos(yaw);
    const GLFix right_x = fast_sin((yaw + 90).normaliseAngle());
    const GLFix right_z = fast_cos((yaw + 90).normaliseAngle());

    const GLFix dx = (fwd_x * forward + right_x * strafe) * fly_per_tick;
    const GLFix dz = (fwd_z * forward + right_z * strafe) * fly_per_tick;
    const GLFix dy = (ascend ? fly_per_tick : GLFix(0))
                   - (descend ? fly_per_tick : GLFix(0));

    // No gravity or water drag while flying. All three axes still respect blocks.
    moveWithCollision(world, box, dx, dy, dz);
}
