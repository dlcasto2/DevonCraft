#include "savefile.h"

#include <cstdio>

#include "blockstorage.h"
#include "itempile.h"

bool writeSurvivalSection(SaveWriter &w)
{
    w.u8(static_cast<uint8_t>(player_state.mode));
    w.u8(static_cast<uint8_t>(world_state.difficulty));
    w.u8(world_state.hardcore ? 1 : 0);
    w.u8(world_state.hardcore_dead ? 1 : 0);

    w.u32(world_state.time_of_day);
    w.u32(world_state.trader_next_visit);

    w.u8(player_state.health);
    w.u8(player_state.hunger);
    w.u16(player_state.saturation_x100);
    w.u16(player_state.exhaustion_x1000);
    w.u16(player_state.air_ticks);
    w.u16(player_state.burning_ticks);
    w.i32(player_state.spawn_x);
    w.i32(player_state.spawn_y);
    w.i32(player_state.spawn_z);
    w.u8(player_state.spawn_is_bed ? 1 : 0);

    w.u8(static_cast<uint8_t>(player_inventory.selected));
    for(const ItemStack &s : player_inventory.slots)
        w.stack(s);

    block_storage.save(w);
    item_piles.save(w);
    return w.ok();
}

bool readSurvivalSection(SaveReader &r)
{
    uint8_t mode, difficulty, hardcore, hardcore_dead;
    if(!r.u8(mode) || !r.u8(difficulty) || !r.u8(hardcore) || !r.u8(hardcore_dead))
        return false;
    player_state.mode = static_cast<GameMode>(mode);
    world_state.difficulty = static_cast<Difficulty>(difficulty);
    world_state.hardcore = hardcore != 0;
    world_state.hardcore_dead = hardcore_dead != 0;

    if(!r.u32(world_state.time_of_day) || !r.u32(world_state.trader_next_visit))
        return false;

    if(!r.u8(player_state.health) || !r.u8(player_state.hunger)
       || !r.u16(player_state.saturation_x100) || !r.u16(player_state.exhaustion_x1000)
       || !r.u16(player_state.air_ticks) || !r.u16(player_state.burning_ticks)
       || !r.i32(player_state.spawn_x) || !r.i32(player_state.spawn_y) || !r.i32(player_state.spawn_z))
        return false;
    uint8_t spawn_is_bed;
    if(!r.u8(spawn_is_bed))
        return false;
    player_state.spawn_is_bed = spawn_is_bed != 0;

    uint8_t selected;
    if(!r.u8(selected))
        return false;
    // The serialized value is a hotbar index, not an arbitrary inventory slot.
    if(selected >= PlayerInventory::HOTBAR)
        return false;
    player_inventory.selected = selected;
    for(ItemStack &s : player_inventory.slots)
        if(!r.stack(s))
            return false;

    return block_storage.load(r) && item_piles.load(r);
}

void convertV6Inventory(const BLOCK_WDATA (&entries)[5], int current_slot, PlayerInventory &inv, PlayerState &ps)
{
    inv.clear();
    for(int i = 0; i < 5; ++i)
        inv.slots[i] = ItemStack::ofBlock(entries[i]);
    inv.selected = (current_slot >= 0 && current_slot < 5) ? current_slot : 0;
    ps.mode = GameMode::CREATIVE;
}

static std::string withSuffix(const std::string &path, const char *suffix)
{
    const std::string tail = ".map.tns";
    if(path.size() >= tail.size() && path.compare(path.size() - tail.size(), tail.size(), tail) == 0)
        return path.substr(0, path.size() - tail.size()) + ".map." + suffix + ".tns";
    return path + "." + suffix;
}

std::string backupPathFor(const std::string &path) { return withSuffix(path, "bak"); }
std::string tempPathFor(const std::string &path) { return withSuffix(path, "tmp"); }
std::string swapPathFor(const std::string &path) { return withSuffix(path, "swap"); }
std::string newWorldPathFor(const std::string &path) { return withSuffix(path, "new"); }

std::string chooseSavePath(const std::string &original, LoadResult r)
{
    return r == LoadResult::UNREADABLE ? newWorldPathFor(original) : original;
}

int readSaveVersion(const char *path)
{
    gzFile f = gzopen(path, "rb");
    if(!f)
        return -1;
    int version;
    const int read = gzread(f, &version, sizeof(version));
    gzclose(f);
    return read == sizeof(version) ? version : -2;
}

static bool copyFile(const std::string &source_path, const std::string &destination_path)
{
    FILE *source = fopen(source_path.c_str(), "rb");
    if(!source)
        return false;

    FILE *destination = fopen(destination_path.c_str(), "wb");
    if(!destination)
    {
        fclose(source);
        return false;
    }

    char buffer[1024];
    bool ok = true;
    for(;;)
    {
        const size_t count = fread(buffer, 1, sizeof(buffer), source);
        if(count && fwrite(buffer, 1, count, destination) != count)
        {
            ok = false;
            break;
        }
        if(count < sizeof(buffer))
        {
            if(ferror(source))
                ok = false;
            break;
        }
    }

    if(fclose(source) != 0)
        ok = false;
    if(fclose(destination) != 0)
        ok = false;
    if(!ok)
        remove(destination_path.c_str());
    return ok;
}

