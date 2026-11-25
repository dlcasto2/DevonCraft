#include "mob_renderer.h"

#include "mobs.h"
#include "terrain.h"
#include "textures/animals.h"

namespace {

// Atlas: four 16x16 columns by two rows. Tiles are original DevonCraft pixel art.
TextureAtlasEntry mobTile(unsigned index)
{
    return textureArea((index % 4) * 16, (index / 4) * 16, 16, 16);
}

const TEXTURE &animalTexture()
{
    static const COLOR palette[16] = {
        0x2903, 0x51c6, 0x7a87, 0xa28c,
        0xc34f, 0xe452, 0xf555, 0xfdf7,
        0x2104, 0x49a5, 0xd677, 0xef3a,
        0xffbc, 0x6a26, 0x8287, 0xddb4
    };
    static COLOR pixels[64 * 32];
    static TEXTURE texture = {64, 32, false, 0, pixels};
    static bool initialized = false;
    if(!initialized)
    {
        for(unsigned i = 0; i < 64 * 32; ++i)
        {
            const char c = mob_animals_indices[i];
            const unsigned index = c <= '9' ? static_cast<unsigned>(c - '0')
                                            : static_cast<unsigned>(c - 'a' + 10);
            pixels[i] = palette[index < 16 ? index : 0];
        }
        initialized = true;
    }
    return texture;
}

struct ModelPoint
{
    GLFix x, y, z;
};

// Model forward is +Z. The heading is the same four-way direction used by
// the mob wandering code, so the snout always faces the direction of travel.
ModelPoint worldPoint(const Entity &mob, GLFix local_x, GLFix local_y, GLFix local_z)
{
    const GLFix ox = (mob.box.low_x + mob.box.high_x) / 2;
    const GLFix oz = (mob.box.low_z + mob.box.high_z) / 2;
    GLFix rx = local_x, rz = local_z;
    switch(mob.mob_heading)
    {
    case 0: rx = local_z;  rz = -local_x; break; // +X
    case 2: rx = -local_z; rz = local_x;  break; // -X
    case 3: rx = -local_x; rz = -local_z; break; // -Z
    default: break;                              // +Z or standing
    }
    return {ox + rx, mob.box.low_y + local_y, oz + rz};
}

void addFace(const Entity &mob,
             GLFix x1, GLFix y1, GLFix z1,
             GLFix x2, GLFix y2, GLFix z2,
             GLFix x3, GLFix y3, GLFix z3,
             GLFix x4, GLFix y4, GLFix z4,
             unsigned tile)
{
    const TextureAtlasEntry uv = mobTile(tile);
    const ModelPoint p1 = worldPoint(mob, x1, y1, z1);
    const ModelPoint p2 = worldPoint(mob, x2, y2, z2);
    const ModelPoint p3 = worldPoint(mob, x3, y3, z3);
    const ModelPoint p4 = worldPoint(mob, x4, y4, z4);
    // Model textures are opaque. Disable backface culling so each simple
    // cuboid face works with nGL's small immediate-mode triangle pipeline.
    constexpr COLOR DRAW_FLAGS = TEXTURE_DRAW_BACKFACE;
    glBegin(GL_QUADS);
    nglAddVertex({p1.x, p1.y, p1.z, GLFix(uv.left),  GLFix(uv.bottom), DRAW_FLAGS});
    nglAddVertex({p2.x, p2.y, p2.z, GLFix(uv.left),  GLFix(uv.top),    DRAW_FLAGS});
    nglAddVertex({p3.x, p3.y, p3.z, GLFix(uv.right), GLFix(uv.top),    DRAW_FLAGS});
    nglAddVertex({p4.x, p4.y, p4.z, GLFix(uv.right), GLFix(uv.bottom), DRAW_FLAGS});
    glEnd();
}

struct FaceSet
{
    unsigned front, back, left, right, top, bottom;
};

void cuboid(const Entity &mob, GLFix cx, GLFix cy, GLFix cz,
            GLFix width, GLFix height, GLFix length, const FaceSet &tex)
{
    const GLFix x0 = cx - width / 2, x1 = cx + width / 2;
    const GLFix y0 = cy - height / 2, y1 = cy + height / 2;
    const GLFix z0 = cz - length / 2, z1 = cz + length / 2;

    // Front (+Z), back (-Z), left (-X), right (+X), top, bottom.
    addFace(mob, x0,y0,z1, x0,y1,z1, x1,y1,z1, x1,y0,z1, tex.front);
    addFace(mob, x1,y0,z0, x1,y1,z0, x0,y1,z0, x0,y0,z0, tex.back);
    addFace(mob, x0,y0,z0, x0,y1,z0, x0,y1,z1, x0,y0,z1, tex.left);
    addFace(mob, x1,y0,z1, x1,y1,z1, x1,y1,z0, x1,y0,z0, tex.right);
    addFace(mob, x0,y1,z0, x0,y1,z1, x1,y1,z1, x1,y1,z0, tex.top);
    addFace(mob, x1,y0,z0, x1,y0,z1, x0,y0,z1, x0,y0,z0, tex.bottom);
}

GLFix b(int percent)
{
    return GLFix(BLOCK_SIZE * percent / 100);
}

void pig(const Entity &mob)
{
    const FaceSet skin = {0, 0, 0, 0, 0, 0};
    const FaceSet leg = {0, 0, 0, 0, 0, 0};
    const FaceSet hoof = {4, 4, 4, 4, 4, 4};

    // Broad rectangular body and squared head.
    cuboid(mob, 0, b(46), -b(2), b(59), b(38), b(57), skin);
    cuboid(mob, 0, b(53), b(22), b(31), b(30), b(29), skin);
    cuboid(mob, 0, b(48), b(35), b(20), b(15), b(9), {1,1,1,1,1,1}); // flat snout

    // Ears and inset eye patches.
    cuboid(mob, -b(12), b(69), b(20), b(10), b(11), b(9), skin);
    cuboid(mob,  b(12), b(69), b(20), b(10), b(11), b(9), skin);
    cuboid(mob, -b(12), b(69), b(25), b(6), b(7), b(2), {2,2,2,2,2,2});
    cuboid(mob,  b(12), b(69), b(25), b(6), b(7), b(2), {2,2,2,2,2,2});
    cuboid(mob, -b(17), b(57), b(29), b(3), b(5), b(5), {3,3,3,3,3,3});
    cuboid(mob,  b(17), b(57), b(29), b(3), b(5), b(5), {3,3,3,3,3,3});

    // Four short legs with separate dark blocky hooves.
    const int sx[2] = {-1, 1}, sz[2] = {-1, 1};
    for(int ix=0; ix<2; ++ix)
        for(int iz=0; iz<2; ++iz)
        {
            const GLFix lx = GLFix(sx[ix]) * b(21), lz = GLFix(sz[iz]) * b(20);
            cuboid(mob, lx, b(15), lz, b(14), b(25), b(14), leg);
            cuboid(mob, lx, b(3), lz, b(15), b(6), b(15), hoof);
        }
    // A small curled tail keeps the silhouette recognisably pig-like.
    cuboid(mob, 0, b(49), -b(34), b(5), b(6), b(13), {0,0,0,0,0,0});
}

void cow(const Entity &mob)
{
    const FaceSet coat = {5, 5, 5, 5, 5, 5};
    const FaceSet patches = {6, 6, 6, 6, 5, 5};
    const FaceSet leg = {5, 5, 5, 5, 5, 5};
    const FaceSet hoof = {4, 4, 4, 4, 4, 4};

    // Long, stocky body with irregular dark patches on the sides.
    cuboid(mob, 0, b(45), -b(2), b(61), b(42), b(60), patches);
    cuboid(mob, 0, b(52), b(22), b(31), b(33), b(28), coat);
    cuboid(mob, 0, b(48), b(36), b(24), b(16), b(10), {7,7,7,7,7,7});

    // Paired ears and small pale horns.
    cuboid(mob, -b(22), b(62), b(20), b(12), b(8), b(13), {5,5,5,5,5,5});
    cuboid(mob,  b(22), b(62), b(20), b(12), b(8), b(13), {5,5,5,5,5,5});
    cuboid(mob, -b(22), b(62), b(26), b(7), b(4), b(3), {2,2,2,2,2,2});
    cuboid(mob,  b(22), b(62), b(26), b(7), b(4), b(3), {2,2,2,2,2,2});
    cuboid(mob, -b(13), b(72), b(23), b(7), b(10), b(7), coat);
    cuboid(mob,  b(13), b(72), b(23), b(7), b(10), b(7), coat);
    cuboid(mob, -b(16), b(56), b(29), b(3), b(5), b(5), {4,4,4,4,4,4});
    cuboid(mob,  b(16), b(56), b(29), b(3), b(5), b(5), {4,4,4,4,4,4});

    // Four pale lower legs and dark hooves.
    const int sx[2] = {-1, 1}, sz[2] = {-1, 1};
    for(int ix=0; ix<2; ++ix)
        for(int iz=0; iz<2; ++iz)
        {
            const GLFix lx = GLFix(sx[ix]) * b(22), lz = GLFix(sz[iz]) * b(21);
            cuboid(mob, lx, b(15), lz, b(14), b(25), b(14), leg);
            cuboid(mob, lx, b(3), lz, b(15), b(6), b(15), hoof);
        }

    // A short tail with a darker tip.
    cuboid(mob, 0, b(52), -b(34), b(5), b(6), b(14), {5,5,5,5,5,5});
    cuboid(mob, 0, b(49), -b(41), b(6), b(6), b(5), {4,4,4,4,4,4});
}


void sheep(const Entity &mob)
{
    const FaceSet fleece = {7, 7, 7, 7, 7, 7};
    const FaceSet body = {15, 15, 15, 15, 15, 15};
    const FaceSet face = {5, 5, 5, 5, 5, 5};
    const FaceSet leg = {15, 15, 15, 15, 15, 15};
    const FaceSet hoof = {4, 4, 4, 4, 4, 4};
    const FaceSet eye = {8, 8, 8, 8, 8, 8};

    if(!mob.sheared)
    {
        // Thick, blocky fleece over the back and flanks.
        cuboid(mob, 0, b(47), -b(2), b(70), b(48), b(66), fleece);
        cuboid(mob, -b(22), b(51), -b(4), b(27), b(34), b(33), fleece);
        cuboid(mob,  b(22), b(51), -b(4), b(27), b(34), b(33), fleece);
        cuboid(mob, 0, b(67), -b(2), b(51), b(25), b(49), fleece);
        cuboid(mob, 0, b(64), b(24), b(43), b(30), b(33), fleece);
    }
    else
    {
        // The close-cropped body remains visible after the fleece is removed.
        cuboid(mob, 0, b(45), -b(2), b(51), b(33), b(57), body);
        cuboid(mob, 0, b(58), b(20), b(32), b(26), b(29), body);
    }

    // Narrow face, ears and dark eyes; the face stays exposed when wool grows.
    cuboid(mob, 0, b(62), b(28), b(28), b(30), b(31), face);
    cuboid(mob, -b(18), b(68), b(23), b(12), b(8), b(15), body);
    cuboid(mob,  b(18), b(68), b(23), b(12), b(8), b(15), body);
    cuboid(mob, -b(9), b(66), b(43), b(4), b(5), b(3), eye);
    cuboid(mob,  b(9), b(66), b(43), b(4), b(5), b(3), eye);
    cuboid(mob, 0, b(54), b(45), b(10), b(8), b(5), {4,4,4,4,4,4});

    // Four short legs and dark hooves.
    const int sx[2] = {-1, 1}, sz[2] = {-1, 1};
    for(int ix=0; ix<2; ++ix)
        for(int iz=0; iz<2; ++iz)
        {
            const GLFix lx = GLFix(sx[ix]) * b(21), lz = GLFix(sz[iz]) * b(20);
            cuboid(mob, lx, b(15), lz, b(13), b(25), b(13), leg);
            cuboid(mob, lx, b(3), lz, b(14), b(6), b(14), hoof);
        }
}

void trader(const Entity &mob)
{
    const FaceSet robe = {0, 0, 2, 1, 1, 2};
    const FaceSet sleeve = {1, 1, 0, 0, 1, 2};
    const FaceSet hood = {6, 6, 6, 6, 6, 6};
    const FaceSet face = {5, 5, 5, 5, 5, 5};
    const FaceSet belt = {3, 3, 3, 3, 3, 3};
    const FaceSet boot = {4, 4, 4, 4, 4, 4};
    const FaceSet eye = {4, 4, 4, 4, 4, 4};

    // Long blue-green robe, belt and two hanging sleeves.
    cuboid(mob, 0, b(80), 0, b(64), b(94), b(46), robe);
    cuboid(mob, 0, b(43), b(1), b(66), b(9), b(49), belt);
    cuboid(mob, -b(39), b(78), 0, b(17), b(68), b(20), sleeve);
    cuboid(mob,  b(39), b(78), 0, b(17), b(68), b(20), sleeve);

    // Hooded head and the characteristic long villager nose.
    cuboid(mob, 0, b(143), 0, b(43), b(43), b(42), face);
    cuboid(mob, 0, b(168), -b(1), b(54), b(12), b(48), hood);
    cuboid(mob, 0, b(142), b(27), b(9), b(12), b(17), {3,3,3,3,3,3});
    cuboid(mob, -b(12), b(151), b(21), b(5), b(5), b(3), eye);
    cuboid(mob,  b(12), b(151), b(21), b(5), b(5), b(3), eye);

    // Boots below the robe.
    cuboid(mob, -b(18), b(11), 0, b(19), b(22), b(25), boot);
    cuboid(mob,  b(18), b(11), 0, b(19), b(22), b(25), boot);
}

const TEXTURE &traderTexture()
{
    static const COLOR palette[8] = {
        0x2D5A, 0x3E7D, 0x1B34, 0x9A45,
        0x5203, 0xD6B2, 0xB58D, 0xE71C
    };
    static COLOR pixels[64 * 32];
    static TEXTURE texture = {64, 32, false, 0, pixels};
    static bool initialized = false;
    if(!initialized)
    {
        for(unsigned y = 0; y < 32; ++y)
            for(unsigned x = 0; x < 64; ++x)
            {
                const unsigned tile = (y / 16) * 4 + (x / 16);
                COLOR c = palette[tile];
                if(tile < 3 && ((x + y) % 7 == 0))
                    c = palette[tile == 1 ? 0 : 1];
                pixels[y * 64 + x] = c;
            }
        initialized = true;
    }
    return texture;
}

} // namespace

