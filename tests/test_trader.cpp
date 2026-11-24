#include "doctest.h"
#include "mobs.h"
#include "playerinventory.h"
#include "trader.h"

TEST_CASE("traders spawn as peaceful wandering mobs")
{
    EntityPool pool;
    Entity *trader = spawnTrader(pool, AABB());
    REQUIRE(trader != nullptr);
    CHECK(trader->type == EntityType::MOB);
    CHECK(trader->mob_kind == MOB_TRADER);
    CHECK(trader->health == 0);
    CHECK(countTraders(pool) == 1);
    CHECK(countPassive(pool) == 1);
    CHECK_FALSE(hurtMob(*trader, 100));
    CHECK(mobDrop(MOB_TRADER).empty());
}

TEST_CASE("traders expose four emerald-priced offers")
{
    EntityPool pool;
    Entity *trader = spawnTrader(pool, AABB());
    REQUIRE(trader != nullptr);
    CHECK(traderOffer(0) != nullptr);
    CHECK(traderOffer(3) != nullptr);
    CHECK(traderOffer(TRADER_OFFER_COUNT) == nullptr);
    for(unsigned i = 0; i < TRADER_OFFER_COUNT; ++i)
    {
        REQUIRE(traderOffer(i) != nullptr);
        CHECK(traderOffer(i)->cost.id == ITEM_EMERALD);
        CHECK(traderOfferAvailable(*trader, i));
    }
}

TEST_CASE("a successful trade charges emeralds and grants the full result")
{
    EntityPool pool;
    Entity *trader = spawnTrader(pool, AABB());
    REQUIRE(trader != nullptr);
    PlayerInventory inv;
    inv.slots[5] = ItemStack(ITEM_EMERALD, 3);

    REQUIRE(executeTraderTrade(inv, *trader, 0));
    CHECK(inv.count(ITEM_EMERALD, 0) == 2);
    CHECK(inv.count(BLOCK_SAPLING, 0) == 2);
    CHECK(trader->trade_uses[0] == 1);
}

TEST_CASE("an unaffordable trade leaves inventory and stock unchanged")
{
    EntityPool pool;
    Entity *trader = spawnTrader(pool, AABB());
    REQUIRE(trader != nullptr);
    PlayerInventory inv;

    CHECK_FALSE(executeTraderTrade(inv, *trader, 2));
    CHECK(inv.count(ITEM_EMERALD, 0) == 0);
    CHECK(inv.count(BLOCK_CACTUS, 0) == 0);
    CHECK(trader->trade_uses[2] == 0);
}

TEST_CASE("a trade does not take payment when the result cannot fit")
{
    EntityPool pool;
    Entity *trader = spawnTrader(pool, AABB());
    REQUIRE(trader != nullptr);
    PlayerInventory inv;
    for(ItemStack &slot : inv.slots)
        slot = ItemStack(ITEM_STICK, 64);
    inv.slots[0] = ItemStack(ITEM_EMERALD, 5);

    CHECK_FALSE(executeTraderTrade(inv, *trader, 0));
    CHECK(inv.count(ITEM_EMERALD, 0) == 5);
    CHECK(inv.count(BLOCK_SAPLING, 0) == 0);
    CHECK(trader->trade_uses[0] == 0);
}

TEST_CASE("trader offers sell out after eight successful trades")
{
    EntityPool pool;
    Entity *trader = spawnTrader(pool, AABB());
    REQUIRE(trader != nullptr);
    PlayerInventory inv;
    inv.slots[0] = ItemStack(ITEM_EMERALD, 64);

    for(unsigned i = 0; i < TRADER_MAX_USES; ++i)
        CHECK(executeTraderTrade(inv, *trader, 1));

    CHECK_FALSE(traderOfferAvailable(*trader, 1));
    const unsigned emeralds = inv.count(ITEM_EMERALD, 0);
    CHECK_FALSE(executeTraderTrade(inv, *trader, 1));
    CHECK(inv.count(ITEM_EMERALD, 0) == emeralds);
}
