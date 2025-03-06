#ifndef STANDARDSDL2_H
#define STANDARDSDL2_H

#if WIN32
// SDL2 includes
#include <SDL2/SDL.h>
#elif MACOSX
// SDL2 includes
#include <SDL2/SDL.h>
#elif SWITCH
// SDL2 includes
#include <SDL2/SDL.h>
// Important includes
#include <switch.h>
#elif IOS
// SDL2 includes
#include "SDL.h"
#elif ANDROID
// SDL2 includes
#include <SDL.h>
#include <SDL_opengles2.h>
#elif LINUX
// SDL2 includes
#include <SDL.h>
#endif

typedef uint32_t uint;

#endif // STANDARDSDL2_H
