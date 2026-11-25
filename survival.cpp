#include "survival.h"

#include <algorithm>

namespace {

uint8_t clampHunger(int value)
{
    return static_cast<uint8_t>(std::max(0, std::min(PLAYER_MAX_HUNGER, value)));
}

uint8_t clampHealth(int value)
{
    return static_cast<uint8_t>(std::max(0, std::min(PLAYER_MAX_HEALTH, value)));
}

void clampSaturation(PlayerState &player)
{
    const uint16_t hunger_cap = static_cast<uint16_t>(player.hunger) * 100u;
    if(player.saturation_x100 > hunger_cap)
        player.saturation_x100 = hunger_cap;
}

} // namespace

bool isFoodItem(ItemId id)
{
    return itemDef(id).food_hunger > 0;
}

bool eatFood(PlayerState &player, ItemStack &held)
{
    if(player.mode != GameMode::SURVIVAL || held.empty() || player.hunger >= PLAYER_MAX_HUNGER)
        return false;

    const ItemDef &food = itemDef(held.id);
    if(food.food_hunger == 0)
        return false;

    player.hunger = clampHunger(player.hunger + food.food_hunger);
    const uint32_t saturation_gain = static_cast<uint32_t>(food.food_saturation_x10) * 10u;
    const uint32_t saturation_cap = static_cast<uint32_t>(player.hunger) * 100u;
    player.saturation_x100 = static_cast<uint16_t>(
        std::min(saturation_cap, static_cast<uint32_t>(player.saturation_x100) + saturation_gain));

    --held.count;
    if(held.count == 0)
        held = ItemStack();
    return true;
}

void addExhaustion(PlayerState &player, Difficulty difficulty, uint32_t amount_x1000)
{
    if(player.mode != GameMode::SURVIVAL || amount_x1000 == 0)
        return;

    const DifficultyRules &rules = difficultyRules(difficulty);
    if(rules.hunger_drain_percent == 0)
        return;

    const uint32_t scaled = amount_x1000 * rules.hunger_drain_percent / 100u;
    const uint32_t sum = static_cast<uint32_t>(player.exhaustion_x1000) + scaled;
    player.exhaustion_x1000 = static_cast<uint16_t>(std::min<uint32_t>(65535u, sum));

    while(player.exhaustion_x1000 >= EXHAUSTION_THRESHOLD_X1000)
    {
        player.exhaustion_x1000 = static_cast<uint16_t>(player.exhaustion_x1000 - EXHAUSTION_THRESHOLD_X1000);
        if(player.saturation_x100 > 0)
            player.saturation_x100 = player.saturation_x100 > 100
                                   ? static_cast<uint16_t>(player.saturation_x100 - 100)
                                   : 0;
        else if(player.hunger > 0)
            --player.hunger;
        clampSaturation(player);
    }
}

SurvivalTickOutcome tickSurvivalNeeds(PlayerState &player, Difficulty difficulty,
                                      uint32_t exhaustion_delta_x1000,
                                      bool regeneration_pulse,
                                      bool starvation_pulse,
                                      bool peaceful_refill_pulse)
{
    SurvivalTickOutcome result;
    if(player.mode != GameMode::SURVIVAL)
        return result;

    const DifficultyRules &rules = difficultyRules(difficulty);
    if(rules.auto_refill)
    {
        if(peaceful_refill_pulse)
        {
            if(player.health < PLAYER_MAX_HEALTH)
                ++player.health;
            if(player.hunger < PLAYER_MAX_HUNGER)
                ++player.hunger;
            player.saturation_x100 = static_cast<uint16_t>(
                std::min<unsigned>(PLAYER_MAX_SATURATION_X100, player.saturation_x100 + 100u));
        }
        player.exhaustion_x1000 = 0;
        return result;
    }

    addExhaustion(player, difficulty, exhaustion_delta_x1000);

    if(regeneration_pulse && player.health < PLAYER_MAX_HEALTH && player.hunger >= 18)
    {
        ++player.health;
        addExhaustion(player, difficulty, 6000); // 6 exhaustion per healed health point
        result.healed = true;
    }

    if(starvation_pulse && rules.can_starve && player.hunger == 0
       && player.health > rules.starvation_floor)
    {
        --player.health;
        result.starved = true;
    }

    clampSaturation(player);
    return result;
}

int damagePlayer(PlayerState &player, int damage)
{
    if(damage <= 0 || player.health == 0)
        return 0;
    const int actual = std::min<int>(player.health, damage);
    player.health = clampHealth(player.health - actual);
    return actual;
}