void renderMob(const Entity &mob)
{
    if(mob.mob_kind == MOB_TRADER)
    {
        glBindTexture(&traderTexture());
        trader(mob);
    }
    else
    {
        glBindTexture(&animalTexture());
        if(mob.mob_kind == MOB_COW)
            cow(mob);
        else if(mob.mob_kind == MOB_SHEEP)
            sheep(mob);
        else
            pig(mob);
    }
}


// The box is drawn without a texture so the outline remains a clear, fixed
// colour over the mob model and the world behind it.
void renderMobHitbox(const Entity &mob)
{
    const AABB &b = mob.box;
    const COLOR outline = mob.hurt_ticks > 0 ? 0xF800 : 0x07FF; // red while hurt, cyan otherwise
    const VERTEX v[8] = {
        {b.low_x,  b.low_y,  b.low_z,  0, 0, outline},
        {b.high_x, b.low_y,  b.low_z,  0, 0, outline},
        {b.low_x,  b.high_y, b.low_z,  0, 0, outline},
        {b.high_x, b.high_y, b.low_z,  0, 0, outline},
        {b.low_x,  b.low_y,  b.high_z, 0, 0, outline},
        {b.high_x, b.low_y,  b.high_z, 0, 0, outline},
        {b.low_x,  b.high_y, b.high_z, 0, 0, outline},
        {b.high_x, b.high_y, b.high_z, 0, 0, outline}
    };

    glBindTexture(nullptr);
    glBegin(GL_LINE_STRIP);
    nglAddVertex(v[0]); nglAddVertex(v[1]); nglAddVertex(v[5]); nglAddVertex(v[4]); nglAddVertex(v[0]);
    glEnd();
    glBegin(GL_LINE_STRIP);
    nglAddVertex(v[2]); nglAddVertex(v[3]); nglAddVertex(v[7]); nglAddVertex(v[6]); nglAddVertex(v[2]);
    glEnd();
    glBegin(GL_LINE_STRIP);
    nglAddVertex(v[0]); nglAddVertex(v[2]);
    glEnd();
    glBegin(GL_LINE_STRIP);
    nglAddVertex(v[1]); nglAddVertex(v[3]);
    glEnd();
    glBegin(GL_LINE_STRIP);
    nglAddVertex(v[4]); nglAddVertex(v[6]);
    glEnd();
    glBegin(GL_LINE_STRIP);
    nglAddVertex(v[5]); nglAddVertex(v[7]);
    glEnd();
}
