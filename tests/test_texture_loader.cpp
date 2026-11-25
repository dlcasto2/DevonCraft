#include "doctest.h"

#include <algorithm>
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

TEST_CASE("binary PPM preserves whitespace-valued first pixel bytes")
{
    const char *path = "/tmp/devoncraft-whitespace-pixel.ppm";
    // The first RGB triplet deliberately contains valid whitespace byte values.
    const uint8_t pixels[] = { 0x20, 0x0A, 0x0D, 0x55, 0x66, 0x77 };

    writeFile(path, "P6\n2 1\n255\n", pixels, sizeof(pixels));
    TEXTURE *loaded = loadTextureFromFile(path);

    REQUIRE(loaded != nullptr);
    REQUIRE(loaded->width == 2);
    REQUIRE(loaded->height == 1);
    CHECK(loaded->bitmap[0] == 0x2041);
    CHECK(loaded->bitmap[1] == 0x532E);
    deleteTexture(loaded);
    unlink(path);
}

TEST_CASE("blank atlas cells are rejected")
{
    TEXTURE *texture = newTexture(32, 32, 0, true, 0);
    REQUIRE(texture != nullptr);

    CHECK_FALSE(textureAtlasCellHasVisibleContent(*texture, 0, 0));
    CHECK_FALSE(textureAtlasCellHasVisibleContent(*texture, 16, 0));
    deleteTexture(texture);
}

TEST_CASE("visible atlas cells pass validation")
{
    TEXTURE *texture = newTexture(32, 32, 0, true, 0);
    REQUIRE(texture != nullptr);
    std::fill(texture->bitmap, texture->bitmap + 32 * 32, static_cast<COLOR>(0x07E0));

    CHECK(textureAtlasCellHasVisibleContent(*texture, 0, 0));
    CHECK_FALSE(textureAtlasCellHasVisibleContent(*texture, 0, 0, 0, 16));
    deleteTexture(texture);
}

TEST_CASE("oversized binary PPM dimensions are rejected before allocation")
{
    const char *path = "/tmp/devoncraft-oversized-atlas.ppm";
    writeFile(path, "P6\n65536 16\n255\n", nullptr, 0);
    TEXTURE *loaded = loadTextureFromFile(path);

    CHECK(loaded == nullptr);
    unlink(path);
}
