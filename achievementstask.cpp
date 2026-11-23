#include "achievementstask.h"
#include <cstdio>
#include "progression.h"
#include "uikit.h"
#include "worldtask.h"
AchievementsTask achievements_task;
void AchievementsTask::makeCurrent() {
 if(current_task!=this) return_task=current_task;
 if(!background_saved) saveBackground();
 selected=selected<achievementCount()?selected:0;
 Task::makeCurrent();
}
void AchievementsTask::render() {
 drawBackground();darkenRect(*screen,0,0,SCREEN_WIDTH,SCREEN_HEIGHT);
 drawPanel(*screen,10,18,SCREEN_WIDTH-20,SCREEN_HEIGHT-36);
 drawPixelTextCenter(*screen,"Achievements",SCREEN_WIDTH/2,24,ui::TITLE);
 char summary[40];std::snprintf(summary,sizeof(summary),"%u / %u unlocked",unlockedAchievementCount(),achievementCount());
 drawPixelTextCenter(*screen,summary,SCREEN_WIDTH/2,36,ui::TEXT_WHITE);
 const int top=49,row_height=14;
 for(unsigned i=0;i<achievementCount();++i) {
  bool unlocked=achievementUnlocked(static_cast<AchievementId>(i));
  char line[48];std::snprintf(line,sizeof(line),"%s %s",unlocked?"[X]":"[ ]",achievementEntry(i).name);
  int y=top+static_cast<int>(i)*row_height;
  if(i==selected)fillRect(*screen,23,y-1,SCREEN_WIDTH-46,row_height,0x318C);
  drawPixelText(*screen,line,27,y,unlocked?ui::YELLOW:ui::GREY_TEXT);
 }
 drawPixelTextCenter(*screen,achievementEntry(selected).description,SCREEN_WIDTH/2,SCREEN_HEIGHT-37,ui::TEXT_WHITE);
 drawPixelText(*screen,"8/2 select   esc/menu back",3,SCREEN_HEIGHT-11,ui::GREY_TEXT);
}
void AchievementsTask::logic() {
 if(key_held_down) {
  key_held_down=keyPressed(KEY_NSPIRE_ESC)||keyPressed(KEY_NSPIRE_MENU)||keyPressed(KEY_NSPIRE_UP)
   ||keyPressed(KEY_NSPIRE_DOWN)||keyPressed(KEY_NSPIRE_8)||keyPressed(KEY_NSPIRE_2);return;
 }
 if(keyPressed(KEY_NSPIRE_ESC)||keyPressed(KEY_NSPIRE_MENU)) {
  if(return_task)return_task->makeCurrent();else world_task.makeCurrent();key_held_down=true;
 } else if(keyPressed(KEY_NSPIRE_UP)||keyPressed(KEY_NSPIRE_8)) {
  selected=(selected+achievementCount()-1)%achievementCount();key_held_down=true;
 } else if(keyPressed(KEY_NSPIRE_DOWN)||keyPressed(KEY_NSPIRE_2)) {
  selected=(selected+1)%achievementCount();key_held_down=true;
 }
}
