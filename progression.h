#ifndef PROGRESSION_H
#define PROGRESSION_H
#include <cstdint>
#include "items.h"
#include "terrain.h"
class SaveReader;
class SaveWriter;
enum AchievementId : uint8_t {
 ACH_TAKING_INVENTORY = 0, ACH_GETTING_WOOD, ACH_BENCHMARKING, ACH_TIME_TO_MINE,
 ACH_HOT_TOPIC, ACH_ACQUIRE_HARDWARE, ACH_DIAMONDS, ACH_SWEET_DREAMS, ACH_ARCHITECT,
 ACHIEVEMENT_COUNT
};
enum AdvancementId : uint8_t {
 ADV_ROOT = 0, ADV_GETTING_WOOD, ADV_BENCHMARKING, ADV_STONE_AGE, ADV_GETTING_UPGRADE,
 ADV_HOT_TOPIC, ADV_ACQUIRE_HARDWARE, ADV_DIAMONDS, ADV_SWEET_DREAMS, ADV_ARCHITECT,
 ADVANCEMENT_COUNT
};
struct ProgressEntry { const char *name; const char *description; };
struct ProgressState {
 uint32_t achievement_bits = 0;
 uint32_t advancement_bits = 0;
 uint32_t advancement_criteria = 0;
 uint16_t blocks_placed = 0;
};
extern ProgressState progress_state;
unsigned achievementCount();
unsigned advancementCount();
const ProgressEntry &achievementEntry(unsigned index);
const ProgressEntry &advancementEntry(unsigned index);
bool achievementUnlocked(AchievementId id);
bool advancementUnlocked(AdvancementId id);
bool advancementCriterionMet(AdvancementId id);
unsigned unlockedAchievementCount();
unsigned unlockedAdvancementCount();
void resetProgress();
void progressionInventoryOpened();
void progressionItemAcquired(ItemId id, unsigned count = 1);
void progressionCrafted(ItemId id, unsigned count = 1);
void progressionBlockMined(BLOCK block);
void progressionBlockPlaced(BLOCK block);
const char *takeProgressNotice();
bool writeProgressSection(SaveWriter &writer);
bool readProgressSection(SaveReader &reader);
#endif
