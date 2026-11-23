#include "bootui.h"

#include <algorithm>
#include <cstdio>
#include <cstdint>

#include "gui_art.h"
#include "uikit.h"

namespace {

COLOR rgb565(unsigned int r, unsigned int g, unsigned int b)
{
    return static_cast<COLOR>(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

COLOR grayscale(COLOR c)
{
    const unsigned int r = (c >> 11) & 31;
    const unsigned int g = (c >> 5) & 63;
    const unsigned int b = c & 31;
    const unsigned int luma6 = (r * 2 * 30 + g * 59 + b * 2 * 11) / 100;
    const unsigned int luma5 = luma6 >> 1;
    return static_cast<COLOR>((luma5 << 11) | (luma6 << 5) | luma5);
}

COLOR tone(COLOR c, bool monochrome)
{
    return monochrome ? grayscale(c) : c;
}

void putPixel(TEXTURE &dst, int x, int y, COLOR c)
{
    if(x >= 0 && y >= 0 && x < dst.width && y < dst.height)
        dst.bitmap[x + y * dst.width] = c;
}

void drawLogo(TEXTURE &dst, int x, int y, bool monochrome)
{
    for(int py = 0; py < gui_title.height; ++py)
        for(int px = 0; px < gui_title.width; ++px)
        {
            const COLOR c = gui_title.bitmap[px + py * gui_title.width];
            if(c != gui_title.transparent_color)
                putPixel(dst, x + px, y + py, tone(c, monochrome));
        }
}

void drawMountain(TEXTURE &dst, int center, int peak_y, int half_width, COLOR c)
{
    for(int y = peak_y; y < dst.height; ++y)
    {
        const int half = (y - peak_y) * half_width / (dst.height - peak_y);
        fillRect(dst, center - half, y, half * 2 + 1, 1, c);
    }
}

void drawVoxel(TEXTURE &dst, int x, int y, COLOR top, COLOR side)
{
    fillRect(dst, x + 2, y, 9, 3, top);
    fillRect(dst, x, y + 3, 11, 7, side);
    fillRect(dst, x, y + 3, 2, 7, 0xFFFF);
    fillRect(dst, x + 2, y + 8, 9, 2, 0x0000);
}

} // namespace

void drawBootUi(TEXTURE &dst, const char *status, unsigned int progress, bool monochrome)
{
    progress = std::min(progress, 100u);

    const COLOR sky0 = tone(rgb565(5, 10, 28), monochrome);
    const COLOR sky1 = tone(rgb565(9, 23, 48), monochrome);
    const COLOR sky2 = tone(rgb565(13, 39, 65), monochrome);
    const COLOR accent = tone(rgb565(51, 205, 190), monochrome);
    const COLOR accentBright = tone(rgb565(120, 240, 216), monochrome);
    const COLOR pale = tone(rgb565(228, 244, 250), monochrome);
    const COLOR muted = tone(rgb565(123, 159, 177), monochrome);
    const COLOR barBack = tone(rgb565(18, 37, 57), monochrome);
    const COLOR landFar = tone(rgb565(15, 46, 58), monochrome);
    const COLOR landNear = tone(rgb565(10, 31, 39), monochrome);

    fillRect(dst, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, sky0);
    fillRect(dst, 0, 0, SCREEN_WIDTH, 74, sky1);
    fillRect(dst, 0, 74, SCREEN_WIDTH, 80, sky2);

    // Sparse, fixed stars keep the splash crisp and inexpensive on the calculator.
    static const uint8_t stars[][2] = {
        {18,18},{47,36},{82,16},{119,29},{157,14},{204,31},{239,18},{286,34},
        {302,13},{30,72},{73,61},{108,83},{211,66},{264,76},{296,91},{14,112},
        {51,101},{91,118},{228,108},{276,122},{185,91},{143,103}
    };
    for(unsigned int i = 0; i < sizeof(stars) / sizeof(stars[0]); ++i)
    {
        const int size = (i % 5 == 0) ? 2 : 1;
        fillRect(dst, stars[i][0], stars[i][1], size, size, (i % 3 == 0) ? accent : muted);
    }

    // Thin technical frame and corner markers.
    fillRect(dst, 16, 15, SCREEN_WIDTH - 32, 1, tone(rgb565(34, 91, 112), monochrome));
    fillRect(dst, 16, 15, 18, 2, accent);
    fillRect(dst, SCREEN_WIDTH - 34, 15, 18, 2, accent);

    // The real project wordmark, converted pixel-by-pixel for the original greyscale LCD.
    drawLogo(dst, (SCREEN_WIDTH - gui_title.width) / 2, 48, monochrome);
    drawPixelTextCenter(dst, "SURVIVAL EDITION", SCREEN_WIDTH / 2, 91, pale);
    drawPixelTextCenter(dst, "BUILD  /  EXPLORE  /  SURVIVE", SCREEN_WIDTH / 2, 111, accentBright);

    // Compact loading panel.
    fillRect(dst, 31, 145, SCREEN_WIDTH - 62, 59, tone(rgb565(3, 10, 22), monochrome));
    fillRect(dst, 31, 145, SCREEN_WIDTH - 62, 1, accent);
    fillRect(dst, 31, 145, 1, 59, tone(rgb565(34, 91, 112), monochrome));
    fillRect(dst, SCREEN_WIDTH - 32, 145, 1, 59, tone(rgb565(34, 91, 112), monochrome));
    fillRect(dst, 31, 203, SCREEN_WIDTH - 62, 1, tone(rgb565(34, 91, 112), monochrome));

    drawPixelTextCenter(dst, status ? status : "Starting...", SCREEN_WIDTH / 2, 160, pale);
    char percent[8];
    std::snprintf(percent, sizeof(percent), "%u%%", progress);
    drawPixelText(dst, percent, SCREEN_WIDTH - 58, 184, accentBright);

    const int bar_x = 47, bar_y = 184, bar_w = 190;
    fillRect(dst, bar_x, bar_y, bar_w, 8, barBack);
    fillRect(dst, bar_x, bar_y, bar_w, 1, muted);
    fillRect(dst, bar_x, bar_y + 7, bar_w, 1, tone(rgb565(3, 7, 15), monochrome));
    const int inner_w = bar_w - 4;
    fillRect(dst, bar_x + 2, bar_y + 2, inner_w * static_cast<int>(progress) / 100, 4, accent);

    // Low-profile voxel landscape along the footer.
    drawMountain(dst, 69, 215, 60, landFar);
    drawMountain(dst, 257, 221, 79, landFar);
    fillRect(dst, 0, 231, SCREEN_WIDTH, 9, landNear);
    fillRect(dst, 0, 231, SCREEN_WIDTH, 1, accent);
    drawVoxel(dst, 22, 224, accent, landNear);
    drawVoxel(dst, 284, 222, accent, landNear);
    drawPixelTextCenter(dst, "TI-NSPIRE EDITION", SCREEN_WIDTH / 2, 221, muted);
}
