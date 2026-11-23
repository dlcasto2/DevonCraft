#include "abyssalgen.h"

#include <cmath>
#include <algorithm>

#include "water.h"

namespace {

const int GRAD3[36] = {
    1,1,0, -1,1,0, 1,-1,0, -1,-1,0,
    1,0,1, -1,0,1, 1,0,-1, -1,0,-1,
    0,1,1, 0,-1,1, 0,1,-1, 0,-1,-1
};

const GLFix F2(0.3660254038f), G2(0.2113248654f);
const GLFix F3(0.3333333333f), G3(0.1666666667f);

uint32_t mulberryNext(uint32_t &state)
{
    state += 0x6D2B79F5u;
    uint32_t t = state;
    t = (t ^ (t >> 15)) * (1u | t);
    t = (t + ((t ^ (t >> 7)) * (61u | t))) ^ t;
    return t ^ (t >> 14);
}

int jsRound(float value)
{
    // JavaScript Math.round: ties go toward positive infinity.
    return static_cast<int>(std::floor(value + 0.5f));
}

GLFix absFix(GLFix v) { return v < GLFix(0) ? -v : v; }
GLFix minFix(GLFix a, GLFix b) { return a < b ? a : b; }
GLFix maxFix(GLFix a, GLFix b) { return a > b ? a : b; }
GLFix clampFix(GLFix v, GLFix lo, GLFix hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

GLFix lerpFix(GLFix a, GLFix b, GLFix t)
{
    return a + (b - a) * t;
}

GLFix smooth(GLFix edge0, GLFix edge1, GLFix x)
{
    // GLFix division left-shifts its 32-bit numerator by 8 bits. For large
    // distances that overflows even though the final smoothstep is simply 0.
    // Calculate this one bounded interpolation ratio in float instead.
    const float denominator = static_cast<float>(edge1) - static_cast<float>(edge0);
    if(denominator == 0.0f)
        return x < edge0 ? GLFix(0) : GLFix(1);

    float t = (static_cast<float>(x) - static_cast<float>(edge0)) / denominator;
    if(t < 0.0f) t = 0.0f;
    if(t > 1.0f) t = 1.0f;
    return GLFix(t * t * (3.0f - 2.0f * t));
}

GLFix length2D(GLFix x, GLFix z)
{
    // Convert before squaring: GLFix stores 32-bit fixed-point values, so
    // squaring world-space offsets directly can overflow far from the origin.
    const float fx = static_cast<float>(x);
    const float fz = static_cast<float>(z);
    return GLFix(std::sqrt(fx * fx + fz * fz));
}

GLFix expNegative(float x)
{
    if(x <= 0.0f)
        return GLFix(1);
    if(x >= 32.0f)
        return GLFix(0);
    return GLFix(static_cast<float>(std::exp(-x)));
}

GLFix anchorInfluence(int x, int z, const AbyssalNoise::Anchor &anchor)
{
    const float dx = static_cast<float>(x - anchor.x) / static_cast<float>(anchor.radius);
    const float dz = static_cast<float>(z - anchor.z) / static_cast<float>(anchor.radius);
    const float q = dx * dx + dz * dz;
    return GLFix(0.55f) * expNegative(q);
}

GLFix fbm2(const AbyssalNoise &noise, unsigned int field, GLFix x, GLFix z, int octaves)
{
    GLFix amplitude(1), frequency(1), sum(0), total(0);
    for(int i = 0; i < octaves; ++i)
    {
        sum += noise.noise2(field, x * frequency, z * frequency) * amplitude;
        total += amplitude;
        amplitude *= GLFix(0.5f);
        frequency *= GLFix(2.03f);
    }
    return total == GLFix(0) ? GLFix(0) : sum / total;
}

} // namespace

void AbyssalNoise::NoiseField::setSeed(uint32_t seed)
{
    uint8_t p[256];
    for(unsigned int i = 0; i < 256; ++i)
        p[i] = static_cast<uint8_t>(i);

    // This is the same seeded Mulberry32 + Fisher-Yates permutation used by
    // the JavaScript Simplex implementation in Abyssal Transcendence.
    uint32_t state = seed;
    for(int i = 255; i > 0; --i)
    {
        const uint32_t random = mulberryNext(state);
        const unsigned int j = static_cast<unsigned int>(
            (static_cast<uint64_t>(random) * static_cast<uint32_t>(i + 1)) >> 32);
        const uint8_t tmp = p[i];
        p[i] = p[j];
        p[j] = tmp;
    }

    for(unsigned int i = 0; i < 512; ++i)
    {
        perm[i] = p[i & 255];
        grad[i] = static_cast<uint8_t>((perm[i] % 12) * 3);
    }
}

GLFix AbyssalNoise::NoiseField::noise2(GLFix xin, GLFix yin) const
{
    GLFix n0(0), n1(0), n2(0);
    const GLFix s = (xin + yin) * F2;
    const int i = (xin + s).floor();
    const int j = (yin + s).floor();
    const GLFix t = GLFix(i + j) * G2;
    const GLFix x0 = xin - (GLFix(i) - t);
    const GLFix y0 = yin - (GLFix(j) - t);

    const int i1 = x0 > y0 ? 1 : 0;
    const int j1 = x0 > y0 ? 0 : 1;
    const GLFix x1 = x0 - i1 + G2;
    const GLFix y1 = y0 - j1 + G2;
    const GLFix x2 = x0 - 1 + G2 * 2;
    const GLFix y2 = y0 - 1 + G2 * 2;
    const int ii = i & 255, jj = j & 255;

    GLFix t0 = GLFix(0.5f) - x0 * x0 - y0 * y0;
    if(t0 >= GLFix(0))
    {
        const int g = grad[ii + perm[jj]];
        t0 *= t0;
        n0 = t0 * t0 * (GLFix(GRAD3[g]) * x0 + GLFix(GRAD3[g + 1]) * y0);
    }

    GLFix t1 = GLFix(0.5f) - x1 * x1 - y1 * y1;
    if(t1 >= GLFix(0))
    {
        const int g = grad[ii + i1 + perm[jj + j1]];
        t1 *= t1;
        n1 = t1 * t1 * (GLFix(GRAD3[g]) * x1 + GLFix(GRAD3[g + 1]) * y1);
    }

    GLFix t2 = GLFix(0.5f) - x2 * x2 - y2 * y2;
    if(t2 >= GLFix(0))
    {
        const int g = grad[ii + 1 + perm[jj + 1]];
        t2 *= t2;
        n2 = t2 * t2 * (GLFix(GRAD3[g]) * x2 + GLFix(GRAD3[g + 1]) * y2);
    }

    return GLFix(70) * (n0 + n1 + n2);
}

GLFix AbyssalNoise::NoiseField::noise3(GLFix xin, GLFix yin, GLFix zin) const
{
    GLFix n0(0), n1(0), n2(0), n3(0);
    const GLFix s = (xin + yin + zin) * F3;
    const int i = (xin + s).floor();
    const int j = (yin + s).floor();
    const int k = (zin + s).floor();
    const GLFix t = GLFix(i + j + k) * G3;
    const GLFix x0 = xin - (GLFix(i) - t);
    const GLFix y0 = yin - (GLFix(j) - t);
    const GLFix z0 = zin - (GLFix(k) - t);

    int i1, j1, k1, i2, j2, k2;
    if(x0 >= y0)
    {
        if(y0 >= z0) { i1=1; j1=0; k1=0; i2=1; j2=1; k2=0; }
        else if(x0 >= z0) { i1=1; j1=0; k1=0; i2=1; j2=0; k2=1; }
        else { i1=0; j1=0; k1=1; i2=1; j2=0; k2=1; }
    }
    else
    {
        if(y0 < z0) { i1=0; j1=0; k1=1; i2=0; j2=1; k2=1; }
        else if(x0 < z0) { i1=0; j1=1; k1=0; i2=0; j2=1; k2=1; }
        else { i1=0; j1=1; k1=0; i2=1; j2=1; k2=0; }
    }

    const GLFix x1 = x0 - i1 + G3, y1 = y0 - j1 + G3, z1 = z0 - k1 + G3;
    const GLFix x2 = x0 - i2 + G3 * 2, y2 = y0 - j2 + G3 * 2, z2 = z0 - k2 + G3 * 2;
    const GLFix x3 = x0 - 1 + G3 * 3, y3 = y0 - 1 + G3 * 3, z3 = z0 - 1 + G3 * 3;
    const int ii = i & 255, jj = j & 255, kk = k & 255;

    GLFix t0 = GLFix(0.6f) - x0*x0 - y0*y0 - z0*z0;
    if(t0 >= GLFix(0))
    {
        const int g = grad[ii + perm[jj + perm[kk]]];
        t0 *= t0;
        n0 = t0*t0*(GLFix(GRAD3[g])*x0 + GLFix(GRAD3[g+1])*y0 + GLFix(GRAD3[g+2])*z0);
    }

    GLFix t1 = GLFix(0.6f) - x1*x1 - y1*y1 - z1*z1;
    if(t1 >= GLFix(0))
    {
        const int g = grad[ii + i1 + perm[jj + j1 + perm[kk + k1]]];
        t1 *= t1;
        n1 = t1*t1*(GLFix(GRAD3[g])*x1 + GLFix(GRAD3[g+1])*y1 + GLFix(GRAD3[g+2])*z1);
    }

    GLFix t2 = GLFix(0.6f) - x2*x2 - y2*y2 - z2*z2;
    if(t2 >= GLFix(0))
    {
        const int g = grad[ii + i2 + perm[jj + j2 + perm[kk + k2]]];
        t2 *= t2;
        n2 = t2*t2*(GLFix(GRAD3[g])*x2 + GLFix(GRAD3[g+1])*y2 + GLFix(GRAD3[g+2])*z2);
    }

    GLFix t3 = GLFix(0.6f) - x3*x3 - y3*y3 - z3*z3;
    if(t3 >= GLFix(0))
    {
        const int g = grad[ii + 1 + perm[jj + 1 + perm[kk + 1]]];
        t3 *= t3;
        n3 = t3*t3*(GLFix(GRAD3[g])*x3 + GLFix(GRAD3[g+1])*y3 + GLFix(GRAD3[g+2])*z3);
    }

    return GLFix(32) * (n0 + n1 + n2 + n3);
}

AbyssalNoise::AbyssalNoise(unsigned int seed)
{
    setSeed(seed);
}

void AbyssalNoise::setSeed(unsigned int seed)
{
    nA.setSeed(seed ^ 0x1111u);
    nB.setSeed(seed ^ 0x2222u);
    nC.setSeed(seed ^ 0x3333u);
    nD.setSeed(seed ^ 0x4444u);
    nE.setSeed(seed ^ 0x5555u);
    nF.setSeed(seed ^ 0x6666u);

    // Origin-world landmark placement uses the source world's Mulberry32 stream.
    uint32_t state = seed ^ 0x51f15eu;
    const float angle = static_cast<float>(
        static_cast<double>(mulberryNext(state)) / 4294967296.0 * 6.283185307179586);
    abyss_anchor = Anchor{jsRound(std::cos(angle) * 170.0f),
                          jsRound(std::sin(angle) * 170.0f), 30};
    dead_anchor = Anchor{jsRound(std::cos(angle + 2.4f) * 260.0f),
                         jsRound(std::sin(angle + 2.4f) * 260.0f), 120};
    lake_anchors[0] = Anchor{jsRound(std::cos(angle + 1.1f) * 46.0f),
                             jsRound(std::sin(angle + 1.1f) * 46.0f), 15};
    lake_anchors[1] = Anchor{jsRound(std::cos(angle - 1.4f) * 95.0f),
                             jsRound(std::sin(angle - 1.4f) * 95.0f), 22};
}

const AbyssalNoise::NoiseField &AbyssalNoise::field(unsigned int index) const
{
    switch(index % 6)
    {
    case 0: return nA;
    case 1: return nB;
    case 2: return nC;
    case 3: return nD;
    case 4: return nE;
    default: return nF;
    }
}

GLFix AbyssalNoise::noise2(unsigned int index, GLFix x, GLFix z) const
{
    return field(index).noise2(x, z);
}

GLFix AbyssalNoise::noise3(unsigned int index, GLFix x, GLFix y, GLFix z) const
{
    return field(index).noise3(x, y, z);
}

AbyssalColumn abyssalColumnAt(const AbyssalNoise &noise, int x, int z)
{
    const GLFix X(x), Z(z);
    const GLFix sea(SEA_LEVEL);

    GLFix continent = fbm2(noise, 0, X / 700, Z / 700, 3) + GLFix(0.05f);
    const AbyssalNoise::Anchor origin_anchor{0, 0, 120};
    const AbyssalNoise::Anchor abyss_anchor{noise.abyss().x, noise.abyss().z,
        static_cast<int>(noise.abyss().radius * 2.2f)};
    continent += anchorInfluence(x, z, origin_anchor);
    continent += anchorInfluence(x, z, abyss_anchor);

    const GLFix hills = fbm2(noise, 1, X / 170, Z / 170, 3);
    const GLFix detail = noise.noise2(1, X / 26, Z / 26) * GLFix(0.25f);

    GLFix ridge = GLFix(1) - absFix(fbm2(noise, 2, X / 300, Z / 300, 2));
    ridge = ridge * ridge * ridge;
    const GLFix mount = smooth(GLFix(0.55f), GLFix(0.85f),
        noise.noise2(3, X / 900 + GLFix(11.3f), Z / 900 - GLFix(7.1f)) * GLFix(0.5f) + GLFix(0.5f));

    GLFix height = sea
        + continent * GLFix(14.1667f)
        + hills * GLFix(2.9167f)
        + detail
        + ridge * mount * GLFix(17.5f);

    GLFix moisture = noise.noise2(4, X / 330 + GLFix(3.1f), Z / 330 + GLFix(9.7f)) * GLFix(0.5f) + GLFix(0.5f);
    moisture += noise.noise2(4, X / 90, Z / 90) * GLFix(0.08f);
    const float mx = static_cast<float>(x) / 90.0f;
    const float mz = static_cast<float>(z) / 90.0f;
    moisture += GLFix(0.2f) * expNegative(mx * mx + mz * mz);

    const GLFix river = absFix(noise.noise2(3, X / 430 + 50, Z / 430 - 20));
    if(river < GLFix(0.055f) && height > sea - GLFix(0.4167f))
        height = lerpFix(sea - GLFix(1.4583f), height, smooth(GLFix(0), GLFix(0.055f), river));

    const GLFix lake_noise = noise.noise2(5, X / 140 + GLFix(7.7f), Z / 140 - GLFix(3.3f));
    if(lake_noise > GLFix(0.55f) && height > sea - GLFix(1.25f))
    {
        const GLFix lake_floor = minFix(height, sea - GLFix(1.0417f) - (lake_noise - GLFix(0.55f)) * GLFix(5.8333f));
        height = lerpFix(height, lake_floor, smooth(GLFix(0.55f), GLFix(0.66f), lake_noise));
    }

    for(unsigned int i = 0; i < 2; ++i)
    {
        const AbyssalNoise::Anchor &lake = noise.lake(i);
        const GLFix dx = X - lake.x, dz = Z - lake.z;
        const GLFix distance = length2D(dx, dz) + noise.noise2(1, X / 18, Z / 18) * 5;
        if(distance < GLFix(lake.radius + 7))
        {
            const GLFix lake_floor = sea - GLFix(0.625f) - smooth(GLFix(lake.radius), GLFix(0), distance) * GLFix(2.5f);
            height = lerpFix(height, lake_floor, smooth(GLFix(lake.radius + 7), GLFix(lake.radius - 3), distance));
        }
    }

    const AbyssalNoise::Anchor &dead_anchor = noise.deadZone();
    const GLFix dead_dx = X - dead_anchor.x, dead_dz = Z - dead_anchor.z;
    const GLFix dead_distance = length2D(dead_dx, dead_dz) + noise.noise2(0, X / 70, Z / 70) * 30;
    const GLFix dead = smooth(GLFix(dead_anchor.radius), GLFix(dead_anchor.radius) * GLFix(0.6f), dead_distance);
    if(dead > GLFix(0))
    {
        height = lerpFix(height, sea + GLFix(2.0833f) + hills * GLFix(2.0833f), dead * GLFix(0.5f));
        const GLFix crater = absFix(noise.noise2(2, X / 48, Z / 48));
        if(crater < GLFix(0.04f))
            height -= (GLFix(1) - crater / GLFix(0.04f)) * GLFix(3.3333f) * dead;
    }

    const AbyssalNoise::Anchor &abyss_anchor_ref = noise.abyss();
    const GLFix abyss_dx = X - abyss_anchor_ref.x, abyss_dz = Z - abyss_anchor_ref.z;
    const GLFix abyss_distance = length2D(abyss_dx, abyss_dz)
        - GLFix(abyss_anchor_ref.radius) * (GLFix(1) + noise.noise2(1, X / 22, Z / 22) * GLFix(0.18f));
    if(abyss_distance < GLFix(16))
        height += (GLFix(16) - maxFix(abyss_distance, GLFix(0))) * GLFix(0.1458333f);

    Biome biome;
    if(abyss_distance < GLFix(0))
        biome = Biome::ABYSSAL_ABYSS;
    else if(abyss_distance < GLFix(9))
        biome = Biome::ABYSSAL_RIM;
    else if(dead > GLFix(0.5f))
        biome = Biome::ABYSSAL_DEAD;
    else if(height < sea - GLFix(0.5f))
        biome = Biome::ABYSSAL_OCEAN;
    else if(height < sea + GLFix(0.75f))
        biome = Biome::ABYSSAL_BEACH;
    else if(height > sea + GLFix(16.6667f))
        biome = Biome::ABYSSAL_SNOW;
    else if(ridge * mount > GLFix(0.3f) && height > sea + GLFix(6.6667f))
        biome = Biome::ABYSSAL_HIGHLAND;
    else if(moisture > GLFix(0.6f))
        biome = Biome::ABYSSAL_DEEPWOOD;
    else if(moisture > GLFix(0.43f))
        biome = Biome::ABYSSAL_WILDWOOD;
    else
        biome = Biome::ABYSSAL_MEADOW;

    GLFix overhang(0.10f);
    if(biome == Biome::ABYSSAL_HIGHLAND || biome == Biome::ABYSSAL_CRYSTAL)
        overhang = GLFix(0.25f);
    else if(biome == Biome::ABYSSAL_DEEPWOOD || biome == Biome::ABYSSAL_WILDWOOD
        || biome == Biome::ABYSSAL_MEADOW || biome == Biome::ABYSSAL_RUINS
        || biome == Biome::ABYSSAL_BEACH)
        overhang = GLFix(0.03f);
    if(abyss_distance < GLFix(20))
        overhang = GLFix(0);

    const int ground = std::max(3, std::min(36, static_cast<int>(height.round())));
    return AbyssalColumn{ground, biome, dead, abyss_distance, overhang, moisture};
}

bool abyssalCaveAt(const AbyssalNoise &noise, int x, int y, int z)
{
    const GLFix X(x), Y(y), Z(z);
    const GLFix a = noise.noise3(2, X / 40, Y / GLFix(9.1667f), Z / 40);
    const GLFix b = noise.noise3(3, X / 40 + 100, Y / GLFix(9.1667f), Z / 40);
    const GLFix ceiling = noise.noise3(4, X / 70, Y / GLFix(14.5833f), Z / 70);
    return a * a + b * b < GLFix(0.012f) || ceiling > GLFix(0.62f);
}

bool abyssalVoidPocketAt(const AbyssalNoise &noise, int x, int y, int z)
{
    return noise.noise3(4, GLFix(x) / 16, GLFix(y) / GLFix(4.5833f), GLFix(z) / 16) > GLFix(0.5f);
}
