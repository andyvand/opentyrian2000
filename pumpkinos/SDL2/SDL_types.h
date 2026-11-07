/*
 * OpenTyrian 2000 PumpkinOS port: minimal SDL2 type definitions.
 *
 * The game sources are written against the SDL2 API. On PumpkinOS there is
 * no SDL available to applications, so the small subset of SDL2 the game
 * actually uses is re-implemented on top of the PalmOS/PumpkinOS API (see
 * pumpkinos/sdl_shim.c). These headers mimic the SDL2 declarations.
 */
#ifndef PUMPKIN_SDL_TYPES_H
#define PUMPKIN_SDL_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef int8_t   Sint8;
typedef uint8_t  Uint8;
typedef int16_t  Sint16;
typedef uint16_t Uint16;
typedef int32_t  Sint32;
typedef uint32_t Uint32;
typedef int64_t  Sint64;
typedef uint64_t Uint64;

typedef enum {
	SDL_FALSE = 0,
	SDL_TRUE = 1
} SDL_bool;

#endif /* PUMPKIN_SDL_TYPES_H */
