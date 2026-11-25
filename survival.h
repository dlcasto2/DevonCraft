#ifndef SURVIVAL_H
#define SURVIVAL_H

#include <cstdint>

#include "gamestate.h"
#include "items.h"

// Survival values are stored in small integer units so the same rules can be
// used on the calculator and in host-side tests.
constexpr uint8_t PLAYER_MAX_HEALTH = 20;
constexpr uint8_t PLAYER_MAX_HUNGER = 20;
constexpr uint16_t PLAYER_MAX_SATURATION_X100 = 2000;
constexpr uint16_t EXHAUSTION_THRESHOLD_X1000 = 4000;

struct SurvivalTickOutcome
{
    bool healed = false;
    bool starved = false;
};

bool isFoodItem(ItemId id);
bool eatFood(PlayerState &player, ItemStack &held);

// Adds exhaustion after applying the difficulty's hunger drain percentage.
// Four exhaustion points consume one saturation point, then one hunger point.
void addExhaustion(PlayerState &player, Difficulty difficulty, uint32_t amount_x1000);

// One simulation tick of hunger, saturation, regeneration, starvation and
// Peaceful refill. The caller supplies the timer pulses so this function is
// deterministic and straightforward to test.
SurvivalTickOutcome tickSurvivalNeeds(PlayerState &player, Difficulty difficulty,
                                      uint32_t exhaustion_delta_x1000,
                                      bool regeneration_pulse,
                                      bool starvation_pulse,
                                      bool peaceful_refill_pulse);

// Returns the number of health points actually removed.
int damagePlayer(PlayerState &player, int damage);

#endif // SURVIVAL_H
