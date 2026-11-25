#ifndef MOB_RENDERER_H
#define MOB_RENDERER_H

#include "entity.h"

// Draws the textured low-poly model for a passive mob.
void renderMob(const Entity &mob);

// Draws a cyan wireframe around the mob currently targeted by the crosshair.
void renderMobHitbox(const Entity &mob);

#endif // MOB_RENDERER_H
