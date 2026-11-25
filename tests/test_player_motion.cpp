#include "doctest.h"
#include "player_motion.h"
#include "grid_world.h"

#include <cmath>

static GridWorld floorWorld() {
    GridWorld w;
    for (int x = -3; x <= 3; ++x) for (int z = -3; z <= 3; ++z) w.solid.insert(std::make_tuple(x, 0, z));
    return w;
}
TEST_CASE("tick conversion") {
    CHECK(velocityPerTick(GLFix(20), 10) == GLFix(10));
    CHECK(velocityPerTick(GLFix(50), 20) == GLFix(50));
    CHECK(accelPerTick(GLFix(5), 10) == GLFix(5) / 4);
}
TEST_CASE("walking forward at yaw 0 moves along +z") {
    GridWorld w = floorWorld(); AABB box = playerBoxAt(64, 128, 64); PlayerMotion m; m.on_ground = true;
    GLFix z0 = box.low_z;
    playerTick(w, box, m, PlayerInput{1, 0, false}, GLFix(0), GLFix(10), false);
    CHECK((box.low_z - z0).floor() >= 9); CHECK((box.low_z - z0).floor() <= 10);
}
TEST_CASE("double-tap forward activates sprint only while the second press is held") {
    DoubleTapSprint sprint;

    CHECK_FALSE(sprint.update(true));  // first press only starts the tap window
    CHECK_FALSE(sprint.update(true));  // holding the key is not another tap
    CHECK_FALSE(sprint.update(false)); // release between taps
    sprint.tick();
    sprint.tick();
    CHECK(sprint.update(true));        // second press activates sprint
    CHECK(sprint.update(true));        // holding the second press keeps sprint active
    CHECK_FALSE(sprint.update(false)); // releasing forward ends sprint
}

TEST_CASE("double-tap sprint expires when the second press is too late") {
    DoubleTapSprint sprint;
    CHECK_FALSE(sprint.update(true));
    CHECK_FALSE(sprint.update(false));
    for(int i = 0; i < 12; ++i)
        sprint.tick();
    CHECK_FALSE(sprint.update(true)); // this is a new first press, not a sprint
}

TEST_CASE("double-tap sprint reset clears pending taps and active state") {
    DoubleTapSprint sprint;
    CHECK_FALSE(sprint.update(true));
    sprint.reset();
    CHECK_FALSE(sprint.update(false));
    CHECK_FALSE(sprint.update(true));
}

TEST_CASE("holding sprint with forward increases ground speed by 30 percent") {
    GridWorld w = floorWorld();
    AABB walking = playerBoxAt(64, 128, 64), sprinting = playerBoxAt(64, 128, 64);
    PlayerMotion walk_motion, sprint_motion;
    walk_motion.on_ground = sprint_motion.on_ground = true;
    const GLFix start_z = walking.low_z;

    playerTick(w, walking, walk_motion, PlayerInput{1, 0, false, false}, GLFix(0), GLFix(10), false);
    playerTick(w, sprinting, sprint_motion, PlayerInput{1, 0, false, true}, GLFix(0), GLFix(10), false);

    const GLFix walk_distance = walking.low_z - start_z;
    const GLFix sprint_distance = sprinting.low_z - start_z;
    CHECK(sprint_distance > walk_distance);
    CHECK(sprint_distance.floor() >= 12);
    CHECK(sprint_distance.floor() <= 13);
}

TEST_CASE("sprint does not boost backward movement") {
    GridWorld w = floorWorld();
    AABB walking = playerBoxAt(64, 128, 64), sprinting = playerBoxAt(64, 128, 64);
    PlayerMotion walk_motion, sprint_motion;
    walk_motion.on_ground = sprint_motion.on_ground = true;
    const GLFix start_z = walking.low_z;

    playerTick(w, walking, walk_motion, PlayerInput{-1, 0, false, false}, GLFix(0), GLFix(10), false);
    playerTick(w, sprinting, sprint_motion, PlayerInput{-1, 0, false, true}, GLFix(0), GLFix(10), false);

    CHECK(sprinting.low_z - start_z == walking.low_z - start_z);
}

