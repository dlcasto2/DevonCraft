#ifndef WORLDLOADERTASK_H
#define WORLDLOADERTASK_H

#include "task.h"

enum class WorldLoaderMode {
    STARTUP,
    LOAD,
    CREATE
};

// A small save-slot browser. The first slot preserves the existing save path;
// the remaining slots are independent files in /documents/ndless.
class WorldLoaderTask : public Task
{
public:
    void setPrimaryPath(const char *path);
    void open(WorldLoaderMode mode);

    virtual void render() override;
    virtual void logic() override;

private:
    void selectSlot(unsigned int slot);
    void goBack();
    void showStatus(const char *message);

    WorldLoaderMode mode = WorldLoaderMode::STARTUP;
    unsigned int selected = 0;
    char status[64] = {};
    unsigned int status_timeout = 0;
};

extern WorldLoaderTask world_loader_task;

#endif // WORLDLOADERTASK_H
