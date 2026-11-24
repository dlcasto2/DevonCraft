#include <sys/stat.h>

#include "worldtask.h"

#include "player_motion.h"
#include "entity.h"
#include "mobs.h"
#include "trader.h"
#include "stresstest.h"
#include "platform_time.h"
#include "lighting.h"
#include "blockdefs.h"
#include "gamestate.h"
#include "progression.h"
#include "furnace.h"
#include "furnacescreen.h"
#include "daylight.h"
#include "mining.h"
#include "saplingrenderer.h"
#include "containers.h"
#include "blockstorage.h"
#include "chestscreen.h"
#include "smithingscreen.h"
#include "itempile.h"
#include "drops.h"
#include "gui_art.h"
#include "particle.h"
#include "texturetools.h"
#include "water.h"
#include "biome.h"

#include "aabb.h"
#include "blockrenderer.h"
#include "blocklisttask.h"
#include "menutask.h"
#include "traderscreen.h"
#include "settingstask.h"
#include "fastmath.h"
#include "font.h"
#include "inventory.h"
#include "inventoryscreen.h"
#include "hotbar.h"
#include "playerinventory.h"
#include "uikit.h"

#include "textures/blockselection.h"

WorldTask world_task;
namespace {
constexpr uint32_t TRADER_FIRST_VISIT_TICKS = 12000;  // ten minutes
constexpr uint32_t TRADER_RETURN_DELAY_TICKS = 24000; // twenty minutes after a visit
constexpr uint32_t TRADER_RETRY_TICKS = 1200;         // retry in one minute if no safe spawn
constexpr uint16_t TRADER_LIFETIME_TICKS = 12000;     // a trader stays for ten minutes
}


constexpr GLFix  WorldTask::player_width,  WorldTask::player_height,  WorldTask::eye_pos;

void WorldTask::makeCurrent()
{
    Task::background_saved = false;

    Task::makeCurrent();
}

//Invert pixel at (x|y) relative to the center of the screen
void WorldTask::crosshairPixel(int x, int y)
{
    int pos = SCREEN_WIDTH/2 + x + (SCREEN_HEIGHT/2 + y)*SCREEN_WIDTH;
    screen->bitmap[pos] = ~screen->bitmap[pos];
}

void WorldTask::getForward(GLFix *x, GLFix *z)
{
    *x = GLFix(fast_sin(yr)) * speed();
    *z = GLFix(fast_cos(yr)) * speed();
}

void WorldTask::getRight(GLFix *x, GLFix *z)
{
    *x = GLFix(fast_sin((yr + 90).normaliseAngle())) * speed();
    *z = GLFix(fast_cos((yr + 90).normaliseAngle())) * speed();
}

GLFix WorldTask::speed()
{
    return 10 * settings_task.getValue(SettingsTask::SPEED) + 10;
}

