#ifndef ADVANCEMENTSTASK_H
#define ADVANCEMENTSTASK_H
#include "task.h"
class AdvancementsTask : public Task {
public:
 void makeCurrent() override;
 void render() override;
 void logic() override;
private:
 Task *return_task = nullptr;
 unsigned int selected = 0;
};
extern AdvancementsTask advancements_task;
#endif
