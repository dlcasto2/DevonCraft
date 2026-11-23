#include "realistic_block_textures.h"

#include <stdint.h>

namespace {

enum Surface
{
    STONE,
    DIRT,
    GRASS_TOP,
    GRASS_SIDE,
    SNOWY_GRASS_SIDE,
    SAND,
    COBBLE,
    LOG_BARK,
    LOG_SPRUCE,
    LOG_BIRCH,
    LOG_END,
    PLANKS,
    PLANKS_DARK,
    PLANKS_LIGHT,
    BEDROCK,
    IRON_BLOCK,
    GOLD_BLOCK,
    DIAMOND_BLOCK,
    EMERALD_BLOCK,
    GOLD_ORE,
    IRON_ORE,
    COAL_ORE,
    DIAMOND_ORE,
    REDSTONE_ORE,
    EMERALD_ORE,
    OBSIDIAN,
    NETHERRACK,
    SANDSTONE_TOP,
    SANDSTONE_SIDE,
    SANDSTONE_BOTTOM,
    SNOW,
    ICE,
    CACTUS_TOP,
    CACTUS_SIDE,
    SPONGE,
    GLOWSTONE
};

static uint32_t texHash(uint32_t x, uint32_t y, uint32_t seed)
{
    uint32_t value = x * 0x8DA6B343u ^ y * 0xD8163841u ^ seed * 0xCB1AB31Fu;
    value ^= value >> 16;
    value *= 0x7FEB352Du;
    value ^= value >> 15;
    value *= 0x846CA68Bu;
    value ^= value >> 16;
    return value;
}

static int clamp8(int value)
{
    if(value < 0) return 0;
    if(value > 255) return 255;
    return value;
}

static COLOR rgb565(int red, int green, int blue)
{
    // Apply a uniform +15% brightness lift to generated block materials.
    // Integer rounding keeps the result deterministic on the ARM calculator.
    red = (red * 115 + 50) / 100;
    green = (green * 115 + 50) / 100;
    blue = (blue * 115 + 50) / 100;

    return static_cast<COLOR>(
        ((clamp8(red) & 0xF8) << 8) |
        ((clamp8(green) & 0xFC) << 3) |
        (clamp8(blue) >> 3));
}

static int variation(int x, int y, int width, int height, uint32_t seed)
{
    const int px = width > 1 ? x % (width - 1) : 0;
    const int py = height > 1 ? y % (height - 1) : 0;
    const int broad = static_cast<int>(texHash(px / 4, py / 4, seed) & 31u) - 15;
    const int fine = static_cast<int>(texHash(px, py, seed ^ 0xA511E9B3u) & 15u) - 7;
    return broad + fine;
}

static COLOR materialPixel(Surface surface, int x, int y, int width, int height, uint32_t seed)
{
    const int px = width > 1 ? x % (width - 1) : 0;
    const int py = height > 1 ? y % (height - 1) : 0;
    const uint32_t h = texHash(px, py, seed);
    const int v = variation(x, y, width, height, seed);
    int r = 120, g = 120, b = 120;

    switch(surface)
    {
    case STONE:
        r = 116 + v; g = 119 + v; b = 119 + v;
        if((h & 255u) > 246u) { r -= 24; g -= 23; b -= 21; }
        else if((h & 255u) < 7u) { r += 18; g += 18; b += 17; }
        break;

    case DIRT:
        r = 105 + v; g = 75 + v / 2; b = 48 + v / 3;
        if((h & 255u) > 238u) { r -= 28; g -= 18; b -= 10; }
        else if((h & 255u) < 12u) { r += 18; g += 12; b += 7; }
        break;

    case GRASS_TOP:
        r = 62 + v / 2; g = 111 + v; b = 42 + v / 3;
        if((h & 255u) > 229u) { r -= 13; g -= 17; b -= 8; }
        else if((h & 255u) < 18u) { r += 13; g += 20; b += 8; }
        break;

    case GRASS_SIDE:
        if(y < height / 5)
        {
            r = 61 + v / 2; g = 111 + v; b = 40 + v / 3;
            if((h & 255u) > 228u) { r -= 12; g -= 16; b -= 7; }
        }
        else
        {
            r = 103 + v; g = 74 + v / 2; b = 47 + v / 3;
            if((h & 255u) > 245u) { r -= 24; g -= 15; b -= 8; }
        }
        break;

    case SNOWY_GRASS_SIDE:
        if(y < height / 5 + ((h >> 5) & 1u))
        {
            r = 221 + v / 3; g = 229 + v / 3; b = 235 + v / 3;
        }
        else
        {
            r = 104 + v; g = 75 + v / 2; b = 48 + v / 3;
            if((h & 255u) > 244u) { r -= 25; g -= 15; b -= 8; }
        }
        break;

    case SAND:
        r = 207 + v / 2; g = 190 + v / 2; b = 143 + v / 3;
        if((h & 255u) > 239u) { r -= 22; g -= 18; b -= 12; }
        else if((h & 255u) < 18u) { r += 13; g += 12; b += 9; }
        break;

    case COBBLE:
    {
        const int local_x = (px + 3) & 7;
        const int local_y = (py + 3) & 7;
        const uint32_t stone_hash = texHash(static_cast<uint32_t>(px / 8), static_cast<uint32_t>(py / 8), seed);
        const bool mortar = local_x == 0 || local_y == 0 ||
            (local_x == 7 && (stone_hash & 1u)) ||
            (local_y == 7 && (stone_hash & 2u));
        const int stone_shade = static_cast<int>((stone_hash >> 4) & 15u) - 7;
        r = 112 + v + stone_shade; g = 114 + v + stone_shade; b = 113 + v + stone_shade;
        if(mortar) { r -= 34; g -= 34; b -= 32; }
        else if((h & 255u) > 247u) { r += 18; g += 18; b += 18; }
        break;
    }

    case LOG_BARK:
    case LOG_SPRUCE:
    case LOG_BIRCH:
    {
        const int base_r = surface == LOG_BIRCH ? 187 : (surface == LOG_SPRUCE ? 91 : 133);
        const int base_g = surface == LOG_BIRCH ? 157 : (surface == LOG_SPRUCE ? 62 : 88);
        const int base_b = surface == LOG_BIRCH ? 105 : (surface == LOG_SPRUCE ? 37 : 51);
        const int groove = (px + static_cast<int>((texHash(0, static_cast<uint32_t>(py / 5), seed) >> 3) & 3u)) % 7;
        r = base_r + v / 2; g = base_g + v / 2; b = base_b + v / 3;
        if(groove == 0) { r -= 35; g -= 23; b -= 12; }
        else if(groove == 1) { r += 13; g += 9; b += 4; }
        if(surface == LOG_BIRCH && (h & 255u) > 236u) { r -= 68; g -= 53; b -= 34; }
        break;
    }

    case LOG_END:
    {
        const int dx = px - width / 2;
        const int dy = py - height / 2;
        const int radius2 = dx * dx + dy * dy;
        const int ring = (radius2 / 13) % 5;
        r = 157 + v / 2; g = 111 + v / 2; b = 66 + v / 3;
        if(ring == 0 || ring == 3) { r -= 47; g -= 31; b -= 18; }
        if(radius2 < 13) { r += 23; g += 13; b += 5; }
        break;
    }

    case PLANKS:
    case PLANKS_DARK:
    case PLANKS_LIGHT:
    {
        const int scale = surface == PLANKS_DARK ? -25 : (surface == PLANKS_LIGHT ? 23 : 0);
        const int seam = py % 8;
        r = 151 + scale + v / 2; g = 105 + scale / 2 + v / 2; b = 63 + scale / 3 + v / 3;
        if(seam == 0) { r -= 55; g -= 39; b -= 24; }
        else if(seam == 1) { r -= 19; g -= 13; b -= 7; }
        else if((h & 31u) == 0u) { r += 22; g += 15; b += 8; }
        break;
    }

    case BEDROCK:
        r = 48 + v; g = 49 + v; b = 51 + v;
        if((h & 255u) > 219u) { r -= 20; g -= 20; b -= 19; }
        break;

    case IRON_BLOCK:
        r = 167 + v / 2; g = 171 + v / 2; b = 170 + v / 2;
        if((h & 255u) > 248u) { r += 18; g += 18; b += 19; }
        break;

    case GOLD_BLOCK:
        r = 211 + v / 2; g = 172 + v / 2; b = 48 + v / 3;
        if((h & 255u) > 240u) { r += 25; g += 19; b -= 5; }
        break;

    case DIAMOND_BLOCK:
        r = 77 + v / 2; g = 190 + v / 2; b = 194 + v / 2;
        if((h & 255u) > 238u) { r += 25; g += 25; b += 24; }
        break;

    case EMERALD_BLOCK:
        r = 39 + v / 3; g = 151 + v / 2; b = 77 + v / 3;
        if((h & 255u) > 237u) { r += 17; g += 26; b += 13; }
        break;

    case GOLD_ORE:
    case IRON_ORE:
    case COAL_ORE:
    case DIAMOND_ORE:
    case REDSTONE_ORE:
    case EMERALD_ORE:
    {
        r = 116 + v; g = 119 + v; b = 119 + v;
        const uint32_t cluster = texHash(static_cast<uint32_t>(px / 3), static_cast<uint32_t>(py / 3), seed ^ 0x63D83595u);
        if((cluster & 255u) > 220u && (h & 255u) > 44u)
        {
            if(surface == GOLD_ORE) { r = 211 + v / 3; g = 163 + v / 3; b = 48 + v / 4; }
            else if(surface == IRON_ORE) { r = 174 + v / 2; g = 143 + v / 2; b = 108 + v / 2; }
            else if(surface == COAL_ORE) { r = 42 + v / 3; g = 43 + v / 3; b = 45 + v / 3; }
            else if(surface == DIAMOND_ORE) { r = 50 + v / 3; g = 190 + v / 3; b = 208 + v / 3; }
            else if(surface == REDSTONE_ORE) { r = 170 + v / 3; g = 35 + v / 4; b = 28 + v / 4; }
            else { r = 34 + v / 3; g = 158 + v / 3; b = 73 + v / 3; }
            if((h & 63u) == 0u) { r += 22; g += 22; b += 22; }
        }
        break;
    }

    case OBSIDIAN:
        r = 27 + v / 3; g = 22 + v / 3; b = 42 + v / 2;
        if((h & 255u) > 239u) { r += 20; g += 10; b += 33; }
        break;

    case NETHERRACK:
        r = 104 + v; g = 39 + v / 2; b = 33 + v / 3;
        if((h & 255u) > 238u) { r -= 35; g -= 13; b -= 8; }
        else if((h & 255u) < 12u) { r += 22; g += 7; b += 5; }
        break;

    case SANDSTONE_TOP:
    case SANDSTONE_SIDE:
    case SANDSTONE_BOTTOM:
        r = 205 + v / 2; g = 185 + v / 2; b = 133 + v / 3;
        if(surface == SANDSTONE_SIDE)
        {
            if(py % 7 == 0) { r -= 40; g -= 35; b -= 24; }
            else if(py % 7 == 1) { r -= 16; g -= 14; b -= 9; }
        }
        if(surface == SANDSTONE_BOTTOM) { r -= 25; g -= 23; b -= 16; }
        if((h & 255u) > 244u) { r -= 17; g -= 14; b -= 10; }
        break;

    case SNOW:
        r = 224 + v / 4; g = 234 + v / 4; b = 241 + v / 4;
        if((h & 255u) > 247u) { r -= 28; g -= 21; b -= 8; }
        break;

    case ICE:
        r = 153 + v / 3; g = 202 + v / 3; b = 224 + v / 3;
        if((px % 9) < 2) { r -= 19; g -= 12; b -= 5; }
        if((h & 255u) > 246u) { r += 25; g += 25; b += 22; }
        break;

    case CACTUS_TOP:
        r = 37 + v / 3; g = 118 + v / 2; b = 45 + v / 3;
        if((px - width / 2) * (px - width / 2) + (py - height / 2) * (py - height / 2) < 20)
        { r -= 12; g -= 30; b -= 10; }
        break;

    case CACTUS_SIDE:
        r = 38 + v / 3; g = 123 + v / 2; b = 47 + v / 3;
        if(px % 7 == 0) { r -= 20; g -= 39; b -= 14; }
        else if(px % 7 == 1) { r += 12; g += 12; b += 5; }
        if((h & 255u) > 248u) { r += 28; g += 18; b += 8; }
        break;

    case SPONGE:
        r = 195 + v / 2; g = 174 + v / 2; b = 73 + v / 3;
        if((h & 255u) > 225u) { r -= 65; g -= 54; b -= 25; }
        break;

    case GLOWSTONE:
        r = 221 + v / 2; g = 174 + v / 2; b = 72 + v / 3;
        if((h & 255u) > 232u) { r += 26; g += 42; b += 39; }
        else if((h & 255u) < 12u) { r -= 50; g -= 35; b -= 15; }
        break;
    }

    return rgb565(r, g, b);
}

static void paintCell(TEXTURE &atlas, int cell_x, int cell_y, int width, int height,
                      Surface surface, uint32_t seed)
{
    for(int y = 0; y < height; ++y)
        for(int x = 0; x < width; ++x)
            atlas.bitmap[cell_x * width + x + (cell_y * height + y) * atlas.width] =
                materialPixel(surface, x, y, width, height, seed);
}

} // namespace