TEST_CASE("jump only from the ground") {
    GridWorld w = floorWorld(); AABB box = playerBoxAt(64, 128, 64); PlayerMotion m; m.on_ground = true;
    playerTick(w, box, m, PlayerInput{0, 0, true}, GLFix(0), GLFix(10), false);
    CHECK(box.low_y > GLFix(128));
    GLFix y1 = box.low_y; GLFix vy1 = m.vy;
    playerTick(w, box, m, PlayerInput{0, 0, true}, GLFix(0), GLFix(10), false);
    CHECK(m.vy < vy1); CHECK(box.low_y > y1);
}

// Match Chunk::intersects: touching a block face still counts as collision.
struct TouchingGridWorld : GridWorld {
    bool intersects(const AABB &box) const override {
        for(const auto &cell : solid) {
            AABB block(GLFix(std::get<0>(cell) * BLOCK_SIZE),
                       GLFix(std::get<1>(cell) * BLOCK_SIZE),
                       GLFix(std::get<2>(cell) * BLOCK_SIZE),
                       GLFix((std::get<0>(cell) + 1) * BLOCK_SIZE),
                       GLFix((std::get<1>(cell) + 1) * BLOCK_SIZE),
                       GLFix((std::get<2>(cell) + 1) * BLOCK_SIZE));
            AABB candidate = box;
            if(block.intersects(candidate))
                return true;
        }
        return false;
    }
};

TEST_CASE("player can start a jump in a one-block-wide two-block-tall tunnel") {
    TouchingGridWorld w;
    for(int z = -1; z <= 1; ++z) {
        w.solid.insert(std::make_tuple(0, 0, z));   // floor
        w.solid.insert(std::make_tuple(0, 3, z));   // ceiling: two air blocks above the floor
        w.solid.insert(std::make_tuple(-1, 1, z));  // one-block-wide passage walls
        w.solid.insert(std::make_tuple(-1, 2, z));
        w.solid.insert(std::make_tuple(1, 1, z));
        w.solid.insert(std::make_tuple(1, 2, z));
    }

    AABB box = playerBoxAt(64, BLOCK_SIZE, 64);
    PlayerMotion motion;
    motion.on_ground = true;
    const GLFix start_y = box.low_y;

    playerTick(w, box, motion, PlayerInput{0, 0, true}, GLFix(0), GLFix(10), false);

    CHECK(box.low_y > start_y);
    CHECK(box.high_y < GLFix(3 * BLOCK_SIZE));
    CHECK(box.low_x > GLFix(0));
    CHECK(box.high_x < GLFix(BLOCK_SIZE));
}

TEST_CASE("creative flight moves vertically and respects ceiling and floor collisions") {
    GridWorld w;
    w.solid.insert(std::make_tuple(0, 0, 0)); // floor
    w.solid.insert(std::make_tuple(0, 3, 0)); // ceiling
    AABB box = playerBoxAt(64, 128, 64);

    playerFlyTick(w, box, 0, 0, true, false, GLFix(0), GLFix(16));
    CHECK(box.low_y == GLFix(144));

    // With the 1.7-block player collider, a second 16-unit ascent still fits;
    // the third would intersect the ceiling and must be blocked.
    playerFlyTick(w, box, 0, 0, true, false, GLFix(0), GLFix(16));
    CHECK(box.low_y == GLFix(160));
    playerFlyTick(w, box, 0, 0, true, false, GLFix(0), GLFix(16));
    CHECK(box.low_y == GLFix(160));

    playerFlyTick(w, box, 0, 0, false, true, GLFix(0), GLFix(16));
    CHECK(box.low_y == GLFix(144));
    playerFlyTick(w, box, 0, 0, false, true, GLFix(0), GLFix(16));
    CHECK(box.low_y == GLFix(128));

    // Descending cannot clip the player through the floor.
    playerFlyTick(w, box, 0, 0, false, true, GLFix(0), GLFix(16));
    CHECK(box.low_y == GLFix(128));
}

TEST_CASE("creative flight keeps yaw-relative horizontal steering") {
    GridWorld w;
    AABB box = playerBoxAt(64, 128, 64);
    const GLFix z0 = box.low_z;
    playerFlyTick(w, box, 1, 0, false, false, GLFix(0), GLFix(16));
    CHECK(box.low_z > z0);
    CHECK(box.low_y == GLFix(128)); // no gravity while hovering
}

