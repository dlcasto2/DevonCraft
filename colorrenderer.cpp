#include "colorrenderer.h"

void ColorBlockRenderer::geometryNormalBlock(const BLOCK_WDATA block, const int local_x, const int local_y, const int local_z, const BLOCK_SIDE side, Chunk &c)
{
    // Low graphics should reduce chunk distance and cosmetic effects, not remove
    // the terrain's block textures. NormalBlockRenderer already uses the
    // pre-rendered textured quads where possible, so this keeps that fast path.
    NormalBlockRenderer::geometryNormalBlock(block, local_x, local_y, local_z, side, c);
}