void WorldTask::logic()
{
    // Record this frame's movement intent; tick() applies physics.
    input_forward = keyPressed(KEY_NSPIRE_8) ? 1 : (keyPressed(KEY_NSPIRE_2) ? -1 : 0);
    input_strafe = keyPressed(KEY_NSPIRE_6) ? 1 : (keyPressed(KEY_NSPIRE_4) ? -1 : 0);
    input_sprint = keyPressed(KEY_NSPIRE_CTRL) && input_forward > 0;

    const bool jump_down = keyPressed(KEY_NSPIRE_5);
    input_jump = jump_down;

    // Creative flight uses the familiar double-tap-jump gesture. The timer is
    // measured in simulation ticks so it remains consistent across frame rates.
    if(player_state.mode == GameMode::CREATIVE)
    {
        if(jump_down && !jump_key_was_down)
        {
            if(jump_tap_ticks > 0)
            {
                creative_flight = !creative_flight;
                jump_tap_ticks = 0;
                vy = 0;
                can_jump = false;
                setMessage(creative_flight ? "Creative flight ON." : "Creative flight OFF.");
            }
            else
                jump_tap_ticks = 12; // 0.6 seconds at 20 ticks per second
        }
    }
    else
    {
        jump_tap_ticks = 0;
        if(creative_flight)
        {
            creative_flight = false;
            vy = 0;
            can_jump = false;
            setMessage("Creative flight disabled.");
        }
    }
    jump_key_was_down = jump_down;
    input_descend = creative_flight && player_state.mode == GameMode::CREATIVE
                 && keyPressed(KEY_NSPIRE_0);

    // Survival mines while 9 is held; tick() does the work.
    input_mine = player_state.mode == GameMode::SURVIVAL && keyPressed(KEY_NSPIRE_9);

    if(has_touchpad)
    {
        touchpad_report_t touchpad;
        touchpad_scan(&touchpad);

        if(touchpad.pressed)
        {
            switch(touchpad.arrow)
            {
            case TPAD_ARROW_DOWN:
                xr += speed()/3;
                break;
            case TPAD_ARROW_UP:
                xr -= speed()/3;
                break;
            case TPAD_ARROW_LEFT:
                yr -= speed()/3;
                break;
            case TPAD_ARROW_RIGHT:
                yr += speed()/3;
                break;
            case TPAD_ARROW_RIGHTDOWN:
                xr += speed()/3;
                yr += speed()/3;
                break;
            case TPAD_ARROW_UPRIGHT:
                xr -= speed()/3;
                yr += speed()/3;
                break;
            case TPAD_ARROW_DOWNLEFT:
                xr += speed()/3;
                yr -= speed()/3;
                break;
            case TPAD_ARROW_LEFTUP:
                xr -= speed()/3;
                yr -= speed()/3;
                break;
            }
        }
        else if(tp_had_contact && touchpad.contact)
        {
            yr += (touchpad.x - tp_last_x) / 50;
            xr -= (touchpad.y - tp_last_y) / 50;
        }

        tp_had_contact = touchpad.contact;
        tp_last_x = touchpad.x;
        tp_last_y = touchpad.y;
    }
    else
    {
        if(keyPressed(KEY_NSPIRE_UP))
            xr -= speed()/3;
        else if(keyPressed(KEY_NSPIRE_DOWN))
            xr += speed()/3;

        if(keyPressed(KEY_NSPIRE_LEFT))
            yr -= speed()/3;
        else if(keyPressed(KEY_NSPIRE_RIGHT))
            yr += speed()/3;
    }

    //Normalisation required for rotation with nglRotate
    yr.normaliseAngle();
    xr.normaliseAngle();

    //xr and yr are normalised, so we can't test for negative values
    if(xr > GLFix(180))
        if(xr <= GLFix(270))
            xr = 269;

    if(xr < GLFix(180))
        if(xr >= GLFix(90))
            xr = 89;

    //Do test only on every second frame, it's expensive
    if(do_test)
    {
        GLFix dx = fast_sin(yr)*fast_cos(xr), dy = -fast_sin(xr), dz = fast_cos(yr)*fast_cos(xr);
        GLFix dist = GLFix::maxValue();
        const ItemStack &held = player_inventory.selectedStack();
        if(!world.intersectsRay(x, y + eye_pos, z, dx, dy, dz, selection_pos, selection_side, dist, !held.empty() && aimsAtWater(held.id)))
            selection_side = AABB::NONE;
        else
            selection_pos_abs = {x + dx * dist, y + eye_pos + dy * dist, z + dz * dist};

        // A mob is targetable only when it is nearer than the block in front of it.
        aim_mob = aimedMob(x, y + eye_pos, z, dx, dy, dz,
                           selection_side == AABB::NONE ? GLFix::maxValue() : dist);
    }

    world.setPosition(x, y, z);

    do_test = !do_test;

    if(key_held_down)
        key_held_down = keyPressed(KEY_NSPIRE_ESC) || keyPressed(KEY_NSPIRE_7) || keyPressed(KEY_NSPIRE_9) || keyPressed(KEY_NSPIRE_1) || keyPressed(KEY_NSPIRE_3) || keyPressed(KEY_NSPIRE_PERIOD) || keyPressed(KEY_NSPIRE_MINUS) || keyPressed(KEY_NSPIRE_PLUS) || keyPressed(KEY_NSPIRE_MENU);

    else if(keyPressed(KEY_NSPIRE_ESC)) //Save & Exit
    {
        save();
        Task::running = false;
        return;
    }
    else if(keyPressed(KEY_NSPIRE_7)) //Put block down
    {
        key_held_down = true;

        if(aim_mob && aim_mob->type == EntityType::MOB && aim_mob->mob_kind == MOB_TRADER)
        {
            trader_screen.open(*aim_mob);
            return;
        }

        if(selection_side == AABB::NONE)
            return;

        if(useBlock(static_cast<int>(selection_pos.x), static_cast<int>(selection_pos.y), static_cast<int>(selection_pos.z)))
            return;

        if(useBucket())
            return;

        if(world.intersect(aabb))
            return;

        if(world.blockAction(selection_pos.x, selection_pos.y, selection_pos.z))
            return;

        BLOCK_WDATA current_block,
                    block_to_place = current_inventory.currentBlock();

        if(block_to_place == BLOCK_AIR)
            return;

        // Survival places from the stack; creative never runs out.
        const bool survival = player_state.mode == GameMode::SURVIVAL;
        auto useOne = [survival]() {
            if(!survival)
                return;
            ItemStack &held = player_inventory.selectedStack();
            if(--held.count == 0)
                held = ItemStack();
        };

        const VECTOR3 pos = besideSelection();
        current_block = world.getBlock(pos.x, pos.y, pos.z);

        if(getBLOCK(block_to_place) == BLOCK_BED)
        {
            if(placeBed(static_cast<int>(pos.x), static_cast<int>(pos.y), static_cast<int>(pos.z)))
            {
                useOne();
                progressionBlockPlaced(BLOCK_BED);
                if(const char *notice = takeProgressNotice())
                    setMessage(notice);
            }
            return;
        }

        // Saplings only take root on dirt or grass
        if(getBLOCK(block_to_place) == BLOCK_SAPLING && !SaplingRenderer::canStayOn(getBLOCK(world.getBlock(pos.x, pos.y - 1, pos.z))))
            return;

        // Cacti (1.8.8) stand on sand or another cactus, with nothing solid beside them
        if(getBLOCK(block_to_place) == BLOCK_CACTUS)
        {
            const BLOCK below = getBLOCK(world.getBlock(pos.x, pos.y - 1, pos.z));
            if(below != BLOCK_SAND && below != BLOCK_CACTUS)
                return;
            if(global_block_renderer.isObstacle(world.getBlock(pos.x - 1, pos.y, pos.z)) || global_block_renderer.isObstacle(world.getBlock(pos.x + 1, pos.y, pos.z))
               || global_block_renderer.isObstacle(world.getBlock(pos.x, pos.y, pos.z - 1)) || global_block_renderer.isObstacle(world.getBlock(pos.x, pos.y, pos.z + 1)))
                return;
        }

        //Only set the block if there's air or a fluid, which it pushes out
        if(current_block == BLOCK_AIR || isFluid(current_block))
        {
            if(!global_block_renderer.isOriented(block_to_place))
                world.changeBlock(pos.x, pos.y, pos.z, block_to_place);
            else
            {
                AABB::SIDE side = selection_side;
                //If the block is not fully oriented and has been placed on top or bottom of another block, determine the orientation by yr
                if(!global_block_renderer.isFullyOriented(block_to_place) && (side == AABB::TOP || side == AABB::BOTTOM))
                    side = yr < GLFix(45) ? AABB::FRONT : yr < GLFix(135) ? AABB::LEFT : yr < GLFix(225) ? AABB::BACK : yr < GLFix(315) ? AABB::RIGHT : AABB::FRONT;

                world.changeBlock(pos.x, pos.y, pos.z, getBLOCKWDATA(block_to_place, side)); //AABB::SIDE is compatible to BLOCK_SIDE
            }

            //If the player is stuck now, it's because of the block change, so remove it again
            if(world.intersect(aabb))
                world.changeBlock(pos.x, pos.y, pos.z, current_block);
            else
            {
                if(getBLOCK(block_to_place) == BLOCK_CHEST)
                    chestPlaced(static_cast<int>(pos.x), static_cast<int>(pos.y), static_cast<int>(pos.z));
                else if(getBLOCK(block_to_place) == BLOCK_FURNACE)
                    furnacePlaced(static_cast<int>(pos.x), static_cast<int>(pos.y), static_cast<int>(pos.z));
                useOne();
                progressionBlockPlaced(getBLOCK(block_to_place));
                if(const char *notice = takeProgressNotice())
                    setMessage(notice);
            }
        }
    }
    else if(keyPressed(KEY_NSPIRE_9) && player_state.mode != GameMode::SURVIVAL) //Remove block (creative: instantly)
    {
        const BLOCK_WDATA target = selection_side == AABB::NONE ? BLOCK_AIR : world.getBlock(selection_pos.x, selection_pos.y, selection_pos.z);
        if(selection_side != AABB::NONE && target != BLOCK_BEDROCK)
            removeBlock(static_cast<int>(selection_pos.x), static_cast<int>(selection_pos.y), static_cast<int>(selection_pos.z), target);

        key_held_down = true;
    }
    else if(keyPressed(KEY_NSPIRE_1)) //Switch inventory slot
    {
        current_inventory.previousSlot();

        key_held_down = true;
    }
    else if(keyPressed(KEY_NSPIRE_3))
    {
        current_inventory.nextSlot();

        key_held_down = true;
    }
    else if(keyPressed(KEY_NSPIRE_PERIOD)) //Open list of blocks (or take screenshot with Ctrl + .)
    {
        if(keyPressed(KEY_NSPIRE_CTRL))
        {
            //Find a filename that doesn't exist
            char buf[45];

            unsigned int i;
            for(i = 0; i <= 999; ++i)
            {
                snprintf(buf, sizeof(buf), "/documents/ndless/screenshot_%d.ppm.tns", i);

                struct stat stat_buf;
                if(stat(buf, &stat_buf) != 0)
                    break;
            }

            if(i > 999 || !saveTextureToFile(*screen, buf))
                setMessage("Screenshot failed!");
            else
            {
                snprintf(message, sizeof(message), "Screenshot taken (%d)!", i);
                message_timeout = 20;
            }
        }
        else
        {
            progressionInventoryOpened();
            if(const char *notice = takeProgressNotice())
                setMessage(notice);
            draw_inventory = false;
            render();
            draw_inventory = true;
            if(player_state.mode == GameMode::SURVIVAL)
                inventory_screen.makeCurrent();
            else
                block_list_task.makeCurrent();
        }

        key_held_down = true;
    }
    else if(keyPressed(KEY_NSPIRE_MINUS)) //Decrease max view distance
    {
        int fov = world.fieldOfView() - 1;
        world.setFieldOfView(fov < 1 ? 1 : fov);

        key_held_down = true;
    }
    else if(keyPressed(KEY_NSPIRE_PLUS)) //Increase max view distance
    {
        world.setFieldOfView(world.fieldOfView() + 1);

        key_held_down = true;
    }
    else if(keyPressed(KEY_NSPIRE_MENU))
    {
        menu_task.makeCurrent();

        key_held_down = true;
    }
}

