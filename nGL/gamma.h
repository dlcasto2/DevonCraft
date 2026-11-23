#ifndef NGL_GAMMA_H
#define NGL_GAMMA_H

#include <stdint.h>
#include <math.h>

// RGB565 gamma lookup. A percentage of 100 is neutral; higher values brighten
// midtones and lower values darken them. The table makes frame-time work three
// small lookups and bit packing, rather than floating-point exponentiation.
class GammaTable
{
public:
    GammaTable() : percent(100)
    {
        for(unsigned int i = 0; i < 32; ++i)
        {
            red[i] = static_cast<uint8_t>(i);
            blue[i] = static_cast<uint8_t>(i);
        }
        for(unsigned int i = 0; i < 64; ++i)
            green[i] = static_cast<uint8_t>(i);
    }

    void set(unsigned int value)
    {
        if(value < 50) value = 50;
        if(value > 200) value = 200;
        if(value == percent)
            return;

        percent = value;
        for(unsigned int i = 0; i < 32; ++i)
        {
            red[i] = corrected(i, 31, value);
            blue[i] = red[i];
        }
        for(unsigned int i = 0; i < 64; ++i)
            green[i] = corrected(i, 63, value);
    }

    uint16_t apply(uint16_t color) const
    {
        const unsigned int r = (color >> 11) & 31;
        const unsigned int g = (color >> 5) & 63;
        const unsigned int b = color & 31;
        return static_cast<uint16_t>((red[r] << 11) | (green[g] << 5) | blue[b]);
    }

    unsigned int getPercent() const { return percent; }

private:
    static uint8_t corrected(unsigned int value, unsigned int maximum, unsigned int gamma_percent)
    {
        if(value == 0 || value == maximum || gamma_percent == 100)
            return static_cast<uint8_t>(value);

        const float normalized = static_cast<float>(value) / static_cast<float>(maximum);
        const float exponent = 100.0f / static_cast<float>(gamma_percent);
        int output = static_cast<int>(powf(normalized, exponent) * maximum + 0.5f);
        if(output < 0) output = 0;
        if(output > static_cast<int>(maximum)) output = maximum;
        return static_cast<uint8_t>(output);
    }

    uint8_t red[32];
    uint8_t green[64];
    uint8_t blue[32];
    unsigned int percent;
};

#endif // NGL_GAMMA_H
