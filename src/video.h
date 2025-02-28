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
#ifndef VIDEO_H
#define VIDEO_H

#include "opentyr.h"

#ifdef WITH_SDL3
#include <SDL3/SDL.h>
#else
#ifdef WITH_SDL
#include <SDL.h>
#else
#include <SDL2/SDL.h>
#endif
#endif

#define vga_width 320
#define vga_height 200

#ifdef VITA
typedef enum {
    SCALE_INTEGER,
    SCALE_FIT,
    SCALE_FULLSCREEN,
    ScalingMode_MAX
} ScalingMode;
#else
typedef enum {
	SCALE_CENTER,
	SCALE_INTEGER,
	SCALE_ASPECT_8_5,
	SCALE_ASPECT_4_3,
	ScalingMode_MAX
} ScalingMode;
#endif

extern const char *const scaling_mode_names[ScalingMode_MAX];

extern int fullscreen_display; // -1 means windowed
extern ScalingMode scaling_mode;

extern SDL_Surface *VGAScreen, *VGAScreenSeg;
extern SDL_Surface *game_screen;
extern SDL_Surface *VGAScreen2;

#ifndef WITH_SDL
extern SDL_Window *main_window;
#endif

#ifdef WITH_SDL3
#if defined(__ANDROID__) || defined(ANDROID)
typedef struct SDL_PixelFormatDetails *SDL_PixelFormatDetailsPtr;
extern SDL_PixelFormatDetailsPtr main_window_tex_format;
#else
extern const SDL_PixelFormatDetails *main_window_tex_format;
#endif
#else
extern SDL_PixelFormat *main_window_tex_format;
#endif

#ifdef WITH_SDL
void init_video( void );

int can_init_scaler( unsigned int new_scaler, bool fullscreen );
bool init_scaler( unsigned int new_scaler, bool fullscreen );
bool can_init_any_scaler( bool fullscreen );
bool init_any_scaler( bool fullscreen );
bool set_scaling_mode_by_name(const char *name);

void deinit_video( void );

void JE_clr256( SDL_Surface * );
void JE_showVGA( void );
void scale_and_flip( SDL_Surface * );
#else
void init_video(void);

void video_on_win_resize(void);
void reinit_fullscreen(int new_display);
void toggle_fullscreen(void);
bool init_scaler(unsigned int new_scaler);
bool set_scaling_mode_by_name(const char *name);

void deinit_video(void);

void JE_clr256(SDL_Surface *);
void JE_showVGA(void);
#endif

void mapScreenPointToWindow(Sint32 *inout_x, Sint32 *inout_y);
void mapWindowPointToScreen(Sint32 *inout_x, Sint32 *inout_y);
void scaleWindowDistanceToScreen(Sint32 *inout_x, Sint32 *inout_y);

#endif /* VIDEO_H */