void WorldTask::tick()
{
    if(jump_tap_ticks > 0)
        --jump_tap_ticks;

    // Build the physics box from the current position, run one tick, read it back.
    AABB box{x - player_width/2, y, z - player_width/2, x + player_width/2, y + player_height, z + player_width/2};

    PlayerMotion motion;
    motion.vy = vy;
    motion.on_ground = can_jump;

    // Eyes underwater: air runs down, mining is slower and the view turns blue.
    const bool head_in_water = getBLOCK(world.getBlock((x / BLOCK_SIZE).floor(), ((y + eye_pos) / BLOCK_SIZE).floor(), (z / BLOCK_SIZE).floor())) == BLOCK_WATER;
    in_water = head_in_water;
    player_state.air_ticks = nextAirTicks(player_state.air_ticks, head_in_water, player_state.mode == GameMode::SURVIVAL);

    const GLFix walk = velocityPerTick(speed(), V13_FPS);
    const bool auto_jump = settings_task.getValue(SettingsTask::AUTO_JUMP) != 0;

    if(creative_flight && player_state.mode == GameMode::CREATIVE)
    {
        playerFlyTick(world_collision, box, input_forward, input_strafe,
                      input_jump, input_descend, yr, walk);
        motion.vy = 0;
        motion.on_ground = false;
        motion.swimming = false;
    }
    else
        playerTick(world_collision, box, motion, PlayerInput{input_forward, input_strafe, input_jump, input_sprint}, yr, walk, auto_jump);

    x = (box.low_x + box.high_x) / 2;
    y = box.low_y;
    z = (box.low_z + box.high_z) / 2;
    vy = motion.vy;
    can_jump = motion.on_ground;

    world.setPosition(x, y, z);

    tickMobs();
    entity_pool.tick(world_collision);

    if(player_state.mode == GameMode::SURVIVAL)
        tickMining(motion.on_ground);
    else
        mining = MiningState();
    collectDrops(entity_pool, box, player_inventory);
    collectPiles(entity_pool, item_piles, box, player_inventory);
    if(const char *notice = takeProgressNotice())
        setMessage(notice);
    removeExpiredPiles(entity_pool, item_piles);
    tickFurnaces();

    // Sleeping: the screen fades, then it's morning.
    if(sleep_ticks > 0 && --sleep_ticks == 0)
    {
        sleepInBed(sleep_x, sleep_y, sleep_z);
        setMessage("Good morning. Spawn point set.");
    }

    world_state.time_of_day = (world_state.time_of_day + 1) % DAY_TICKS;
    const int ambient_light = skyBrightnessLevel(world_state.time_of_day);
    world.setBrightnessAll(ambient_light);

    #ifndef STRESS_TEST
        const ItemStack &held_light = player_inventory.selectedStack();
        int hand_light_level = 0;
        if(!held_light.empty() && isBlockItem(held_light.id) && held_light.id <= BLOCK_SPECIAL_LAST)
        {
            const BLOCK held_block = static_cast<BLOCK>(held_light.id);
            hand_light_level = blockDef(held_block).light;
            if(held_block == BLOCK_REDSTONE_LAMP && (held_light.meta & 1u) == 0)
                hand_light_level = 0;
        }
        world.updateDynamicLighting(ambient_light,
                                    (x / BLOCK_SIZE).floor(),
                                    (y / BLOCK_SIZE).floor(),
                                    (z / BLOCK_SIZE).floor(),
                                    hand_light_level);
    #endif

    #ifdef STRESS_TEST
        if(!stress_spawned)
        {
            spawnStressDummies(entity_pool, x, y, z, 12345);
            stress_spawned = true;
        }
        ++stress_tick_count;
        entity_pool.forEach([this](Entity &e){ stressSteer(e, stress_tick_count, 999); });
        world.setBrightnessAll(stressBrightnessForTick(stress_tick_count));

        // TPS over the last full RTC second.
        const uint32_t now = platformRtcSeconds();
        if(now != stress_rtc_second)
        {
            stress_tps = stress_ticks_this_second;
            stress_ticks_this_second = 0;
            stress_rtc_second = now;
        }
        ++stress_ticks_this_second;
    #endif
}

