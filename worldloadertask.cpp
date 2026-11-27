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

// Try a recovery copy without changing the active world's canonical save path.
// Task::load is synchronous, so the candidate string remains alive throughout
// the read; restore the active pointer before returning to the caller.
LoadResult loadFromTemporaryPath(const std::string &path)
{
    const char *const active_savefile = Task::savefile;
    Task::savefile = path.c_str();
    const LoadResult result = Task::load();
    Task::savefile = active_savefile;
    return result;
}

// Keep a byte-for-byte copy of the just-saved active world while another slot
// is being read. This is especially important for newly created worlds, which
// may not have a .bak yet.
bool copySaveFile(const std::string &source_path, const std::string &copy_path)
{
    FILE *source = std::fopen(source_path.c_str(), "rb");
    if(!source)
        return false;

    FILE *destination = std::fopen(copy_path.c_str(), "wb");
    if(!destination)
    {
        std::fclose(source);
        return false;
    }

    char buffer[1024];
    bool ok = true;
    for(;;)
    {
        const size_t count = std::fread(buffer, 1, sizeof(buffer), source);
        if(count && std::fwrite(buffer, 1, count, destination) != count)
        {
            ok = false;
            break;
        }
        if(count < sizeof(buffer))
        {
            if(std::ferror(source))
                ok = false;
            break;
        }
    }

    if(std::fclose(source) != 0)
        ok = false;
    if(std::fclose(destination) != 0)
        ok = false;
    if(!ok)
        std::remove(copy_path.c_str());
    return ok;
}

bool supportedVersion(int version)
{
    return version >= 4 && version <= 8;
}

void refreshSlots()
{
    for(unsigned int i = 0; i < WORLD_SLOT_COUNT; ++i)
    {
        // Recover a completed save left in .tmp by older builds before showing
        // the slot state or attempting to load it.
        recoverSavePath(slot_paths[i]);
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
    else if(!has_active_world)
        showStatus("Choose a readable world before continuing.");
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
    std::string resume_path;
    if(had_active_world)
    {
        if(!Task::save())
        {
            showStatus("Current world could not be saved; switch cancelled.");
            return;
        }

        resume_path = previous_path + ".switch-restore.tns";
        if(!copySaveFile(previous_path, resume_path))
        {
            showStatus("World saved; recovery copy failed. Switch cancelled.");
            return;
        }
    }

    activatePath(target_path);
    LoadResult result = Task::load();
    if(result == LoadResult::OK)
    {
        if(!resume_path.empty())
            std::remove(resume_path.c_str());
        has_active_world = true;
        world_task.setMessage("World loaded.");
        world_task.makeCurrent();
        return;
    }

    // A previously saved world may have a usable .bak even when its primary
    // file is damaged. Try that copy before abandoning the requested switch.
    const std::string target_backup = backupPathFor(target_path);
    if(access(target_backup.c_str(), F_OK) == 0)
    {
        const LoadResult backup_result = loadFromTemporaryPath(target_backup);
        if(backup_result == LoadResult::OK)
        {
            if(!resume_path.empty())
                std::remove(resume_path.c_str());
            activatePath(target_path);
            has_active_world = true;
            world_task.setMessage("World loaded from backup.");
            world_task.makeCurrent();
            return;
        }
    }

    // Task::load deliberately discards partial data on corruption. The outgoing
    // world was saved before attempting the switch, so restore its primary file.
    if(had_active_world)
    {
        activatePath(previous_path);
        LoadResult restore = Task::load();
        if(restore != LoadResult::OK && !resume_path.empty())
            restore = loadFromTemporaryPath(resume_path);

        if(restore != LoadResult::OK)
        {
            // The canonical save and exact resume copy are preferred; the .bak
            // is an older last-resort recovery point.
            const std::string previous_backup = backupPathFor(previous_path);
            if(access(previous_backup.c_str(), F_OK) == 0)
                restore = loadFromTemporaryPath(previous_backup);
        }

        if(restore == LoadResult::OK)
        {
            if(!resume_path.empty())
                std::remove(resume_path.c_str());
            activatePath(previous_path);
            has_active_world = true;
            world_task.setMessage("Load failed; previous world restored.");
            world_task.makeCurrent();
            return;
        }

        has_active_world = false;
        if(restore == LoadResult::UNREADABLE)
            activatePath(chooseSavePath(previous_path, restore));
        // No readable copy was found. Keep the save files intact and return to
        // the picker with an explicit failure instead of creating a new world.
        mode = WorldLoaderMode::STARTUP;
        refreshSlots();
        showStatus("World switch failed; saved copies kept.");
        return;
    }

    if(result == LoadResult::UNREADABLE)
        activatePath(chooseSavePath(target_path, result));

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
