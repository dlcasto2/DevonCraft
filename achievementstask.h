#ifndef ACHIEVEMENTSTASK_H
#define ACHIEVEMENTSTASK_H
#include "task.h"
class AchievementsTask : public Task {
public:
 void makeCurrent() override;
 void render() override;
 void logic() override;
private:
 Task *return_task = nullptr;
 unsigned int selected = 0;
};
extern AchievementsTask achievements_task;
#endif