// Sun and moon on opposite sides of a circle around the camera, drawn before
// the world with only the camera's rotation applied, so they stay at the
// horizon whatever the player does. The depth buffer is cleared afterwards,
// so any terrain in front covers them.
void WorldTask::renderSky()
{
    constexpr int SKY_DISTANCE = 1500, SKY_SIZE = 260;
    const FFix angle = sunAngleDegrees(world_state.time_of_day);
    const GLFix c = fast_cos(angle), s = fast_sin(angle);
    const TextureAtlasEntry whole = textureArea(0, 0, 32, 32);

    // A little below the horizon still shows, so it can set behind the hills.
    const GLFix below_horizon = -0.15f;
    if(s > below_horizon)
    {
        glBindTexture(&gui_sun);
        Particle::render(VECTOR3{c * SKY_DISTANCE, s * SKY_DISTANCE, 0}, SKY_SIZE, whole);
    }
    if(-s > below_horizon)
    {
        glBindTexture(&gui_moon);
        Particle::render(VECTOR3{-c * SKY_DISTANCE, -s * SKY_DISTANCE, 0}, SKY_SIZE * 3 / 4, whole);
    }

    glClear(GL_DEPTH_BUFFER_BIT);
    glBindTexture(terrain_current);
}

VECTOR3 WorldTask::besideSelection() const
{
    VECTOR3 pos = selection_pos;
    switch(selection_side)
    {
    case AABB::BACK:
        ++pos.z;
        break;
    case AABB::FRONT:
        --pos.z;
        break;
    case AABB::LEFT:
        --pos.x;
        break;
    case AABB::RIGHT:
        ++pos.x;
        break;
    case AABB::BOTTOM:
        --pos.y;
        break;
    case AABB::TOP:
        ++pos.y;
        break;
    default:
        puts("This can't normally happen #1");
        break;
    }
    return pos;
}

bool WorldTask::useBucket()
{
    ItemStack &held = player_inventory.selectedStack();
    if(held.empty() || (held.id != ITEM_BUCKET && held.id != ITEM_WATER_BUCKET))
        return false;

    // Creative keeps its bucket, full or empty, as in 1.8.8.
    const bool survival = player_state.mode == GameMode::SURVIVAL;

    if(held.id == ITEM_BUCKET)
    {
        // The ray stops at water sources while an empty bucket is held.
        const int bx = static_cast<int>(selection_pos.x), by = static_cast<int>(selection_pos.y), bz = static_cast<int>(selection_pos.z);
        if(!canScoop(world.getBlock(bx, by, bz)))
            return true;

        world.changeBlock(bx, by, bz, BLOCK_AIR);
        if(!survival)
            return true;

        if(held.count == 1)
            held = ItemStack(ITEM_WATER_BUCKET, 1);
        else
        {
            --held.count;
            // The full bucket goes into the inventory, or at your feet if it's full.
            const ItemStack full(ITEM_WATER_BUCKET, 1);
            if(player_inventory.add(full) > 0)
                spawnDrop(entity_pool, x, y + BLOCK_SIZE / 2, z, full, player_inventory);
        }
        return true;
    }

    // A water bucket pours a source against the face you aim at.
    const VECTOR3 pos = besideSelection();
    if(!canPourInto(world.getBlock(pos.x, pos.y, pos.z)))
        return true;

    world.changeBlock(pos.x, pos.y, pos.z, WATER_SOURCE);
    if(survival)
        held = ItemStack(ITEM_BUCKET, 1);
    return true;
}

bool WorldTask::useBlock(int bx, int by, int bz)
{
    const BLOCK_WDATA target = world.getBlock(bx, by, bz);
    const BLOCK b = getBLOCK(target);
    if(b != BLOCK_CHEST && b != BLOCK_SMITHING_TABLE && b != BLOCK_BED && b != BLOCK_CRAFTING_TABLE && b != BLOCK_FURNACE)
        return false;
    // Creative has the block list instead, so a table there is just a block.
    if(b == BLOCK_CRAFTING_TABLE && player_state.mode != GameMode::SURVIVAL)
        return false;

    if(b == BLOCK_BED)
    {
        switch(canSleep(world_state.time_of_day, world_state.difficulty))
        {
        case SleepCheck::OK:
            sleep_ticks = SLEEP_FADE_TICKS;
            sleep_x = bx; sleep_y = by; sleep_z = bz;
            break;
        case SleepCheck::NOT_NIGHT:
            setMessage("You can only sleep at night.");
            break;
        case SleepCheck::TORMENT:
            setMessage("The bed is cold. Something is awake.");
            break;
        }
        return true;
    }

    // Open the screen over a clean frame of the world.
    draw_inventory = false;
    render();
    draw_inventory = true;
    if(b == BLOCK_CHEST)
        chest_screen.open(bx, by, bz);
    else if(b == BLOCK_CRAFTING_TABLE)
        inventory_screen.openCrafting();
    else if(b == BLOCK_FURNACE)
        furnace_screen.open(bx, by, bz);
    else
        smithing_screen.makeCurrent();
    return true;
}

