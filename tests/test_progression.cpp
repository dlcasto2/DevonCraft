#include "doctest.h"
#include "progression.h"
#include "saveio.h"

#include <cstdio>
#include <cstring>

TEST_CASE("inventory unlocks the root advancement and Taking Inventory") {
    resetProgress();
    progressionInventoryOpened();
    CHECK(achievementUnlocked(ACH_TAKING_INVENTORY));
    CHECK(advancementUnlocked(ADV_ROOT));
    CHECK(unlockedAchievementCount() == 1);
    CHECK(unlockedAdvancementCount() == 1);
}

TEST_CASE("advancement criteria wait for their parent and then complete") {
    resetProgress();
    progressionItemAcquired(BLOCK_WOOD);
    CHECK(achievementUnlocked(ACH_GETTING_WOOD));
    CHECK(advancementCriterionMet(ADV_GETTING_WOOD));
    CHECK_FALSE(advancementUnlocked(ADV_GETTING_WOOD));

    progressionInventoryOpened();
    CHECK(advancementUnlocked(ADV_ROOT));
    CHECK(advancementUnlocked(ADV_GETTING_WOOD));

    progressionCrafted(BLOCK_CRAFTING_TABLE);
    progressionBlockMined(BLOCK_STONE);
    CHECK(advancementUnlocked(ADV_BENCHMARKING));
    CHECK(advancementUnlocked(ADV_STONE_AGE));

    progressionCrafted(toolId(ToolMaterial::STONE, ToolKind::PICKAXE));
    progressionCrafted(BLOCK_FURNACE);
    progressionItemAcquired(ITEM_IRON_INGOT);
    progressionItemAcquired(ITEM_DIAMOND);
    progressionCrafted(BLOCK_BED);
    CHECK(advancementUnlocked(ADV_GETTING_UPGRADE));
    CHECK(advancementUnlocked(ADV_HOT_TOPIC));
    CHECK(advancementUnlocked(ADV_ACQUIRE_HARDWARE));
    CHECK(advancementUnlocked(ADV_DIAMONDS));
    CHECK(advancementUnlocked(ADV_SWEET_DREAMS));
    CHECK(achievementUnlocked(ACH_TIME_TO_MINE));
    CHECK(achievementUnlocked(ACH_HOT_TOPIC));
    CHECK(achievementUnlocked(ACH_ACQUIRE_HARDWARE));
    CHECK(achievementUnlocked(ACH_DIAMONDS));
    CHECK(achievementUnlocked(ACH_SWEET_DREAMS));
}

TEST_CASE("placing one hundred blocks completes Architect") {
    resetProgress();
    progressionInventoryOpened();
    progressionItemAcquired(BLOCK_WOOD);
    for(int i = 0; i < 100; ++i)
        progressionBlockPlaced(BLOCK_DIRT);
    CHECK(progress_state.blocks_placed == 100);
    CHECK(achievementUnlocked(ACH_ARCHITECT));
    CHECK(advancementUnlocked(ADV_ARCHITECT));
    progressionBlockPlaced(BLOCK_STONE);
    CHECK(progress_state.blocks_placed == 100);
}

TEST_CASE("progression unlock notices are consumed once") {
    resetProgress();
    progressionInventoryOpened();
    const char *notice = takeProgressNotice();
    REQUIRE(notice != nullptr);
    CHECK(std::strstr(notice, "Achievement") != nullptr);
    const char *advancement_notice = takeProgressNotice();
    REQUIRE(advancement_notice != nullptr);
    CHECK(std::strstr(advancement_notice, "Advancement") != nullptr);
    CHECK(takeProgressNotice() == nullptr);
}

TEST_CASE("progression save section round-trips") {
    resetProgress();
    progressionInventoryOpened();
    progressionItemAcquired(BLOCK_WOOD);
    progressionCrafted(BLOCK_CRAFTING_TABLE);
    progressionBlockMined(BLOCK_STONE);
    for(int i = 0; i < 42; ++i)
        progressionBlockPlaced(BLOCK_PLANKS_NORMAL);

    gzFile out = gzopen("/tmp/devoncraft_progress.gz", "wb");
    REQUIRE(out != nullptr);
    SaveWriter writer(out);
    CHECK(writeProgressSection(writer));
    CHECK(gzclose(out) == Z_OK);

    resetProgress();
    gzFile in = gzopen("/tmp/devoncraft_progress.gz", "rb");
    REQUIRE(in != nullptr);
    SaveReader reader(in);
    CHECK(readProgressSection(reader));
    CHECK(gzclose(in) == Z_OK);

    CHECK(achievementUnlocked(ACH_TAKING_INVENTORY));
    CHECK(achievementUnlocked(ACH_GETTING_WOOD));
    CHECK(advancementUnlocked(ADV_ROOT));
    CHECK(advancementUnlocked(ADV_GETTING_WOOD));
    CHECK(advancementUnlocked(ADV_BENCHMARKING));
    CHECK(advancementUnlocked(ADV_STONE_AGE));
    CHECK(progress_state.blocks_placed == 42);
}

TEST_CASE("progression save section rejects invalid unlock bits and counters") {
    resetProgress();
    gzFile out = gzopen("/tmp/devoncraft_progress_bad.gz", "wb");
    REQUIRE(out != nullptr);
    SaveWriter writer(out);
    CHECK(writer.u32(0x80000000u));
    CHECK(writer.u32(0));
    CHECK(writer.u32(0));
    CHECK(writer.u16(0));
    CHECK(gzclose(out) == Z_OK);

    gzFile in = gzopen("/tmp/devoncraft_progress_bad.gz", "rb");
    REQUIRE(in != nullptr);
    SaveReader reader(in);
    CHECK_FALSE(readProgressSection(reader));
    CHECK(gzclose(in) == Z_OK);
}
