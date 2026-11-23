#ifndef BOOTUI_H
#define BOOTUI_H

#include "gl.h"

// Draws the branded 320x240 startup screen. Progress is clamped to 0-100.
// The monochrome flag selects a grayscale-safe palette for the original CAS LCD.
void drawBootUi(TEXTURE &dst, const char *status, unsigned int progress, bool monochrome);

#endif // BOOTUI_H
