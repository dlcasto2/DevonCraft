#include "worldloadertask.h"

#include <cstdio>
#include <string>
#include <unistd.h>

#include "menutask.h"
#include "uikit.h"
#include "worldtask.h"

WorldLoaderTask world_loader_task;

namespace {

constexpr unsigned int WORLD_SLOT_COUNT = 8;
constexpr unsigned int BACK_ROW = WORLD_SLOT_COUNT;
constexpr unsigned int ROW_COUNT = WORLD_SLOT_COUNT + 1;
constexpr int BUTTON_HEIGHT = 15;
constexpr int BUTTON_GAP = 2;
constexpr int BUTTON_TOP = 47;

std::string slot_paths[WORLD_SLOT_COUNT];
std::string active_path;
bool slot_exists[WORLD_SLOT_COUNT] = {};
int slot_versions[WORLD_SLOT_COUNT] = {};
bool has_active_world = false;

void activatePath(const std::string &path)
{
    active_path = path;
    Task::savefile = active_path.c_str();
}

bool supportedVersion(int version)
{
    return version >= 4 && version <= 8;
}

void refreshSlots()
{
    for(unsigned int i = 0; i < WORLD_SLOT_COUNT; ++i)
    {
        slot_exists[i] = access(slot_paths[i].c_str(), F_OK) == 0;
        slot_versions[i] = slot_exists[i] ? readSaveVersion(slot_paths[i].c_str()) : -1;
    }
}

bool isCurrentSlot(unsigned int slot)
{
    return has_active_world && slot_paths[slot] == active_path;
}

bool canCreateIn(unsigned int slot)
{
    return !slot_exists[slot] && !isCurrentSlot(slot);
}

bool canLoadFrom(unsigned int slot)
{
    return slot_exists[slot] || isCurrentSlot(slot);
}

void makeSlotLabel(unsigned int slot, char *label, unsigned int size)
{
    if(isCurrentSlot(slot))
    {
        std::snprintf(label, size, "World %u - Current", slot + 1);
        return;
    }

    if(!slot_exists[slot])
    {
        std::snprintf(label, size, "World %u - Empty", slot + 1);
        return;
    }

    if(supportedVersion(slot_versions[slot]))
        std::snprintf(label, size, "World %u - Saved (v%d)", slot + 1, slot_versions[slot]);
    else
        std::snprintf(label, size, "World %u - Unreadable", slot + 1);
}

} // namespace

void WorldLoaderTask::setPrimaryPath(const char *path)
{
    slot_paths[0] = (path && *path) ? path : "/documents/ndless/devoncraft.map.tns";

    for(unsigned int i = 1; i < WORLD_SLOT_COUNT; ++i)
    {
        char filename[72];
        std::snprintf(filename, sizeof(filename),
                      "/documents/ndless/devoncraft-world-%u.map.tns", i + 1);
        slot_paths[i] = filename;
    }

    activatePath(slot_paths[0]);
    has_active_world = false;
    refreshSlots();
}

void WorldLoaderTask::open(WorldLoaderMode next_mode)
{
    mode = next_mode;
    selected = 0;
    status[0] = 0;
    status_timeout = 0;
    refreshSlots();
    Task::makeCurrent();
}

void WorldLoaderTask::showStatus(const char *message)
{
    std::snprintf(status, sizeof(status), "%s", message ? message : "");
    status_timeout = 60;
}

void WorldLoaderTask::goBack()
{
    if(mode == WorldLoaderMode::STARTUP)
        running = false;
    else
        menu_task.makeCurrent();
}

void WorldLoaderTask::selectSlot(unsigned int slot)
{
    if(slot >= WORLD_SLOT_COUNT)
    {
        goBack();
        return;
    }

    const std::string target_path = slot_paths[slot];
    const bool exists = access(target_path.c_str(), F_OK) == 0;
    const bool current = has_active_world && target_path == active_path;

    if(mode == WorldLoaderMode::CREATE)
    {
        if(exists || current)
        {
            showStatus("That slot already contains a world.");
            refreshSlots();
            return;
        }

        if(has_active_world && !Task::save())
        {
            showStatus("Current world could not be saved.");
            return;
        }

        activatePath(target_path);
        world_task.resetWorld();
        has_active_world = true;
        world_task.setMessage("New world created.");
        world_task.makeCurrent();
        return;
    }

    if(mode == WorldLoaderMode::LOAD && !exists)
    {
        if(current)
        {
            world_task.makeCurrent();
            return;
        }
        showStatus("This slot is empty.");
        return;
    }

    if(mode == WorldLoaderMode::STARTUP && !exists)
    {
        activatePath(target_path);
        world_task.resetWorld();
        has_active_world = true;
        world_task.setMessage("New world created.");
        world_task.makeCurrent();
        return;
    }

    // Selecting the active world is a no-op; do not reload and discard
    // progress made since the last manual save.
    if(current)
    {
        world_task.makeCurrent();
        return;
    }

    const std::string previous_path = active_path;
    const bool had_active_world = has_active_world;
    if(had_active_world && !Task::save())
    {
        showStatus("Current world could not be saved; switch cancelled.");
        return;
    }

    activatePath(target_path);
    const LoadResult result = Task::load();
    if(result == LoadResult::OK)
    {
        has_active_world = true;
        world_task.setMessage("World loaded.");
        world_task.makeCurrent();
        return;
    }

    // Task::load deliberately discards partial data on corruption. Since the
    // outgoing world was saved above, restore it before returning to play.
    if(had_active_world)
    {
        activatePath(previous_path);
        const LoadResult restore = Task::load();
        if(restore == LoadResult::OK)
        {
            has_active_world = true;
            world_task.setMessage("Load failed; previous world restored.");
            world_task.makeCurrent();
            return;
        }

        has_active_world = false;
        showStatus("Load failed; previous world could not be restored.");
        return;
    }

    refreshSlots();
    showStatus(result == LoadResult::UNREADABLE
               ? "Save unreadable. Original file kept."
               : "World file could not be opened.");
}

