#pragma once
#include <cstdint>
struct SDL_Surface { int w, h, pitch; void *pixels; };
#define SDL_INIT_VIDEO 0
#define SDL_SWSURFACE 0
inline int SDL_Init(int) { return 0; }
inline SDL_Surface *SDL_SetVideoMode(int, int, int, int) { static SDL_Surface s{320,240,640,nullptr}; return &s; }
inline int SDL_LockSurface(SDL_Surface *) { return 0; }
inline void SDL_UnlockSurface(SDL_Surface *) {}
inline void SDL_UpdateRect(SDL_Surface *, int, int, int, int) {}