bool WorldTask::placeBed(int fx, int fy, int fz)
{
    // The foot goes where you aimed, the head one block further the way you face.
    const BLOCK_SIDE facing = bedFacingFromYaw(static_cast<int>(yr));
    int dx, dz;
    sideOffset(facing, dx, dz);
    const int hx = fx + dx, hz = fz + dz;
    if(world.getBlock(fx, fy, fz) != BLOCK_AIR || world.getBlock(hx, fy, hz) != BLOCK_AIR)
        return false;
    if(!global_block_renderer.isObstacle(world.getBlock(fx, fy - 1, fz)) || !global_block_renderer.isObstacle(world.getBlock(hx, fy - 1, hz)))
        return false;

    world.changeBlock(fx, fy, fz, getBLOCKWDATA(BLOCK_BED, bedData(facing, false)));
    world.changeBlock(hx, fy, hz, getBLOCKWDATA(BLOCK_BED, bedData(facing, true)));
    if(world.intersect(aabb))
    {
        world.changeBlock(fx, fy, fz, BLOCK_AIR);
        world.changeBlock(hx, fy, hz, BLOCK_AIR);
        return false;
    }
    return true;
}

void WorldTask::removeBlock(int bx, int by, int bz, BLOCK_WDATA target)
{
    world.spawnDestructionParticles(bx, by, bz);
    world.changeBlock(bx, by, bz, BLOCK_AIR);

    const GLFix cx = bx * BLOCK_SIZE + BLOCK_SIZE / 2, cy = by * BLOCK_SIZE + BLOCK_SIZE / 2, cz = bz * BLOCK_SIZE + BLOCK_SIZE / 2;
    if(getBLOCK(target) == BLOCK_CHEST || getBLOCK(target) == BLOCK_FURNACE)
    {
        // Everything inside comes out as one pile.
        const std::vector<ItemStack> contents = chestBroken(bx, by, bz);
        if(!contents.empty())
            dropPile(entity_pool, item_piles, cx, cy, cz, contents.data(), static_cast<int>(contents.size()), player_inventory);
    }
    else if(getBLOCK(target) == BLOCK_BED)
    {
        // Either half takes the whole bed with it (one bed drops).
        int ox, oz;
        bedOtherHalf(bx, bz, getBLOCKDATA(target), ox, oz);
        if(getBLOCK(world.getBlock(ox, by, oz)) == BLOCK_BED)
            world.changeBlock(ox, by, oz, BLOCK_AIR);
    }
}

void WorldTask::tickMobs()
{
    // Mobs wander; any that have strayed far from the player go away.
    const int px = (x / BLOCK_SIZE).floor(), pz = (z / BLOCK_SIZE).floor();
    entity_pool.forEach([&](Entity &e) {
        if(e.type != EntityType::MOB)
            return;
        const int dx = (e.box.low_x / BLOCK_SIZE).floor() - px, dz = (e.box.low_z / BLOCK_SIZE).floor() - pz;
        const bool too_far = dx * dx + dz * dz > MOB_DESPAWN_BLOCKS * MOB_DESPAWN_BLOCKS;
        const bool visit_over = e.mob_kind == MOB_TRADER && e.age_ticks >= TRADER_LIFETIME_TICKS;
        if(too_far || visit_over)
        {
            if(e.mob_kind == MOB_TRADER)
                world_state.trader_next_visit = TRADER_RETURN_DELAY_TICKS;
            if(aim_mob == &e)
                aim_mob = nullptr;
            entity_pool.remove(&e);
            return;
        }
        steerMob(e, mob_rng);
    });

    if(++mob_spawn_timer >= MOB_SPAWN_INTERVAL_TICKS)
    {
        mob_spawn_timer = 0;
        if(countPassive(entity_pool) < MOB_MAX_PASSIVE)
            trySpawnPassive();
    }

    // A single wandering trader visits occasionally. Traders themselves are
    // not saved between sessions, like the passive animals.
    if(countTraders(entity_pool) > 0)
        world_state.trader_next_visit = 0;
    else if(world_state.trader_next_visit == 0)
        world_state.trader_next_visit = TRADER_FIRST_VISIT_TICKS;
    else if(--world_state.trader_next_visit == 0 && !trySpawnTrader())
        world_state.trader_next_visit = TRADER_RETRY_TICKS;

    if(mob_hit_cooldown > 0)
        --mob_hit_cooldown;

    // Survival: animals can be hit, but the wandering trader stays peaceful.
    if(input_mine && aim_mob && aim_mob->type == EntityType::MOB)
    {
        if(aim_mob->mob_kind == MOB_TRADER)
            input_mine = false;
        else if(mob_hit_cooldown == 0)
        {
            hitMob(*aim_mob);
            input_mine = false;
        }
    }
}

