# Textured-floor render check (host only)

Renders a textured floor through the real nGL pipeline (nGL/gl.cpp) and compares
every covered pixel against a perspective-correct reference. Each texel encodes
its own (x, y), so errors are read back in texels.

Build and run from the repository root:

    g++ -std=c++11 -O2 -I nGL -I . -I tests/render_check \
        tests/render_check/tex_harness.cpp nGL/gl.cpp nGL/gldrawarray.cpp \
        nGL/texturetools.cpp nGL/fastmath.cpp -o tex_harness && ./tex_harness

tests/render_check/SDL/SDL.h is a stub so gl.cpp builds without SDL on the host.

Expected after the precision fix: mean_err ~0.3 texels, bad(>1 texel) ~1.2%.
The residual is a half-pixel row convention on the far edge, not texture error.
Before the fix: mean_err ~28.5 texels, bad ~99.7%.
