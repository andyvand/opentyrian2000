/*
 * OpenTyrian 2000 PumpkinOS port: SDL2 byte order helpers.
 */
#ifndef PUMPKIN_SDL_ENDIAN_H
#define PUMPKIN_SDL_ENDIAN_H

#include "SDL_types.h"

#define SDL_LIL_ENDIAN 1234
#define SDL_BIG_ENDIAN 4321

#ifndef SDL_BYTEORDER
#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#define SDL_BYTEORDER SDL_BIG_ENDIAN
#else
#define SDL_BYTEORDER SDL_LIL_ENDIAN
#endif
#endif

static inline Uint16 SDL_Swap16(Uint16 x)
{
	return (Uint16)((x << 8) | (x >> 8));
}

static inline Uint32 SDL_Swap32(Uint32 x)
{
	return (Uint32)((x << 24) | ((x << 8) & 0x00FF0000) | ((x >> 8) & 0x0000FF00) | (x >> 24));
}

#if SDL_BYTEORDER == SDL_LIL_ENDIAN
#define SDL_SwapLE16(X) (X)
#define SDL_SwapLE32(X) (X)
#define SDL_SwapBE16(X) SDL_Swap16(X)
#define SDL_SwapBE32(X) SDL_Swap32(X)
#else
#define SDL_SwapLE16(X) SDL_Swap16(X)
#define SDL_SwapLE32(X) SDL_Swap32(X)
#define SDL_SwapBE16(X) (X)
#define SDL_SwapBE32(X) (X)
#endif

#endif /* PUMPKIN_SDL_ENDIAN_H */
