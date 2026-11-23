#include "advancementstask.h"
#include <cstdio>
#include "progression.h"
#include "uikit.h"
#include "worldtask.h"
AdvancementsTask advancements_task;
void AdvancementsTask::makeCurrent() {
 if(current_task!=this)return_task=current_task;
 if(!background_saved)saveBackground();
 selected=selected<advancementCount()?selected:0;
 Task::makeCurrent();
}
void AdvancementsTask::render() {
 drawBackground();darkenRect(*screen,0,0,SCREEN_WIDTH,SCREEN_HEIGHT);
 drawPanel(*screen,10,18,SCREEN_WIDTH-20,SCREEN_HEIGHT-36);
 drawPixelTextCenter(*screen,"Advancements",SCREEN_WIDTH/2,24,ui::TITLE);
 char summary[40];std::snprintf(summary,sizeof(summary),"%u / %u completed",unlockedAdvancementCount(),advancementCount());
 drawPixelTextCenter(*screen,summary,SCREEN_WIDTH/2,36,ui::TEXT_WHITE);
 const int top=49,row_height=13;
 for(unsigned i=0;i<advancementCount();++i) {
  AdvancementId id=static_cast<AdvancementId>(i);
  bool done=advancementUnlocked(id),criterion=advancementCriterionMet(id);
  char line[48];std::snprintf(line,sizeof(line),"%s %s",done?"[X]":criterion?"[~]":"[ ]",advancementEntry(i).name);
  int y=top+static_cast<int>(i)*row_height;
  if(i==selected)fillRect(*screen,23,y-1,SCREEN_WIDTH-46,row_height,0x318C);
  drawPixelText(*screen,line,27,y,done?ui::YELLOW:criterion?ui::TEXT_WHITE:ui::GREY_TEXT);
 }
 drawPixelTextCenter(*screen,advancementEntry(selected).description,SCREEN_WIDTH/2,SCREEN_HEIGHT-37,ui::TEXT_WHITE);
 drawPixelText(*screen,"[X] Done  [~] Goal met, waiting for parent",3,SCREEN_HEIGHT-23,ui::GREY_TEXT);
 drawPixelText(*screen,"8/2 select   esc/menu back",3,SCREEN_HEIGHT-11,ui::GREY_TEXT);
}
void AdvancementsTask::logic() {
 if(key_held_down) {
  key_held_down=keyPressed(KEY_NSPIRE_ESC)||keyPressed(KEY_NSPIRE_MENU)||keyPressed(KEY_NSPIRE_UP)
   ||keyPressed(KEY_NSPIRE_DOWN)||keyPressed(KEY_NSPIRE_8)||keyPressed(KEY_NSPIRE_2);return;
 }
 if(keyPressed(KEY_NSPIRE_ESC)||keyPressed(KEY_NSPIRE_MENU)) {
  if(return_task)return_task->makeCurrent();else world_task.makeCurrent();key_held_down=true;
 } else if(keyPressed(KEY_NSPIRE_UP)||keyPressed(KEY_NSPIRE_8)) {
  selected=(selected+advancementCount()-1)%advancementCount();key_held_down=true;
 } else if(keyPressed(KEY_NSPIRE_DOWN)||keyPressed(KEY_NSPIRE_2)) {
  selected=(selected+1)%advancementCount();key_held_down=true;
 }
}
