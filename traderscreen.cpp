#include "traderscreen.h"

#include <cstdio>
#include <cstring>

#include "mobs.h"
#include "playerinventory.h"
#include "progression.h"
#include "trader.h"
#include "uikit.h"
#include "worldtask.h"

TraderScreen trader_screen;

constexpr int TraderScreen::PANEL_W, TraderScreen::PANEL_H;

void TraderScreen::open(Entity &trader)
{
    merchant = &trader;
    selection = 0;
    setStatus("Choose an offer and press 5.");
    makeCurrent();
}

void TraderScreen::makeCurrent()
{
    if(!background_saved)
        saveBackground();
    Task::makeCurrent();
}

void TraderScreen::setStatus(const char *text)
{
    std::snprintf(status, sizeof(status), "%s", text);
}

void TraderScreen::close()
{
    merchant = nullptr;
    world_task.makeCurrent();
}

void TraderScreen::render()
{
    drawBackground();
    darkenRect(*screen, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    const int px = (SCREEN_WIDTH - PANEL_W) / 2;
    const int py = (SCREEN_HEIGHT - PANEL_H) / 2 - 3;
    drawPanel(*screen, px, py, PANEL_W, PANEL_H);
    drawPixelTextCenter(*screen, "Wandering Trader", px + PANEL_W / 2, py + 6, ui::DARK_TEXT);
    drawPixelTextCenter(*screen, "Supplies for emeralds", px + PANEL_W / 2, py + 16, ui::DARK_TEXT, false);

    for(unsigned i = 0; i < TRADER_OFFER_COUNT; ++i)
    {
        const int row_y = py + 29 + static_cast<int>(i) * 25;
        const bool available = merchant && traderOfferAvailable(*merchant, i);
        fillRect(*screen, px + 7, row_y, PANEL_W - 14, 23,
                 i == selection ? 0x6B5A : (available ? 0xBDF7 : 0xA534));

        const TradeOffer *offer = traderOffer(i);
        if(!offer)
            continue;
        drawSlot(*screen, px + 11, row_y + 2);
        drawItemStack(*screen, px + 12, row_y + 3, offer->cost);
        drawPixelTextCenter(*screen, "->", px + 61, row_y + 8, ui::DARK_TEXT, false);
        drawSlot(*screen, px + 76, row_y + 2);
        drawItemStack(*screen, px + 77, row_y + 3, offer->result);

        const char *name = itemName(offer->result);
        drawPixelText(*screen, name, px + 100, row_y + 8,
                      available ? ui::DARK_TEXT : ui::GREY_TEXT, false);
        if(!available)
            drawPixelText(*screen, "SOLD OUT", px + PANEL_W - 57, row_y + 8, ui::DARK_TEXT, false);
    }

    char balance[40];
    std::snprintf(balance, sizeof(balance), "Emeralds: %u",
                  player_inventory.count(ITEM_EMERALD, 0));
    drawPixelText(*screen, balance, px + 10, py + 132, ui::DARK_TEXT, false);
    drawPixelText(*screen, status, px + 10, py + 146, ui::DARK_TEXT, false);
    drawPixelTextCenter(*screen, "8/2 choose   5 trade   esc close",
                        SCREEN_WIDTH / 2, py + PANEL_H + 4, ui::TEXT);
}

void TraderScreen::logic()
{
    if(!merchant || merchant->type != EntityType::MOB || merchant->mob_kind != MOB_TRADER)
    {
        close();
        return;
    }

    if(key_held_down)
    {
        key_held_down = keyPressed(KEY_NSPIRE_ESC) || keyPressed(KEY_NSPIRE_PERIOD)
                     || keyPressed(KEY_NSPIRE_8) || keyPressed(KEY_NSPIRE_2)
                     || keyPressed(KEY_NSPIRE_UP) || keyPressed(KEY_NSPIRE_DOWN)
                     || keyPressed(KEY_NSPIRE_5) || keyPressed(KEY_NSPIRE_CLICK);
        return;
    }

    if(keyPressed(KEY_NSPIRE_ESC) || keyPressed(KEY_NSPIRE_PERIOD))
    {
        key_held_down = true;
        close();
    }
    else if(keyPressed(KEY_NSPIRE_8) || keyPressed(KEY_NSPIRE_UP))
    {
        key_held_down = true;
        selection = (selection + TRADER_OFFER_COUNT - 1) % TRADER_OFFER_COUNT;
    }
    else if(keyPressed(KEY_NSPIRE_2) || keyPressed(KEY_NSPIRE_DOWN))
    {
        key_held_down = true;
        selection = (selection + 1) % TRADER_OFFER_COUNT;
    }
    else if(keyPressed(KEY_NSPIRE_5) || keyPressed(KEY_NSPIRE_CLICK))
    {
        key_held_down = true;
        const TradeOffer *offer = traderOffer(selection);
        if(!traderOfferAvailable(*merchant, selection))
            setStatus("That offer is sold out.");
        else if(offer && player_inventory.count(offer->cost.id, offer->cost.meta) < offer->cost.count)
            setStatus("You need more emeralds.");
        else
        {
            const PlayerInventory before = player_inventory;
            if(executeTraderTrade(player_inventory, *merchant, selection))
            {
                progressionInventoryDelta(before, player_inventory);
                setStatus("Trade complete!");
            }
            else
                setStatus("Make room in your inventory.");
        }
    }
    else
        key_held_down = false;
}