void WorldTask::trySpawnPassive()
{
    const uint32_t r = mobRandom(mob_rng);
    const int px = (x / BLOCK_SIZE).floor(), py = (y / BLOCK_SIZE).floor(), pz = (z / BLOCK_SIZE).floor();

    // Eight to fifteen blocks away, in one of four directions.
    const int dist = 8 + static_cast<int>(r % 8);
    const int dir = static_cast<int>((r >> 8) % 4);
    const int bx = px + MOB_DIR_DX[dir] * dist, bz = pz + MOB_DIR_DZ[dir] * dist;

    // Look down from a little above the player for the top block of that column.
    for(int by = py + 3; by > py - 8; --by)
    {
        if(!world.isLoaded(bx, by, bz))
            return;
        const BLOCK top = getBLOCK(world.getBlock(bx, by, bz));
        if(top == BLOCK_AIR)
            continue;

        const bool on_grass = top == BLOCK_GRASS && getBLOCK(world.getBlock(bx, by + 1, bz)) == BLOCK_AIR;
        if(!canSpawnPassive(on_grass, daylightFactor(world_state.time_of_day)))
            return;

        // A mob is three quarters of a block wide and tall, standing on the grass.
        const GLFix half = BLOCK_SIZE * 3 / 8;
        const GLFix cx = GLFix(bx * BLOCK_SIZE + BLOCK_SIZE / 2), cz = GLFix(bz * BLOCK_SIZE + BLOCK_SIZE / 2);
        const GLFix low_y = GLFix((by + 1) * BLOCK_SIZE), high_y = low_y + BLOCK_SIZE * 3 / 4;
        const AABB box{cx - half, low_y, cz - half, cx + half, high_y, cz + half};
        spawnPassive(entity_pool, (r >> 12) % 2 == 0 ? MOB_PIG : MOB_COW, box);
        return;
    }
}

bool WorldTask::trySpawnTrader()
{
    if(countTraders(entity_pool) != 0)
        return false;

    const uint32_t r = mobRandom(mob_rng);
    const int px = (x / BLOCK_SIZE).floor(), py = (y / BLOCK_SIZE).floor(), pz = (z / BLOCK_SIZE).floor();
    const int dist = 8 + static_cast<int>(r % 8);
    const int dir = static_cast<int>((r >> 8) % 4);
    const int bx = px + MOB_DIR_DX[dir] * dist, bz = pz + MOB_DIR_DZ[dir] * dist;

    // A trader needs a solid, non-fluid floor and two clear blocks above it.
    for(int by = py + 3; by > py - 8; --by)
    {
        if(!world.isLoaded(bx, by, bz))
            return false;
        const BLOCK_WDATA floor = world.getBlock(bx, by, bz);
        const BLOCK top = getBLOCK(floor);
        if(top == BLOCK_AIR)
            continue;
        if(isFluid(floor) || !global_block_renderer.isObstacle(floor))
            return false;
        if(!world.isLoaded(bx, by + 1, bz) || !world.isLoaded(bx, by + 2, bz))
            return false;
        if(getBLOCK(world.getBlock(bx, by + 1, bz)) != BLOCK_AIR
           || getBLOCK(world.getBlock(bx, by + 2, bz)) != BLOCK_AIR)
            return false;

        const GLFix half = BLOCK_SIZE * 3 / 10;
        const GLFix cx = GLFix(bx * BLOCK_SIZE + BLOCK_SIZE / 2);
        const GLFix cz = GLFix(bz * BLOCK_SIZE + BLOCK_SIZE / 2);
        const GLFix low_y = GLFix((by + 1) * BLOCK_SIZE);
        const AABB box{cx - half, low_y, cz - half,
                       cx + half, low_y + BLOCK_SIZE * 18 / 10, cz + half};
        if(world_collision.intersects(box))
            return false;
        if(!spawnTrader(entity_pool, box))
            return false;
        setMessage("A wandering trader is nearby.");
        return true;
    }
    return false;
}

void WorldTask::hitMob(Entity &e)
{
    if(e.mob_kind == MOB_TRADER)
        return;
    mob_hit_cooldown = MOB_HIT_COOLDOWN_TICKS;
    if(!hurtMob(e, MOB_HIT_DAMAGE))
        return;

    const GLFix cx = (e.box.low_x + e.box.high_x) / 2, cy = (e.box.low_y + e.box.high_y) / 2, cz = (e.box.low_z + e.box.high_z) / 2;
    const ItemStack meat = mobDrop(e.mob_kind);
    entity_pool.remove(&e);
    aim_mob = nullptr;
    spawnDrop(entity_pool, cx, cy, cz, meat, player_inventory);
}

Entity *WorldTask::aimedMob(GLFix ox, GLFix oy, GLFix oz, GLFix dx, GLFix dy, GLFix dz, GLFix block_dist)
{
    // 1.8.8 reach: three blocks.
    const GLFix reach = GLFix(BLOCK_SIZE) * 3;
    Entity *best = nullptr;
    GLFix best_dist = block_dist < reach ? block_dist : reach;

    entity_pool.forEach([&](Entity &e) {
        if(e.type != EntityType::MOB)
            return;
        GLFix d;
        if(e.box.intersectsRay(ox, oy, oz, dx, dy, dz, d) != AABB::NONE && d < best_dist)
        {
            best = &e;
            best_dist = d;
        }
    });
    return best;
}

void WorldTask::tickFurnaces()
{
    static std::vector<BlockPos> gone;
    gone.clear();
    block_storage.forEach([this](const BlockPos &p, StorageRecord &r) {
        if(r.kind != StorageKind::FURNACE)
            return;
        // Idle furnaces cost nothing; the rest only run while their chunk is loaded.
        if(r.burn_ticks_left == 0 && r.cook_ticks == 0 && (r.slots[FURNACE_INPUT].empty() || r.slots[FURNACE_FUEL].empty()))
            return;
        if(!world.isLoaded(p.x, p.y, p.z))
            return;
        if(getBLOCK(world.getBlock(p.x, p.y, p.z)) != BLOCK_FURNACE)
        {
            gone.push_back(p);   // blown up or replaced
            return;
        }
        furnaceTick(r);
    });
    for(const BlockPos &p : gone)
        block_storage.erase(p);
}

