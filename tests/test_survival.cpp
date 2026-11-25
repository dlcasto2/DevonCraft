#include "doctest.h"
#include "survival.h"

TEST_CASE("food restores hunger and saturation and consumes one item") {
    PlayerState player;
    player.mode = GameMode::SURVIVAL;
    player.hunger = 10;
    player.saturation_x100 = 0;
    ItemStack apples(ITEM_APPLE, 2);

    CHECK(isFoodItem(ITEM_APPLE));
    CHECK(eatFood(player, apples));
    CHECK(player.hunger == 14);
    CHECK(player.saturation_x100 == 240);
    CHECK(apples.id == ITEM_APPLE);
    CHECK(apples.count == 1);
}

TEST_CASE("food values are respected and saturation is capped by hunger") {
    PlayerState player;
    player.mode = GameMode::SURVIVAL;
    player.hunger = 18;
    player.saturation_x100 = 1900;
    ItemStack steak(ITEM_BEEF_COOKED, 1);

    CHECK(eatFood(player, steak));
    CHECK(player.hunger == 20);
    CHECK(player.saturation_x100 == PLAYER_MAX_SATURATION_X100);
    CHECK(steak.empty());
}

TEST_CASE("food cannot be eaten in creative or at full hunger") {
    PlayerState player;
    ItemStack bread(ITEM_BREAD, 2);

    CHECK_FALSE(eatFood(player, bread));
    CHECK(bread.count == 2);

    player.mode = GameMode::SURVIVAL;
    player.hunger = PLAYER_MAX_HUNGER;
    CHECK_FALSE(eatFood(player, bread));
    CHECK(bread.count == 2);
    CHECK_FALSE(isFoodItem(ITEM_COAL));
}

TEST_CASE("exhaustion consumes saturation before hunger") {
    PlayerState player;
    player.mode = GameMode::SURVIVAL;
    player.hunger = 10;
    player.saturation_x100 = 150;

    addExhaustion(player, Difficulty::NORMAL, 4000);
    CHECK(player.saturation_x100 == 50);
    CHECK(player.hunger == 10);

    addExhaustion(player, Difficulty::NORMAL, 4000);
    CHECK(player.saturation_x100 == 0);
    CHECK(player.hunger == 10);

    addExhaustion(player, Difficulty::NORMAL, 4000);
    CHECK(player.hunger == 9);
    CHECK(player.exhaustion_x1000 == 0);
}

TEST_CASE("difficulty scales hunger drain and peaceful disables it") {
    PlayerState torment;
    torment.mode = GameMode::SURVIVAL;
    torment.hunger = 10;
    torment.saturation_x100 = 1000;
    addExhaustion(torment, Difficulty::TORMENT, 4000);
    CHECK(torment.saturation_x100 == 900);
    CHECK(torment.exhaustion_x1000 == 2000);

    PlayerState peaceful;
    peaceful.mode = GameMode::SURVIVAL;
    peaceful.hunger = 10;
    peaceful.saturation_x100 = 500;
    addExhaustion(peaceful, Difficulty::PEACEFUL, 12000);
    CHECK(peaceful.hunger == 10);
    CHECK(peaceful.saturation_x100 == 500);
    CHECK(peaceful.exhaustion_x1000 == 0);
}

TEST_CASE("natural regeneration needs hunger and consumes exhaustion") {
    PlayerState player;
    player.mode = GameMode::SURVIVAL;
    player.health = 10;
    player.hunger = 18;
    player.saturation_x100 = 1000;

    const SurvivalTickOutcome healed =
        tickSurvivalNeeds(player, Difficulty::NORMAL, 0, true, false, false);
    CHECK(healed.healed);
    CHECK(player.health == 11);
    CHECK(player.saturation_x100 == 900);
    CHECK(player.exhaustion_x1000 == 2000);

    player.health = 10;
    player.hunger = 17;
    const SurvivalTickOutcome hungry =
        tickSurvivalNeeds(player, Difficulty::NORMAL, 0, true, false, false);
    CHECK_FALSE(hungry.healed);
    CHECK(player.health == 10);
}

TEST_CASE("starvation respects each difficulty health floor") {
    PlayerState easy;
    easy.mode = GameMode::SURVIVAL;
    easy.health = 11;
    easy.hunger = 0;
    CHECK(tickSurvivalNeeds(easy, Difficulty::EASY, 0, false, true, false).starved);
    CHECK(easy.health == 10);
    CHECK_FALSE(tickSurvivalNeeds(easy, Difficulty::EASY, 0, false, true, false).starved);
    CHECK(easy.health == 10);

    PlayerState normal;
    normal.mode = GameMode::SURVIVAL;
    normal.health = 2;
    normal.hunger = 0;
    CHECK(tickSurvivalNeeds(normal, Difficulty::NORMAL, 0, false, true, false).starved);
    CHECK(normal.health == 1);

    PlayerState hard;
    hard.mode = GameMode::SURVIVAL;
    hard.health = 1;
    hard.hunger = 0;
    CHECK(tickSurvivalNeeds(hard, Difficulty::HARD, 0, false, true, false).starved);
    CHECK(hard.health == 0);
}

TEST_CASE("peaceful gradually refills health hunger and saturation") {
    PlayerState player;
    player.mode = GameMode::SURVIVAL;
    player.health = 8;
    player.hunger = 4;
    player.saturation_x100 = 0;
    player.exhaustion_x1000 = 3000;

    tickSurvivalNeeds(player, Difficulty::PEACEFUL, 9000, false, false, false);
    CHECK(player.health == 8);
    CHECK(player.hunger == 4);

    tickSurvivalNeeds(player, Difficulty::PEACEFUL, 9000, false, false, true);
    CHECK(player.health == 9);
    CHECK(player.hunger == 5);
    CHECK(player.saturation_x100 == 100);
    CHECK(player.exhaustion_x1000 == 0);
}

TEST_CASE("creative mode has no hunger, regeneration or starvation updates") {
    PlayerState player;
    player.mode = GameMode::CREATIVE;
    player.health = 10;
    player.hunger = 0;
    const SurvivalTickOutcome outcome =
        tickSurvivalNeeds(player, Difficulty::HARD, 8000, true, true, true);
    CHECK_FALSE(outcome.healed);
    CHECK_FALSE(outcome.starved);
    CHECK(player.health == 10);
    CHECK(player.hunger == 0);
}

TEST_CASE("player damage is clamped and reports actual health lost") {
    PlayerState player;
    CHECK(damagePlayer(player, 4) == 4);
    CHECK(player.health == 16);
    CHECK(damagePlayer(player, 40) == 16);
    CHECK(player.health == 0);
    CHECK(damagePlayer(player, 1) == 0);
    CHECK(player.health == 0);
}

TEST_CASE("exhaustion cannot drain hunger below zero") {
    PlayerState player;
    player.mode = GameMode::SURVIVAL;
    player.hunger = 0;
    player.saturation_x100 = 0;
    addExhaustion(player, Difficulty::HARD, 40000);
    CHECK(player.hunger == 0);
    CHECK(player.saturation_x100 == 0);
    CHECK(player.exhaustion_x1000 < EXHAUSTION_THRESHOLD_X1000);
}

TEST_CASE("Peaceful saturation never exceeds the current hunger bar") {
    PlayerState player;
    player.mode = GameMode::SURVIVAL;
    player.health = PLAYER_MAX_HEALTH;
    player.hunger = 2;
    player.saturation_x100 = 1000;
    tickSurvivalNeeds(player, Difficulty::PEACEFUL, 0, false, false, true);
    CHECK(player.hunger == 3);
    CHECK(player.saturation_x100 == 300);
}
