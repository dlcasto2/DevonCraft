#ifndef TRADER_H
#define TRADER_H

#include <cstdint>

#include "entity.h"

class PlayerInventory;

constexpr unsigned TRADER_OFFER_COUNT = 4;
constexpr uint8_t TRADER_MAX_USES = 8;

struct TradeOffer
{
    ItemStack cost;
    ItemStack result;
};

// Returns nullptr for an out-of-range offer index.
const TradeOffer *traderOffer(unsigned index);
bool traderOfferAvailable(const Entity &trader, unsigned index);

// Executes a trade atomically: payment and stock are changed only if the
// complete result fits in the player's inventory.
bool executeTraderTrade(PlayerInventory &inventory, Entity &trader, unsigned index);

#endif // TRADER_H