static GridWorld stepWorld() {
    GridWorld w; for (int x = -3; x <= 3; ++x) w.solid.insert(std::make_tuple(x, 0, 0));
    w.solid.insert(std::make_tuple(0, 1, 1));
    return w;
}
TEST_CASE("auto-jump climbs a step when on") {
    GridWorld w = stepWorld(); AABB box = playerBoxAt(64, 128, 70); PlayerMotion m; m.on_ground = true;
    playerTick(w, box, m, PlayerInput{1, 0, false}, GLFix(0), GLFix(20), true);
    CHECK(box.low_y > GLFix(128));
}
TEST_CASE("no auto-jump when off") {
    GridWorld w = stepWorld(); AABB box = playerBoxAt(64, 128, 70); PlayerMotion m; m.on_ground = true;
    playerTick(w, box, m, PlayerInput{1, 0, false}, GLFix(0), GLFix(20), false);
    CHECK(box.low_y == GLFix(128));
}

// A pool: water in cells (x, 1..depth, z) over a stone floor at y 0.
static GridWorld poolWorld(int depth) {
    GridWorld w;
    for (int x = -6; x <= 6; ++x) for (int z = -6; z <= 6; ++z) {
        w.solid.insert(std::make_tuple(x, 0, z));
        for (int y = 1; y <= depth; ++y) w.water.insert(std::make_tuple(x, y, z));
    }
    return w;
}
static GLFix blocksPerTick(GLFix units) { return units / BLOCK_SIZE; }

TEST_CASE("you only swim with your body in the water") {
    GridWorld w = poolWorld(3);
    CHECK(w.touchesWater(swimBox(playerBoxAt(64, 128, 64))));
    CHECK(w.touchesWater(swimBox(playerBoxAt(64, 4 * 128 - 60, 64))));    // feet just under the surface
    CHECK_FALSE(w.touchesWater(swimBox(playerBoxAt(64, 4 * 128 - 40, 64))));
    CHECK_FALSE(poolWorld(0).touchesWater(swimBox(playerBoxAt(64, 128, 64))));
}

TEST_CASE("sinking in water settles at 2 blocks a second") {
    GridWorld w = poolWorld(30); AABB box = playerBoxAt(64, 25 * 128, 64); PlayerMotion m;
    m.vy = -60;   // falling in fast
    for (int i = 0; i < 40; ++i) playerTick(w, box, m, PlayerInput{0, 0, false}, GLFix(0), GLFix(20), false);
    CHECK(m.swimming);
    const GLFix y0 = box.low_y;
    playerTick(w, box, m, PlayerInput{0, 0, false}, GLFix(0), GLFix(20), false);
    const GLFix per_tick = blocksPerTick(y0 - box.low_y);
    CHECK(per_tick > GLFix(0.09f)); CHECK(per_tick < GLFix(0.11f));
}

TEST_CASE("holding jump swims up at 2 blocks a second") {
    GridWorld w = poolWorld(30); AABB box = playerBoxAt(64, 5 * 128, 64); PlayerMotion m;
    for (int i = 0; i < 40; ++i) playerTick(w, box, m, PlayerInput{0, 0, true}, GLFix(0), GLFix(20), false);
    const GLFix y0 = box.low_y;
    playerTick(w, box, m, PlayerInput{0, 0, true}, GLFix(0), GLFix(20), false);
    const GLFix per_tick = blocksPerTick(box.low_y - y0);
    CHECK(per_tick > GLFix(0.09f)); CHECK(per_tick < GLFix(0.11f));
}

TEST_CASE("swimming up stops with the feet just under the surface") {
    GridWorld w = poolWorld(4); AABB box = playerBoxAt(64, 128, 64); PlayerMotion m;
    GLFix highest = 0;
    for (int i = 0; i < 200; ++i) {
        playerTick(w, box, m, PlayerInput{0, 0, true}, GLFix(0), GLFix(20), false);
        if (i > 100 && box.low_y > highest) highest = box.low_y;
    }
    // the surface is at y 5 (640): bobbing there, never out of the water
    CHECK(highest > GLFix(5 * 128 - 70)); CHECK(highest < GLFix(5 * 128));
}