void WorldLoaderTask::render()
{
    if(background_saved)
    {
        drawBackground();
        darkenRect(*screen, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    }
    else
        fillRect(*screen, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0x0841);

    drawPanel(*screen, 15, 14, SCREEN_WIDTH - 30, SCREEN_HEIGHT - 30);
    drawPixelTextCenter(*screen, "Worlds", SCREEN_WIDTH / 2, 21, ui::TITLE);

    const char *subtitle = mode == WorldLoaderMode::CREATE
                         ? "Choose an empty slot"
                         : mode == WorldLoaderMode::LOAD
                         ? "Choose a saved world"
                         : "Select or create a world";
    drawPixelTextCenter(*screen, subtitle, SCREEN_WIDTH / 2, 33, ui::TEXT_WHITE);

    const int button_x = (SCREEN_WIDTH - (SCREEN_WIDTH - 70)) / 2;
    const int button_w = SCREEN_WIDTH - 70;
    for(unsigned int i = 0; i < WORLD_SLOT_COUNT; ++i)
    {
        char label[48];
        makeSlotLabel(i, label, sizeof(label));
        const bool enabled = mode == WorldLoaderMode::STARTUP
                          || (mode == WorldLoaderMode::LOAD && canLoadFrom(i))
                          || (mode == WorldLoaderMode::CREATE && canCreateIn(i));
        drawButton(*screen, button_x, BUTTON_TOP + static_cast<int>(i) * (BUTTON_HEIGHT + BUTTON_GAP),
                   button_w, BUTTON_HEIGHT, label, selected == i, enabled);
    }

    drawButton(*screen, button_x, BUTTON_TOP + static_cast<int>(BACK_ROW) * (BUTTON_HEIGHT + BUTTON_GAP),
               button_w, BUTTON_HEIGHT, "Back", selected == BACK_ROW);

    if(status_timeout > 0)
    {
        drawPixelTextCenter(*screen, status, SCREEN_WIDTH / 2, SCREEN_HEIGHT - 24, ui::YELLOW);
        --status_timeout;
    }

    drawPixelText(*screen, "8/2 move   5 select   esc/menu back", 3, SCREEN_HEIGHT - 11, ui::GREY_TEXT);
}

void WorldLoaderTask::logic()
{
    if(key_held_down)
    {
        key_held_down = keyPressed(KEY_NSPIRE_ESC) || keyPressed(KEY_NSPIRE_MENU)
                     || keyPressed(KEY_NSPIRE_UP) || keyPressed(KEY_NSPIRE_DOWN)
                     || keyPressed(KEY_NSPIRE_8) || keyPressed(KEY_NSPIRE_2)
                     || keyPressed(KEY_NSPIRE_5) || keyPressed(KEY_NSPIRE_CLICK);
        return;
    }

    if(keyPressed(KEY_NSPIRE_ESC) || keyPressed(KEY_NSPIRE_MENU))
    {
        goBack();
        key_held_down = true;
    }
    else if(keyPressed(KEY_NSPIRE_UP) || keyPressed(KEY_NSPIRE_8))
    {
        selected = (selected + ROW_COUNT - 1) % ROW_COUNT;
        key_held_down = true;
    }
    else if(keyPressed(KEY_NSPIRE_DOWN) || keyPressed(KEY_NSPIRE_2))
    {
        selected = (selected + 1) % ROW_COUNT;
        key_held_down = true;
    }
    else if(keyPressed(KEY_NSPIRE_5) || keyPressed(KEY_NSPIRE_CLICK))
    {
        selectSlot(selected);
        key_held_down = true;
    }
}