static bool fileExists(const std::string &path)
{
    FILE *file = fopen(path.c_str(), "rb");
    if(!file)
        return false;
    fclose(file);
    return true;
}

static bool completeGzipFile(const std::string &path)
{
    gzFile file = gzopen(path.c_str(), "rb");
    if(!file)
        return false;

    char buffer[1024];
    int count;
    do
    {
        count = gzread(file, buffer, sizeof(buffer));
    }
    while(count > 0);

    int error_code = Z_OK;
    gzerror(file, &error_code);
    const int close_result = gzclose(file);
    return count == 0
        && (error_code == Z_OK || error_code == Z_STREAM_END)
        && close_result == Z_OK;
}

static bool validPendingSave(const std::string &path)
{
    const int version = readSaveVersion(path.c_str());
    return version >= 4 && version <= 8 && completeGzipFile(path);
}

// Install the already-closed temporary file. The first rename is the fast,
// atomic path on filesystems that replace existing destinations. On the
// calculator filesystem, rename may reject an existing destination; in that
// case move the old file aside, install the completed temp, then clean up.
// The old file is copied to .bak once before replacement whenever possible.
static bool installTempSave(const std::string &path, SaveRenameFunction move_file,
                            bool require_valid_save)
{
    const std::string temp = tempPathFor(path);
    if(require_valid_save && !validPendingSave(temp))
        return false;
    if(!fileExists(temp))
        return false;

    const std::string backup = backupPathFor(path);
    const bool had_target = fileExists(path);

    if(had_target && !fileExists(backup))
    {
        if(!copyFile(path, backup))
            return false;
    }

    // POSIX-like implementations replace the destination atomically here.
    if(move_file(temp.c_str(), path.c_str()) == 0)
        return true;

    // Some calculator filesystem implementations reject rename when the
    // destination already exists. Never delete it: move it to a rollback
    // path, then rename the completed temp into the now-empty destination.
    if(!had_target || !fileExists(path))
        return false;

    const std::string swap = swapPathFor(path);
    if(fileExists(swap) || move_file(path.c_str(), swap.c_str()) != 0)
        return false;

    if(move_file(temp.c_str(), path.c_str()) == 0)
    {
        // A copy of the old save already exists in .bak. If not, preserve the
        // displaced file there; if that rename fails, .swap remains recoverable.
        if(fileExists(backup))
            remove(swap.c_str());
        else
            move_file(swap.c_str(), backup.c_str());
        return true;
    }

    // The new file could not be installed. Restore the old name; if rename
    // itself fails, copy the rollback file back without deleting the rollback.
    if(move_file(swap.c_str(), path.c_str()) != 0 && !copyFile(swap, path))
        return false;
    return false;
}

bool recoverSavePath(const std::string &path, SaveRenameFunction rename_function)
{
    const SaveRenameFunction move_file = rename_function ? rename_function : ::rename;
    const std::string temp = tempPathFor(path);
    const std::string swap = swapPathFor(path);
    const std::string backup = backupPathFor(path);
    bool has_target = fileExists(path);
    bool has_swap = fileExists(swap);

    // A crash may occur after the old save was moved aside but before the
    // completed temp was installed. Prefer a fully valid pending save; if it
    // is incomplete, restore the old save instead.
    if(!has_target && has_swap)
    {
        if(validPendingSave(temp) && move_file(temp.c_str(), path.c_str()) == 0)
        {
            if(fileExists(backup))
                remove(swap.c_str());
            else
                move_file(swap.c_str(), backup.c_str());
            return true;
        }

        if(move_file(swap.c_str(), path.c_str()) == 0 || copyFile(swap, path))
            return true;
        return false;
    }

    // A crash may occur after the new target was installed but before the
    // displaced old file was cleaned up.
    if(has_target && has_swap)
    {
        if(!fileExists(backup) && move_file(swap.c_str(), backup.c_str()) == 0)
            has_swap = false;
        if(has_swap && fileExists(backup))
            remove(swap.c_str());
    }

    // Older builds could leave a complete .tmp when rename over the existing
    // default world failed. Promote it only after validating the entire gzip
    // stream and supported save version, keeping the old file recoverable.
    if(fileExists(path) && validPendingSave(temp))
        return installTempSave(path, move_file, true);

    // A brand-new world may have a valid temp but no final path yet.
    if(!fileExists(path) && validPendingSave(temp))
        return installTempSave(path, move_file, true);

    return true;
}

bool replaceSaveSafely(const std::string &path, bool (*write)(gzFile, void *), void *ctx,
                       SaveRenameFunction rename_function)
{
    const SaveRenameFunction move_file = rename_function ? rename_function : ::rename;

    // Finish any previous interrupted replacement before reusing the temp path.
    if(!recoverSavePath(path, move_file))
        return false;

    const std::string temp = tempPathFor(path);
    gzFile file = gzopen(temp.c_str(), "wb");
    if(!file)
        return false;

    const bool written = write(file, ctx);
    const bool closed_ok = gzclose(file) == Z_OK;
    if(!written || !closed_ok)
    {
        remove(temp.c_str());
        return false;
    }

    // The callback just wrote and closed this temp in this call, so the full
    // stream is trusted here; the recovery path performs an explicit CRC check.
    return installTempSave(path, move_file, false);
}