TEST_CASE("walking in water is half speed, and sprint does not change it") {
    GridWorld w = poolWorld(2);
    AABB walking = playerBoxAt(64, 128, 64), sprinting = playerBoxAt(64, 128, 64);
    PlayerMotion walk_motion, sprint_motion;
    walk_motion.on_ground = sprint_motion.on_ground = true;
    const GLFix start_z = walking.low_z;

    playerTick(w, walking, walk_motion, PlayerInput{1, 0, false, false}, GLFix(0), GLFix(20), true);
    playerTick(w, sprinting, sprint_motion, PlayerInput{1, 0, false, true}, GLFix(0), GLFix(20), true);

    CHECK(walk_motion.swimming);
    CHECK(sprint_motion.swimming);
    CHECK(sprinting.low_z - start_z == walking.low_z - start_z);
    CHECK((walking.low_z - start_z).floor() >= 9);
    CHECK((walking.low_z - start_z).floor() <= 10);
}

TEST_CASE("pushing against a bank near the surface steps out of the water") {
    // Water 2 deep for z < 2; the bank at z >= 2 is level with the surface (top at y 3).
    GridWorld w;
    for (int x = -4; x <= 4; ++x) for (int z = -4; z <= 6; ++z) {
        w.solid.insert(std::make_tuple(x, 0, z));
        if (z < 2) { w.water.insert(std::make_tuple(x, 1, z)); w.water.insert(std::make_tuple(x, 2, z)); }
        else { w.solid.insert(std::make_tuple(x, 1, z)); w.solid.insert(std::make_tuple(x, 2, z)); }
    }
    AABB box = playerBoxAt(64, 128, 64 + 128 - 52); PlayerMotion m;   // against the bank, on the floor
    // Walking along the bottom gets nowhere...
    for (int i = 0; i < 20; ++i) playerTick(w, box, m, PlayerInput{1, 0, false}, GLFix(0), GLFix(20), false);
    CHECK(box.low_y < GLFix(2 * 128));
    // ...but swimming up while pushing climbs onto the bank.
    for (int i = 0; i < 60; ++i) playerTick(w, box, m, PlayerInput{1, 0, true}, GLFix(0), GLFix(20), false);
    CHECK(box.low_y >= GLFix(3 * 128)); CHECK(box.low_z > GLFix(2 * 128));
}

TEST_CASE("a bank a block above the water can't be climbed from deep water") {
    GridWorld w;
    for (int x = -4; x <= 4; ++x) for (int z = -4; z <= 6; ++z) {
        w.solid.insert(std::make_tuple(x, 0, z));
        for (int y = 1; y <= 3; ++y) {
            if (z < 2 && y <= 2) w.water.insert(std::make_tuple(x, y, z));
            if (z >= 2) w.solid.insert(std::make_tuple(x, y, z));
        }
    }
    AABB box = playerBoxAt(64, 128, 64 + 128 - 52); PlayerMotion m;
    for (int i = 0; i < 80; ++i) playerTick(w, box, m, PlayerInput{1, 0, true}, GLFix(0), GLFix(20), false);
    CHECK(box.low_z < GLFix(2 * 128));
}



TEST_CASE("stop ends a sprint even while forward stays held, and a new double-tap restarts it") {
    DoubleTapSprint sprint;
    CHECK_FALSE(sprint.update(true));
    CHECK_FALSE(sprint.update(false));
    sprint.tick();
    CHECK(sprint.update(true));        // double-tap: sprinting
    sprint.stop();                     // ran into a wall
    CHECK_FALSE(sprint.update(true));  // still holding forward, but no longer sprinting
    CHECK_FALSE(sprint.update(false));
    sprint.tick();
    CHECK_FALSE(sprint.update(true));  // a single tap is not enough
    CHECK_FALSE(sprint.update(false));
    sprint.tick();
    CHECK(sprint.update(true));        // a fresh double-tap sprints again
}

