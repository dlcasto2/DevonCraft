#include "doctest.h"

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <unistd.h>

#include "texturetools.h"

namespace {

void writeFile(const char *path, const char *header, const uint8_t *payload, const size_t payload_size)
{
    FILE *file = fopen(path, "wb");
    REQUIRE(file != nullptr);
    REQUIRE(fwrite(header, 1, strlen(header), file) == strlen(header));
    if(payload_size)
        REQUIRE(fwrite(payload, 1, payload_size, file) == payload_size);
    REQUIRE(fclose(file) == 0);
}

} // namespace

TEST_CASE("truncated binary PPM atlases are rejected")
{
    const char *path = "/tmp/devoncraft-truncated-atlas.ppm";
    const uint8_t partial_pixels[] = { 0x80, 0x60, 0x40 };

    writeFile(path, "P6\n2 2\n255\n", partial_pixels, sizeof(partial_pixels));
    TEXTURE *loaded = loadTextureFromFile(path);

    CHECK(loaded == nullptr);
    unlink(path);
}

TEST_CASE("truncated PAM atlases are rejected")
{
    const char *path = "/tmp/devoncraft-truncated-atlas.pam";
    const uint8_t partial_pixels[] = { 0x80, 0x60, 0x40, 0xFF };

    writeFile(path, "P7\nWIDTH 2\nHEIGHT 2\nDEPTH 4\nMAXVAL 255\nTUPLTYPE RGB_ALPHA\nENDHDR\n",
              partial_pixels, sizeof(partial_pixels));
    TEXTURE *loaded = loadTextureFromFile(path);

    CHECK(loaded == nullptr);
    unlink(path);
}

TEST_CASE("complete binary PPM textures still load")
{
    const char *path = "/tmp/devoncraft-valid-atlas.ppm";
    const uint8_t pixels[] = { 0x80, 0x60, 0x40, 0x20, 0x40, 0x60, 0xA0, 0x80, 0x60 };

    writeFile(path, "P6\n3 1\n255\n", pixels, sizeof(pixels));
    TEXTURE *loaded = loadTextureFromFile(path);

    REQUIRE(loaded != nullptr);
    CHECK(loaded->width == 3);
    CHECK(loaded->height == 1);
    deleteTexture(loaded);
    unlink(path);
}

TEST_CASE("oversized binary PPM dimensions are rejected before allocation")
{
    const char *path = "/tmp/devoncraft-oversized-atlas.ppm";
    writeFile(path, "P6\n65536 16\n255\n", nullptr, 0);
    TEXTURE *loaded = loadTextureFromFile(path);

    CHECK(loaded == nullptr);
    unlink(path);
}