void WorldTask::tickMining(bool on_ground)
{
    const bool has_target = selection_side != AABB::NONE;
    const int bx = static_cast<int>(selection_pos.x), by = static_cast<int>(selection_pos.y), bz = static_cast<int>(selection_pos.z);
    const BLOCK_WDATA target = has_target ? world.getBlock(bx, by, bz) : BLOCK_AIR;
    ItemStack &held = player_inventory.selectedStack();
    const ItemId tool = held.empty() ? 0 : held.id;

    if(!miningTick(mining, input_mine, has_target && target != BLOCK_AIR, bx, by, bz, target, tool, in_water, on_ground))
        return;

    // 1.8.8 order: work out the drop, clear the block, drop it, then wear the tool,
    // so a tool that breaks on its last block still gives that block.
    const ItemStack drop = blockDropFor(target, tool, drop_rng);
    progressionBlockMined(getBLOCK(target));
    removeBlock(bx, by, bz, target);

    // Ice (1.8.8) melts into water, unless there's nothing under it to hold it.
    if(getBLOCK(target) == BLOCK_ICE)
    {
        const BLOCK_WDATA below = world.getBlock(bx, by - 1, bz);
        if(global_block_renderer.isObstacle(below) || isFluid(below))
            world.changeBlock(bx, by, bz, WATER_SOURCE);
    }

    spawnDrop(entity_pool, GLFix(bx * BLOCK_SIZE + BLOCK_SIZE / 2), GLFix(by * BLOCK_SIZE + BLOCK_SIZE / 2),
              GLFix(bz * BLOCK_SIZE + BLOCK_SIZE / 2), drop, player_inventory);
    wearTool(held, toolWearFor(tool, target));
    if(const char *notice = takeProgressNotice())
        setMessage(notice);
}

