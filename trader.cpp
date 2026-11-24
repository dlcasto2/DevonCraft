#include "trader.h"

#include "mobs.h"
#include "playerinventory.h"

namespace {

const TradeOffer offers[TRADER_OFFER_COUNT] = {
    { ItemStack(ITEM_EMERALD, 1), ItemStack(BLOCK_SAPLING, 2, 0) },
    { ItemStack(ITEM_EMERALD, 1), ItemStack(BLOCK_FLOWER, 3, 1) },
    { ItemStack(ITEM_EMERALD, 2), ItemStack(BLOCK_CACTUS, 4, 0) },
    { ItemStack(ITEM_EMERALD, 3), ItemStack(ITEM_WATER_BUCKET, 1) }
};

} // namespace

const TradeOffer *traderOffer(unsigned index)
{
    return index < TRADER_OFFER_COUNT ? &offers[index] : nullptr;
}

bool traderOfferAvailable(const Entity &trader, unsigned index)
{
    return trader.type == EntityType::MOB
        && trader.mob_kind == MOB_TRADER
        && index < TRADER_OFFER_COUNT
        && trader.trade_uses[index] < TRADER_MAX_USES;
}

bool executeTraderTrade(PlayerInventory &inventory, Entity &trader, unsigned index)
{
    if(!traderOfferAvailable(trader, index))
        return false;

    const TradeOffer &offer = offers[index];
    PlayerInventory updated = inventory;
    if(updated.remove(offer.cost.id, offer.cost.meta, offer.cost.count) != offer.cost.count)
        return false;
    if(updated.add(offer.result) != 0)
        return false;

    inventory = updated;
    ++trader.trade_uses[index];
    return true;
}