void paintRealisticBlockTextures(TEXTURE &terrain, int field_width, int field_height)
{
    if(!terrain.bitmap || field_width <= 0 || field_height <= 0 ||
       terrain.width < field_width * 16 || terrain.height < field_height * 16)
        return;

    // Common terrain materials.
    paintCell(terrain, 1, 0, field_width, field_height, STONE, 101);
    paintCell(terrain, 2, 0, field_width, field_height, DIRT, 102);
    paintCell(terrain, 0, 0, field_width, field_height, GRASS_TOP, 103);
    paintCell(terrain, 3, 0, field_width, field_height, GRASS_SIDE, 104);
    paintCell(terrain, 4, 0, field_width, field_height, PLANKS, 105);
    paintCell(terrain, 7, 0, field_width, field_height, COBBLE, 106);
    paintCell(terrain, 1, 1, field_width, field_height, BEDROCK, 107);
    paintCell(terrain, 2, 1, field_width, field_height, SAND, 108);
    paintCell(terrain, 3, 1, field_width, field_height, COBBLE, 109);

    // Logs: realistic bark on the sides and visible growth rings on the ends.
    paintCell(terrain, 4, 1, field_width, field_height, LOG_BARK, 110);
    paintCell(terrain, 5, 1, field_width, field_height, LOG_END, 111);
    paintCell(terrain, 4, 7, field_width, field_height, LOG_SPRUCE, 112);
    paintCell(terrain, 5, 7, field_width, field_height, LOG_BIRCH, 113);

    // Ores and solid mineral blocks.
    paintCell(terrain, 6, 1, field_width, field_height, IRON_BLOCK, 114);
    paintCell(terrain, 7, 1, field_width, field_height, GOLD_BLOCK, 115);
    paintCell(terrain, 8, 1, field_width, field_height, DIAMOND_BLOCK, 116);
    paintCell(terrain, 9, 1, field_width, field_height, EMERALD_BLOCK, 117);
    paintCell(terrain, 0, 2, field_width, field_height, GOLD_ORE, 118);
    paintCell(terrain, 1, 2, field_width, field_height, IRON_ORE, 119);
    paintCell(terrain, 2, 2, field_width, field_height, COAL_ORE, 120);
    paintCell(terrain, 5, 2, field_width, field_height, OBSIDIAN, 121);
    paintCell(terrain, 2, 3, field_width, field_height, DIAMOND_ORE, 122);
    paintCell(terrain, 3, 3, field_width, field_height, REDSTONE_ORE, 123);
    paintCell(terrain, 11, 10, field_width, field_height, EMERALD_ORE, 124);

    // Secondary terrain materials.
    paintCell(terrain, 0, 3, field_width, field_height, SPONGE, 125);
    paintCell(terrain, 2, 4, field_width, field_height, SNOW, 126);
    paintCell(terrain, 3, 4, field_width, field_height, ICE, 127);
    paintCell(terrain, 4, 4, field_width, field_height, SNOWY_GRASS_SIDE, 128);
    paintCell(terrain, 5, 4, field_width, field_height, CACTUS_TOP, 129);
    paintCell(terrain, 6, 4, field_width, field_height, CACTUS_SIDE, 130);
    paintCell(terrain, 7, 4, field_width, field_height, CACTUS_SIDE, 131);
    paintCell(terrain, 7, 6, field_width, field_height, NETHERRACK, 132);
    paintCell(terrain, 9, 6, field_width, field_height, GLOWSTONE, 133);
    paintCell(terrain, 0, 11, field_width, field_height, SANDSTONE_TOP, 134);
    paintCell(terrain, 0, 12, field_width, field_height, SANDSTONE_SIDE, 135);
    paintCell(terrain, 0, 13, field_width, field_height, SANDSTONE_BOTTOM, 136);

    // Alternate plank woods keep their original block mapping and placement.
    paintCell(terrain, 6, 12, field_width, field_height, PLANKS_DARK, 137);
    paintCell(terrain, 6, 13, field_width, field_height, PLANKS_LIGHT, 138);
}
