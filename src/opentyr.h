/*
 * OpenTyrian: A modern cross-platform port of Tyrian
 * Copyright (C) 2007-2009  The OpenTyrian Development Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#ifndef OPENTYR_H
#define OPENTYR_H

#if defined(ANDROID) || defined(__ANDROID__)
#ifndef WITH_SDL3
#define WITH_SDL3 1
#endif

#ifndef WITH_NETWORK
#define WITH_NETWORK 1
#endif

#ifndef WITH_SDL3
#ifndef WITH_MIDI
#define WITH_MIDI 1
#endif

#ifndef NO_NATIVE_MIDI
#define NO_NATIVE_MIDI 1
#endif
#endif
#endif

#ifdef WITH_SDL3
#include <SDL3/SDL.h>
#else
#ifdef WITH_SDL
#include <SDL.h>
#else
#include <SDL2/SDL_types.h>
#endif
#endif

#ifdef VITA
#ifdef VDEBUG
#include <psp2shell.h>
#define printf psp2shell_print
#endif
#endif

#ifdef WITH_SDL
#define EXTATTR EXT_RAM_ATTR
#else
#define EXTATTR
#endif

#ifdef PSP
#ifdef _DEBUG
#include <pspdebug.h>

#define _printf pspDebugScreenPrintf
#define _fprintf(X, ...) if ((X == stderr) || (X == stdout)) { pspDebugScreenPrintf(__VA_ARGS__); }  else { fprintf(X, __VA_ARGS__); }
#else
#define _printf printf
#define _fprintf fprintf
#endif
#else
#ifdef WITH_SDL
#include "esp_log.h"

#define _printf(...) ESP_LOGI(opentyrian_str, __VA_ARGS__)
#define _fprintf(X, ...) if ((X == stderr) || (X == stdout)) { ESP_LOGI(opentyrian_str, __VA_ARGS__); } else { fprintf(X, __VA_ARGS__); }
#else
#define _printf printf
#define _fprintf fprintf
#endif
#endif

#include <math.h>
#include <stdbool.h>

#ifndef COUNTOF
#define COUNTOF(x) ((unsigned)(sizeof(x) / sizeof *(x)))  // use only on arrays!
#endif
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifndef M_PI
#define M_PI    3.14159265358979323846  // pi
#endif
#ifndef M_PI_2
#define M_PI_2  1.57079632679489661923  // pi/2
#endif
#ifndef M_PI_4
#define M_PI_4  0.78539816339744830962  // pi/4
#endif

typedef unsigned int uint;
typedef unsigned long ulong;

// Pascal types, yuck.
typedef Sint32 JE_longint;
typedef Sint16 JE_integer;
typedef Sint8  JE_shortint;
typedef Uint16 JE_word;
typedef Uint8  JE_byte;
typedef bool   JE_boolean;
typedef char   JE_char;
typedef float  JE_real;

#ifndef TYRIAN_VERSION
#define TYRIAN_VERSION "2000"
#endif

extern const char *opentyrian_str;
extern const char *opentyrian_version;

void setupMenu(void);

#if defined(__linux__) || defined(VITA) || defined(PSP)
#ifndef strlcpy
#define strlcpy SDL_strlcpy
#endif /* strlcpy */

#ifndef strlcat
#define strlcat SDL_strlcat
#endif /* strlcat */
#endif /* __linux__ */

#endif /* OPENTYR_H */