void WorldTask::render()
{
    aabb = {x - player_width/2, y, z - player_width/2, x + player_width/2, y + player_height, z + player_width/2};
    //printf("X: %f Y: %f Z: %f XR: %d YR: %d\n", x.toFloat(), y.toFloat(), z.toFloat(), xr.toInt(), yr.toInt());

    // Sky colour follows the time of day
    unsigned sky_r, sky_g, sky_b;
    skyColor(world_state.time_of_day, sky_r, sky_g, sky_b);
    nglSetColor(static_cast<COLOR>(((sky_r >> 3) << 11) | ((sky_g >> 2) << 5) | (sky_b >> 3)));
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glPushMatrix();

    //Inverted rotation of the world
    nglRotateX((GLFix(359) - xr).normaliseAngle());
    nglRotateY((GLFix(359) - yr).normaliseAngle());

    renderSky();
    //Inverted translation of the world
    glTranslatef(-x, -y - eye_pos, -z);

    glBindTexture(terrain_current);

    world.render();

    renderEntities(entity_pool);

    //Draw indication
    glBindTexture(&blockselection);

    //Do a quick animation
    const unsigned int blockselection_frame_width = blockselection.width / blockselection_frames;
    TextureAtlasEntry tex = textureArea(0, 0, blockselection_frame_width, blockselection.height);
    tex.left += blockselection_frame_width * blockselection_frame;
    tex.right += blockselection_frame_width * blockselection_frame;

    //Only increment the frame nr each 5 frames
    if(++blockselection_frame_fraction == 5)
    {
        blockselection_frame_fraction = 0;

        if(++blockselection_frame == blockselection_frames)
            blockselection_frame = 0;
    }

    const GLFix indicator_x = selection_pos.x * BLOCK_SIZE, indicator_y = selection_pos.y * BLOCK_SIZE, indicator_z = selection_pos.z * BLOCK_SIZE;
    // Pass 0 draws the selection outline; pass 1, while mining this block,
    // the crack stage just in front of it.
    const int crack = crackStage(mining);
    const bool cracking = crack >= 0 && mining.x == static_cast<int>(selection_pos.x)
                          && mining.y == static_cast<int>(selection_pos.y) && mining.z == static_cast<int>(selection_pos.z);
    for(int pass = 0; pass < (cracking ? 2 : 1); ++pass)
    {
        if(pass == 1)
        {
            glBindTexture(&gui_crack);
            tex = textureArea(crack * 16, 0, 16, 16);
        }
        const GLFix selection_offset = pass == 0 ? 3 : 5; //Needed to prevent Z-fighting

        glPushMatrix();
        glTranslatef(indicator_x, indicator_y, indicator_z);

        glBegin(GL_QUADS);
        switch(selection_side)
        {
        case AABB::FRONT:
            nglAddVertex({0, 0, selection_pos_abs.z - indicator_z - selection_offset, tex.left, tex.bottom, TEXTURE_TRANSPARENT});
            nglAddVertex({0, BLOCK_SIZE, selection_pos_abs.z - indicator_z - selection_offset, tex.left, tex.top, TEXTURE_TRANSPARENT});
            nglAddVertex({BLOCK_SIZE, BLOCK_SIZE, selection_pos_abs.z - indicator_z - selection_offset, tex.right, tex.top, TEXTURE_TRANSPARENT});
            nglAddVertex({BLOCK_SIZE, 0, selection_pos_abs.z - indicator_z - selection_offset, tex.right, tex.bottom, TEXTURE_TRANSPARENT});
            break;
        case AABB::BACK:
            nglAddVertex({BLOCK_SIZE, 0, selection_pos_abs.z - indicator_z + selection_offset, tex.left, tex.bottom, TEXTURE_TRANSPARENT});
            nglAddVertex({BLOCK_SIZE, BLOCK_SIZE, selection_pos_abs.z - indicator_z + selection_offset, tex.left, tex.top, TEXTURE_TRANSPARENT});
            nglAddVertex({0, BLOCK_SIZE, selection_pos_abs.z - indicator_z + selection_offset, tex.right, tex.top, TEXTURE_TRANSPARENT});
            nglAddVertex({0, 0, selection_pos_abs.z - indicator_z + selection_offset, tex.right, tex.bottom, TEXTURE_TRANSPARENT});
            break;
        case AABB::RIGHT:
            nglAddVertex({selection_pos_abs.x - indicator_x + selection_offset, 0, 0, tex.right, tex.bottom, TEXTURE_TRANSPARENT});
            nglAddVertex({selection_pos_abs.x - indicator_x + selection_offset, BLOCK_SIZE, 0, tex.right, tex.top, TEXTURE_TRANSPARENT});
            nglAddVertex({selection_pos_abs.x - indicator_x + selection_offset, BLOCK_SIZE, BLOCK_SIZE, tex.left, tex.top, TEXTURE_TRANSPARENT});
            nglAddVertex({selection_pos_abs.x - indicator_x + selection_offset, 0, BLOCK_SIZE, tex.left, tex.bottom, TEXTURE_TRANSPARENT});
            break;
        case AABB::LEFT:
            nglAddVertex({selection_pos_abs.x - indicator_x - selection_offset, 0, BLOCK_SIZE, tex.left, tex.bottom, TEXTURE_TRANSPARENT});
            nglAddVertex({selection_pos_abs.x - indicator_x - selection_offset, BLOCK_SIZE, BLOCK_SIZE, tex.left, tex.top, TEXTURE_TRANSPARENT});
            nglAddVertex({selection_pos_abs.x - indicator_x - selection_offset, BLOCK_SIZE, 0, tex.right, tex.top, TEXTURE_TRANSPARENT});
            nglAddVertex({selection_pos_abs.x - indicator_x - selection_offset, 0, 0, tex.right, tex.bottom, TEXTURE_TRANSPARENT});
            break;
        case AABB::TOP:
            nglAddVertex({0, selection_pos_abs.y - indicator_y + selection_offset, 0, tex.left, tex.bottom, TEXTURE_TRANSPARENT});
            nglAddVertex({0, selection_pos_abs.y - indicator_y + selection_offset, BLOCK_SIZE, tex.left, tex.top, TEXTURE_TRANSPARENT});
            nglAddVertex({BLOCK_SIZE, selection_pos_abs.y - indicator_y + selection_offset, BLOCK_SIZE, tex.right, tex.top, TEXTURE_TRANSPARENT});
            nglAddVertex({BLOCK_SIZE, selection_pos_abs.y - indicator_y + selection_offset, 0, tex.right, tex.bottom, TEXTURE_TRANSPARENT});
            break;
        case AABB::BOTTOM:
            nglAddVertex({BLOCK_SIZE, selection_pos_abs.y - indicator_y - selection_offset, 0, tex.left, tex.bottom, TEXTURE_TRANSPARENT});
            nglAddVertex({BLOCK_SIZE, selection_pos_abs.y - indicator_y - selection_offset, BLOCK_SIZE, tex.left, tex.top, TEXTURE_TRANSPARENT});
            nglAddVertex({0, selection_pos_abs.y - indicator_y - selection_offset, BLOCK_SIZE, tex.right, tex.top, TEXTURE_TRANSPARENT});
            nglAddVertex({0, selection_pos_abs.y - indicator_y - selection_offset, 0, tex.right, tex.bottom, TEXTURE_TRANSPARENT});
            break;
        case AABB::NONE:
            break;
        }
        glEnd();

        glPopMatrix();
    }
    glBindTexture(terrain_current);

    glPopMatrix();

    crosshairPixel(0, 0);
    crosshairPixel(-1, 0);
    crosshairPixel(-2, 0);
    crosshairPixel(0, -1);
    crosshairPixel(0, -2);
    crosshairPixel(1, 0);
    crosshairPixel(2, 0);
    crosshairPixel(0, 1);
    crosshairPixel(0, 2);

    // Underwater the view takes on the water's colour.
    if(in_water)
        tintRect(*screen, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, UNDERWATER_TINT);

    //Don't draw the inventory when drawing the background for BlockListTask
    if(draw_inventory)
    {
        current_inventory.draw(*screen);
        const int top = drawStatusBars(*screen, in_water);
        const char *name = itemName(player_inventory.selectedStack());
        if(*name)
            drawPixelTextCenter(*screen, name, SCREEN_WIDTH / 2, top - 9, ui::TEXT_WHITE);
    }

    if(sleep_ticks > 0)
    {
        // darker and darker as the night is skipped
        const int passes = 1 + (SLEEP_FADE_TICKS - sleep_ticks) * 4 / SLEEP_FADE_TICKS;
        for(int i = 0; i < passes; ++i)
            darkenRect(*screen, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    }

    if(message_timeout > 0)
    {
        drawPixelText(*screen, message, 3, 3, ui::TEXT_WHITE);
        --message_timeout;
    }

    #ifdef FPS_COUNTER
        if(message_timeout == 0 && settings_task.getValue(SettingsTask::SHOW_FPS))
        {
            // The biome too, like 1.8.8's debug screen.
            const Biome biome = biomeAt(world.noiseGenerator(), (x / BLOCK_SIZE).floor(), (z / BLOCK_SIZE).floor());
            snprintf(this->message, sizeof(this->message), "FPS: %u  %s", fps, biomeName(biome));
            message_timeout = 20;
        }
    #endif

    #ifdef STRESS_TEST
        snprintf(this->message, sizeof(this->message), "FPS %u | TPS %u | ents %d | light %d | tex %uKB",
                 fps, stress_tps, entity_pool.count(), stressBrightnessForTick(stress_tick_count),
                 static_cast<unsigned>(lightingMemoryBytes() / 1024));
        drawPixelText(*screen, this->message, 3, 3, ui::TEXT_WHITE);
    #endif

    frame_counter++;
}

void WorldTask::resetWorld()
{
    x = z = 0;
    y = World::HEIGHT * Chunk::SIZE * BLOCK_SIZE;
    xr = yr = 0;
    world.generateSeed();
    world.clear();
    current_inventory.resetToDefaults();
    resetProgress();
    // Nothing from the previous world carries over.
    block_storage.clear();
    item_piles.clear();
    entity_pool = EntityPool();
    mining = MiningState();
    sleep_ticks = 0;
    mob_rng = 0x6d6f62;
    mob_spawn_timer = 0;
    mob_hit_cooldown = 0;
    aim_mob = nullptr;
    creative_flight = false;
    jump_key_was_down = false;
    jump_tap_ticks = 0;
    input_jump = input_sprint = input_descend = false;
    world_state.trader_next_visit = 0;
}

void WorldTask::setMessage(const char *message)
{
    if(strlen(message) >= sizeof(this->message))
        return;

    strcpy(this->message, message);
    message_timeout = 50;
}
