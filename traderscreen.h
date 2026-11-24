#ifndef TRADERSCREEN_H
#define TRADERSCREEN_H

#include "task.h"
#include "entity.h"

// Four emerald-priced offers. 8/2 selects, 5 trades, Esc or . closes.
class TraderScreen : public Task
{
public:
    void open(Entity &trader);
    virtual void makeCurrent() override;
    virtual void render() override;
    virtual void logic() override;
    void close();

private:
    void setStatus(const char *text);
    Entity *merchant = nullptr;
    unsigned selection = 0;
    char status[40] = "Choose an offer and press 5.";
    static constexpr int PANEL_W = 246, PANEL_H = 180;
};

extern TraderScreen trader_screen;

#endif // TRADERSCREEN_H