TEST_CASE("running into a wall while sprinting ends the sprint, walking does not") {
    GridWorld w = floorWorld();
    w.solid.insert(std::make_tuple(0, 1, 1));   // a wall ahead, in the path of the player
    AABB sprinting = playerBoxAt(64, 128, 64), walking = playerBoxAt(64, 128, 64);
    PlayerMotion sprint_motion, walk_motion;
    sprint_motion.on_ground = walk_motion.on_ground = true;

    bool sprint_stopped = false, walk_stopped = false;
    for(int i = 0; i < 6; ++i) {
        playerTick(w, sprinting, sprint_motion, PlayerInput{1, 0, false, true}, GLFix(0), GLFix(10), false);
        playerTick(w, walking, walk_motion, PlayerInput{1, 0, false, false}, GLFix(0), GLFix(10), false);
        sprint_stopped = sprint_stopped || sprint_motion.sprint_stopped;
        walk_stopped = walk_stopped || walk_motion.sprint_stopped;
    }
    CHECK(sprint_stopped);
    CHECK_FALSE(walk_stopped);
}

TEST_CASE("forward and strafe together are no faster than forward alone") {
    GridWorld w = floorWorld();
    AABB straight = playerBoxAt(64, 128, 64), diagonal = playerBoxAt(64, 128, 64);
    PlayerMotion a, b;
    a.on_ground = b.on_ground = true;
    const GLFix start_z = straight.low_z, start_x = straight.low_x;

    playerTick(w, straight, a, PlayerInput{1, 0, false, false}, GLFix(0), GLFix(20), false);
    playerTick(w, diagonal, b, PlayerInput{1, 1, false, false}, GLFix(0), GLFix(20), false);

    const float straight_x = (straight.low_x - start_x).toFloat();
    const float straight_z = (straight.low_z - start_z).toFloat();
    const float diagonal_x = (diagonal.low_x - start_x).toFloat();
    const float diagonal_z = (diagonal.low_z - start_z).toFloat();
    const float straight_distance = std::sqrt(straight_x * straight_x + straight_z * straight_z);
    const float diagonal_distance = std::sqrt(diagonal_x * diagonal_x + diagonal_z * diagonal_z);
    CHECK(straight_distance > 0);
    CHECK(std::fabs(diagonal_distance / straight_distance - 1.0f) < 0.02f);
}

TEST_CASE("a sprint jump carries the player forward, and the momentum decays") {
    GridWorld w = floorWorld();
    AABB box = playerBoxAt(64, 128, 64);
    PlayerMotion m;
    m.on_ground = true;

    // The jump tick: forward held, sprint on, jump pressed.
    playerTick(w, box, m, PlayerInput{1, 0, true, true}, GLFix(0), GLFix(10), false);
    CHECK(m.boost_z > GLFix(0));
    CHECK(m.boost_x == GLFix(0));

    // Hold nothing: only the momentum moves the player, and it shrinks each tick.
    GLFix previous = box.low_z;
    GLFix first_step = GLFix(0), later_step = GLFix(0);
    for(int i = 0; i < 12; ++i) {
        playerTick(w, box, m, PlayerInput{0, 0, false, false}, GLFix(0), GLFix(10), false);
        const GLFix step = box.low_z - previous;
        previous = box.low_z;
        if(i == 0) first_step = step;
        if(i == 4) later_step = step;
    }
    CHECK(first_step > GLFix(0));
    CHECK(later_step < first_step);
}

TEST_CASE("walking jumps do not gain sprint momentum") {
    GridWorld w = floorWorld();
    AABB box = playerBoxAt(64, 128, 64);
    PlayerMotion m;
    m.on_ground = true;
    playerTick(w, box, m, PlayerInput{1, 0, true, false}, GLFix(0), GLFix(10), false);
    CHECK(m.boost_z == GLFix(0));
    CHECK(m.boost_x == GLFix(0));
}

TEST_CASE("the sprint near plane narrows to 100/115 of its base, and returns at walking speed") {
    CHECK(sprintNearPlane(GLFix(100), 0) == GLFix(100));
    CHECK(sprintNearPlane(GLFix(115), SPRINT_FOV_TICKS) == GLFix(100));
    CHECK(sprintNearPlane(GLFix(115), 99) == GLFix(100));   // ramp is clamped
    CHECK(sprintNearPlane(GLFix(115), -3) == GLFix(115));
    GLFix previous = sprintNearPlane(GLFix(115), 0);
    for(int ramp = 1; ramp <= SPRINT_FOV_TICKS; ++ramp) {
        const GLFix current = sprintNearPlane(GLFix(115), ramp);
        CHECK(current < previous);
        previous = current;
    }
}
