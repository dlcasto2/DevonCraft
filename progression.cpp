#include "progression.h"
#include <cstdio>
#include "saveio.h"

ProgressState progress_state;
namespace {
const ProgressEntry achievements[ACHIEVEMENT_COUNT] = {
 {"Taking Inventory","Open your inventory for the first time."},
 {"Getting Wood","Pick up a wood log."},
 {"Benchmarking","Craft a crafting table."},
 {"Time to Mine!","Craft a pickaxe."},
 {"Hot Topic","Craft a furnace."},
 {"Acquire Hardware","Obtain an iron ingot."},
 {"DIAMONDS!","Obtain a diamond."},
 {"Sweet Dreams","Craft a bed."},
 {"Architect","Place 100 blocks."}
};
const ProgressEntry advancements[ADVANCEMENT_COUNT] = {
 {"DevonCraft","Open your inventory and begin your world journey."},
 {"Getting Wood","Pick up a wood log."},
 {"Benchmarking","Craft a crafting table."},
 {"Stone Age","Mine a stone block."},
 {"Getting an Upgrade","Craft a stone pickaxe."},
 {"Hot Topic","Craft a furnace."},
 {"Acquire Hardware","Obtain an iron ingot."},
 {"DIAMONDS!","Obtain a diamond."},
 {"Sweet Dreams","Craft a bed."},
 {"Architect","Place 100 blocks."}
};
const int8_t parents[ADVANCEMENT_COUNT] = {
 -1, ADV_ROOT, ADV_GETTING_WOOD, ADV_GETTING_WOOD, ADV_STONE_AGE,
 ADV_BENCHMARKING, ADV_HOT_TOPIC, ADV_ACQUIRE_HARDWARE, ADV_GETTING_WOOD, ADV_GETTING_WOOD
};
char notice[48];
bool notice_pending = false;
uint32_t bit(unsigned i) { return static_cast<uint32_t>(1u) << i; }
uint32_t validMask(unsigned n) { return bit(n) - 1u; }
void queueNotice(const char *kind, const char *name) {
 if(notice_pending) return;
 std::snprintf(notice,sizeof(notice),"%s: %.30s",kind,name);
 notice_pending=true;
}
void setAchievement(AchievementId id) {
 const unsigned i=static_cast<unsigned>(id);
 if(i>=ACHIEVEMENT_COUNT || (progress_state.achievement_bits & bit(i))) return;
 progress_state.achievement_bits |= bit(i);
 queueNotice("Achievement",achievements[i].name);
}
void setCriterion(AdvancementId id) {
 const unsigned i=static_cast<unsigned>(id);
 if(i<ADVANCEMENT_COUNT) progress_state.advancement_criteria |= bit(i);
}
void evaluateAdvancements() {
 bool changed;
 do {
  changed=false;
  for(unsigned i=0;i<ADVANCEMENT_COUNT;++i) {
   const uint32_t mask=bit(i);
   if(!(progress_state.advancement_criteria & mask) || (progress_state.advancement_bits & mask)) continue;
   const int parent=parents[i];
   if(parent>=0 && !(progress_state.advancement_bits & bit(static_cast<unsigned>(parent)))) continue;
   progress_state.advancement_bits |= mask;
   queueNotice("Advancement",advancements[i].name);
   changed=true;
  }
 } while(changed);
}
bool isPickaxe(ItemId id) {
 for(unsigned m=static_cast<unsigned>(ToolMaterial::WOOD);m<=static_cast<unsigned>(ToolMaterial::NETHERITE);++m)
  if(id==toolId(static_cast<ToolMaterial>(m),ToolKind::PICKAXE)) return true;
 return false;
}
}
unsigned achievementCount(){return ACHIEVEMENT_COUNT;}
unsigned advancementCount(){return ADVANCEMENT_COUNT;}
const ProgressEntry &achievementEntry(unsigned i){return achievements[i<ACHIEVEMENT_COUNT?i:0];}
const ProgressEntry &advancementEntry(unsigned i){return advancements[i<ADVANCEMENT_COUNT?i:0];}
bool achievementUnlocked(AchievementId id){unsigned i=static_cast<unsigned>(id);return i<ACHIEVEMENT_COUNT && (progress_state.achievement_bits&bit(i));}
bool advancementUnlocked(AdvancementId id){unsigned i=static_cast<unsigned>(id);return i<ADVANCEMENT_COUNT && (progress_state.advancement_bits&bit(i));}
bool advancementCriterionMet(AdvancementId id){unsigned i=static_cast<unsigned>(id);return i<ADVANCEMENT_COUNT && (progress_state.advancement_criteria&bit(i));}
unsigned unlockedAchievementCount(){unsigned n=0;for(unsigned i=0;i<ACHIEVEMENT_COUNT;++i)n+=(progress_state.achievement_bits&bit(i))!=0;return n;}
unsigned unlockedAdvancementCount(){unsigned n=0;for(unsigned i=0;i<ADVANCEMENT_COUNT;++i)n+=(progress_state.advancement_bits&bit(i))!=0;return n;}
void resetProgress(){progress_state=ProgressState();notice_pending=false;notice[0]='\0';}
void progressionInventoryOpened(){setAchievement(ACH_TAKING_INVENTORY);setCriterion(ADV_ROOT);evaluateAdvancements();}
void progressionItemAcquired(ItemId id,unsigned count) {
 if(!count)return;
 if(id==BLOCK_WOOD || id==BLOCK_SPRUCE_LOG || id==BLOCK_BIRCH_LOG) {
  setAchievement(ACH_GETTING_WOOD);setCriterion(ADV_GETTING_WOOD);
 } else if(id==ITEM_IRON_INGOT) {
  setAchievement(ACH_ACQUIRE_HARDWARE);setCriterion(ADV_ACQUIRE_HARDWARE);
 } else if(id==ITEM_DIAMOND) {
  setAchievement(ACH_DIAMONDS);setCriterion(ADV_DIAMONDS);
 }
 evaluateAdvancements();
}
void progressionCrafted(ItemId id,unsigned count) {
 if(!count)return;
 if(id==BLOCK_CRAFTING_TABLE) {setAchievement(ACH_BENCHMARKING);setCriterion(ADV_BENCHMARKING);}
 if(isPickaxe(id)) setAchievement(ACH_TIME_TO_MINE);
 if(id==toolId(ToolMaterial::STONE,ToolKind::PICKAXE)) setCriterion(ADV_GETTING_UPGRADE);
 if(id==BLOCK_FURNACE) {setAchievement(ACH_HOT_TOPIC);setCriterion(ADV_HOT_TOPIC);}
 if(id==BLOCK_BED) {setAchievement(ACH_SWEET_DREAMS);setCriterion(ADV_SWEET_DREAMS);}
 progressionItemAcquired(id,count);
 evaluateAdvancements();
}
void progressionBlockMined(BLOCK block){if(block==BLOCK_STONE)setCriterion(ADV_STONE_AGE);evaluateAdvancements();}
void progressionBlockPlaced(BLOCK) {
 if(progress_state.blocks_placed<100) ++progress_state.blocks_placed;
 if(progress_state.blocks_placed>=100){setAchievement(ACH_ARCHITECT);setCriterion(ADV_ARCHITECT);}
 evaluateAdvancements();
}
const char *takeProgressNotice(){if(!notice_pending)return nullptr;notice_pending=false;return notice;}
bool writeProgressSection(SaveWriter &w) {
 return w.u32(progress_state.achievement_bits)&&w.u32(progress_state.advancement_bits)
  &&w.u32(progress_state.advancement_criteria)&&w.u16(progress_state.blocks_placed);
}
bool readProgressSection(SaveReader &r) {
 ProgressState p;
 if(!r.u32(p.achievement_bits)||!r.u32(p.advancement_bits)||!r.u32(p.advancement_criteria)||!r.u16(p.blocks_placed))return false;
 if((p.achievement_bits&~validMask(ACHIEVEMENT_COUNT))||(p.advancement_bits&~validMask(ADVANCEMENT_COUNT))
   ||(p.advancement_criteria&~validMask(ADVANCEMENT_COUNT))||p.blocks_placed>100)return false;
 progress_state=p;notice_pending=false;notice[0]='\0';return true;
}
