#ifndef VILLAGEGEN_H
#define VILLAGEGEN_H

#include <cstdint>

#include "biome.h"

class Chunk;

// Called once by the chunk containing the village anchor and its surface.
// Writes to neighbouring chunks through Chunk's pending-block mechanism.
void generateChunkVillages(Chunk &chunk, const PerlinNoise &noise, uint32_t seed);

#endif // VILLAGEGEN_H
