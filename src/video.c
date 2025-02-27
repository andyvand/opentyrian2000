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
#include "video.h"

#include "keyboard.h"
#include "opentyr.h"
#include "palette.h"
#include "video_scale.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *const scaling_mode_names[ScalingMode_MAX] = {
	"Center",
	"Integer",
	"Fit 8:5",
	"Fit 4:3",
};

int fullscreen_display;
#if defined(IOS) || defined(ANDROID) || defined(__ANDROID__) || defined(__3DS__)
ScalingMode scaling_mode = SCALE_ASPECT_4_3;
#else
ScalingMode scaling_mode = SCALE_INTEGER;
#endif
static SDL_Rect last_output_rect = { 0, 0, vga_width, vga_height };

SDL_Surface *VGAScreen, *VGAScreenSeg;
SDL_Surface *VGAScreen2;
SDL_Surface *game_screen;

SDL_Window *main_window = NULL;
static SDL_Renderer *main_window_renderer = NULL;

#ifdef WITH_SDL3
#if defined(ANDROID) || defined(__ANDROID__)
SDL_PixelFormatDetailsPtr main_window_tex_format = NULL;
#else
const SDL_PixelFormatDetails *main_window_tex_format = NULL;
#endif
#else
SDL_PixelFormat *main_window_tex_format = NULL;
#endif

static SDL_Texture *main_window_texture = NULL;

static ScalerFunction scaler_function;

static void init_renderer(void);
static void deinit_renderer(void);
static void init_texture(void);
static void deinit_texture(void);

static int window_get_display_index(void);
static void window_center_in_display(int display_index);
static void calc_dst_render_rect(SDL_Surface *src_surface, SDL_Rect *dst_rect);
static void scale_and_flip(SDL_Surface *);

void init_video(void)
{
#ifndef WITH_SDL3
    if (SDL_WasInit(SDL_INIT_VIDEO))
        return;

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) == -1)
    {
        _fprintf(stderr, "error: failed to initialize SDL video: %s\n", SDL_GetError());
        exit(1);
    }
#else
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) == false)
    {
        _fprintf(stderr, "error: failed to initialize SDL video: %s\n", SDL_GetError());
        exit(1);
    }

    if (SDL_WasInit(SDL_INIT_VIDEO) == false)
    {
        return;
    }
#endif

	// Create the software surfaces that the game renders to. These are all 320x200x8 regardless
	// of the window size or monitor resolution.
#ifdef WITH_SDL3
    VGAScreen = VGAScreenSeg = SDL_CreateSurface(vga_width, vga_height, SDL_GetPixelFormatForMasks(8, 0, 0, 0, 0));
    VGAScreen2 = SDL_CreateSurface(vga_width, vga_height, SDL_GetPixelFormatForMasks(8, 0, 0, 0, 0));
    game_screen = SDL_CreateSurface(vga_width, vga_height, SDL_GetPixelFormatForMasks(8, 0, 0, 0, 0));
#else
	VGAScreen = VGAScreenSeg = SDL_CreateRGBSurface(0, vga_width, vga_height, 8, 0, 0, 0, 0);
	VGAScreen2 = SDL_CreateRGBSurface(0, vga_width, vga_height, 8, 0, 0, 0, 0);
	game_screen = SDL_CreateRGBSurface(0, vga_width, vga_height, 8, 0, 0, 0, 0);
#endif

	// The game code writes to surface->pixels directly without locking, so make sure that we
	// indeed created software surfaces that support this.
	assert(!SDL_MUSTLOCK(VGAScreen));
	assert(!SDL_MUSTLOCK(VGAScreen2));
	assert(!SDL_MUSTLOCK(game_screen));

	JE_clr256(VGAScreen);

	// Create the window with a temporary initial size, hidden until we set up the
	// scaler and find the true window size
#ifdef WITH_SDL3
#if defined(IOS) || defined(__3DS__)
    main_window = SDL_CreateWindow(opentyrian_str, vga_width, vga_height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_FULLSCREEN);
#else
    main_window = SDL_CreateWindow(opentyrian_str, vga_width, vga_height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN);
#endif
#else
#ifdef IOS
    main_window = SDL_CreateWindow(opentyrian_str,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        vga_width, vga_height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_FULLSCREEN);
#else
    main_window = SDL_CreateWindow(opentyrian_str,
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		vga_width, vga_height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN);
#endif
#endif

	if (main_window == NULL)
	{
		_fprintf(stderr, "error: failed to create window: %s\n", SDL_GetError());
		exit(EXIT_FAILURE);
	}

	reinit_fullscreen(fullscreen_display);
	init_renderer();
	init_texture();
	init_scaler(scaler);

	SDL_ShowWindow(main_window);

	SDL_SetRenderDrawColor(main_window_renderer, 0, 0, 0, 255);
	SDL_RenderClear(main_window_renderer);
	SDL_RenderPresent(main_window_renderer);
}

void deinit_video(void)
{
	deinit_texture();
	deinit_renderer();

	SDL_DestroyWindow(main_window);

#ifdef WITH_SDL3
    SDL_DestroySurface(VGAScreenSeg);
    SDL_DestroySurface(VGAScreen2);
    SDL_DestroySurface(game_screen);
#else
	SDL_FreeSurface(VGAScreenSeg);
	SDL_FreeSurface(VGAScreen2);
	SDL_FreeSurface(game_screen);
#endif

	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

static void init_renderer(void)
{
#ifdef WITH_SDL3
    main_window_renderer = SDL_CreateRenderer(main_window, NULL);
#else
    main_window_renderer = SDL_CreateRenderer(main_window, -1, 0);
#endif

    if (main_window_renderer == NULL)
	{
		_fprintf(stderr, "error: failed to create renderer: %s\n", SDL_GetError());
		exit(EXIT_FAILURE);
	}
}

static void deinit_renderer(void)
{
	if (main_window_renderer != NULL)
	{
		SDL_DestroyRenderer(main_window_renderer);
		main_window_renderer = NULL;
	}
}

static void init_texture(void)
{
	assert(main_window_renderer != NULL);

	int bpp = 32; // TODOSDL2

#ifdef WITH_SDL3
    Uint32 format = bpp == 32 ? SDL_PIXELFORMAT_XRGB8888 : SDL_PIXELFORMAT_RGB565;
#else
	Uint32 format = bpp == 32 ? SDL_PIXELFORMAT_RGB888 : SDL_PIXELFORMAT_RGB565;
#endif

	int scaler_w = scalers[scaler].width;
	int scaler_h = scalers[scaler].height;

#ifdef WITH_SDL3
#if defined(ANDROID) || defined(__ANDROID__)
    main_window_tex_format = (SDL_PixelFormatDetailsPtr)SDL_GetPixelFormatDetails(format);
#else
    main_window_tex_format = SDL_GetPixelFormatDetails(format);
#endif
#else
	main_window_tex_format = SDL_AllocFormat(format);
#endif

	main_window_texture = SDL_CreateTexture(main_window_renderer, format, SDL_TEXTUREACCESS_STREAMING, scaler_w, scaler_h);

	if (main_window_texture == NULL)
	{
		_fprintf(stderr, "error: failed to create scaler texture %dx%dx%s: %s\n", scaler_w, scaler_h, SDL_GetPixelFormatName(format), SDL_GetError());
		exit(EXIT_FAILURE);
	}
}

static void deinit_texture(void)
{
	if (main_window_texture != NULL)
	{
		SDL_DestroyTexture(main_window_texture);
		main_window_texture = NULL;
	}

	if (main_window_tex_format != NULL)
    {
#ifndef WITH_SDL3
		SDL_FreeFormat(main_window_tex_format);
#endif

		main_window_tex_format = NULL;
	}
}

static int window_get_display_index(void)
{
#ifdef WITH_SDL3
    return SDL_GetDisplayForWindow(main_window);
#else
	return SDL_GetWindowDisplayIndex(main_window);
#endif
}

static void window_center_in_display(int display_index)
{
	int win_w, win_h;
	SDL_GetWindowSize(main_window, &win_w, &win_h);

	SDL_Rect bounds;
	SDL_GetDisplayBounds(display_index, &bounds);

	SDL_SetWindowPosition(main_window, bounds.x + (bounds.w - win_w) / 2, bounds.y + (bounds.h - win_h) / 2);
}

void reinit_fullscreen(int new_display)
{
	fullscreen_display = new_display;

#ifdef WITH_SDL3
    int num_displays = 0;

    SDL_GetDisplays(&num_displays);

    if (fullscreen_display >= num_displays)
#else
	if (fullscreen_display >= SDL_GetNumVideoDisplays())
#endif
	{
		fullscreen_display = 0;
	}

	SDL_SetWindowFullscreen(main_window, false);
	SDL_SetWindowSize(main_window, scalers[scaler].width, scalers[scaler].height);

	if (fullscreen_display == -1)
	{
		window_center_in_display(window_get_display_index());
	}
	else
	{
		window_center_in_display(fullscreen_display);

#ifdef WITH_SDL3
        if (SDL_SetWindowFullscreen(main_window, true) == false)
#else
		if (SDL_SetWindowFullscreen(main_window, SDL_WINDOW_FULLSCREEN_DESKTOP) != 0)
#endif
		{
			reinit_fullscreen(-1);
			return;
        }
	}
}

void video_on_win_resize(void)
{
	int w, h;
	int scaler_w, scaler_h;

	// Tell video to reinit if the window was manually resized by the user.
	// Also enforce a minimum size on the window.

	SDL_GetWindowSize(main_window, &w, &h);
	scaler_w = scalers[scaler].width;
	scaler_h = scalers[scaler].height;

	if (w < scaler_w || h < scaler_h)
	{
		w = w < scaler_w ? scaler_w : w;
		h = h < scaler_h ? scaler_h : h;

		SDL_SetWindowSize(main_window, w, h);
	}
}

void toggle_fullscreen(void)
{
	if (fullscreen_display != -1)
		reinit_fullscreen(-1);
	else
#ifdef WITH_SDL3
        reinit_fullscreen(SDL_GetDisplayForWindow(main_window));
#else
		reinit_fullscreen(SDL_GetWindowDisplayIndex(main_window));
#endif
}

bool init_scaler(unsigned int new_scaler)
{
	int w = scalers[new_scaler].width,
	    h = scalers[new_scaler].height;
#ifdef WITH_SDL3
    int bpp = main_window_tex_format->bits_per_pixel; // TODOSDL2
#else
	int bpp = main_window_tex_format->BitsPerPixel; // TODOSDL2
#endif

	scaler = new_scaler;

	deinit_texture();
	init_texture();

	if (fullscreen_display == -1)
	{
		// Changing scalers, when not in fullscreen mode, forces the window
		// to resize to exactly match the scaler's output dimensions.
		SDL_SetWindowSize(main_window, w, h);
		window_center_in_display(window_get_display_index());
    }

	switch (bpp)
	{
	case 32:
		scaler_function = scalers[scaler].scaler32;
		break;
	case 16:
		scaler_function = scalers[scaler].scaler16;
		break;
	default:
		scaler_function = NULL;
		break;
	}

	if (scaler_function == NULL)
	{
		assert(false);
		return false;
	}

	return true;
}

bool set_scaling_mode_by_name(const char *name)
{
	for (int i = 0; i < ScalingMode_MAX; ++i)
	{
		 if (strcmp(name, scaling_mode_names[i]) == 0)
		 {
			 scaling_mode = i;
			 return true;
		 }
	}
	return false;
}

void JE_clr256(SDL_Surface *screen)
{
#ifdef WITH_SDL3
    SDL_FillSurfaceRect(screen, NULL, 0);
#else
	SDL_FillRect(screen, NULL, 0);
#endif
}

void JE_showVGA(void)
{
	scale_and_flip(VGAScreen);
}

static void calc_dst_render_rect(SDL_Surface *const src_surface, SDL_Rect *const dst_rect)
{
	// Decides how the logical output texture (after software scaling applied) will fit
	// in the window.

	int win_w, win_h;
#ifdef WITH_SDL3
    float dstwidth;
    float dstheight;
#endif
	SDL_GetWindowSize(main_window, &win_w, &win_h);

	int maxh_width, maxw_height;

	switch (scaling_mode)
	{
	case SCALE_CENTER:
#ifdef WITH_SDL3
        SDL_GetTextureSize(main_window_texture, &dstwidth, &dstheight);
        dst_rect->w = (int)dstwidth;
        dst_rect->h = (int)dstheight;
#else
        SDL_QueryTexture(main_window_texture, NULL, NULL, &dst_rect->w, &dst_rect->h);
#endif

		break;
	case SCALE_INTEGER:
		dst_rect->w = src_surface->w;
		dst_rect->h = src_surface->h;
		while (dst_rect->w + src_surface->w <= win_w && dst_rect->h + src_surface->h <= win_h)
		{
			dst_rect->w += src_surface->w;
			dst_rect->h += src_surface->h;
		}
		break;
	case SCALE_ASPECT_8_5:
		maxh_width = win_h * (8.f / 5.f);
		maxw_height = win_w * (5.f / 8.f);

		if (maxh_width > win_w)
		{
			dst_rect->w = win_w;
			dst_rect->h = maxw_height;
		}
		else
		{
			dst_rect->w = maxh_width;
			dst_rect->h = win_h;
		}
		break;
	case SCALE_ASPECT_4_3:
		maxh_width = win_h * (4.f / 3.f);
		maxw_height = win_w * (3.f / 4.f);

		if (maxh_width > win_w)
		{
			dst_rect->w = win_w;
			dst_rect->h = maxw_height;
		}
		else
		{
			dst_rect->w = maxh_width;
			dst_rect->h = win_h;
		}
		break;
	case ScalingMode_MAX:
		assert(false);
		break;
	}

	dst_rect->x = (win_w - dst_rect->w) / 2;
	dst_rect->y = (win_h - dst_rect->h) / 2;
}

static void scale_and_flip(SDL_Surface *src_surface)
{
	// Do software scaling
    scaler_function(src_surface, main_window_texture);

#ifdef WITH_SDL3
	SDL_FRect dst_frect;
#endif

    SDL_Rect dst_rect;
	calc_dst_render_rect(src_surface, &dst_rect);

#ifdef WITH_SDL3
    dst_frect.x = (float)dst_rect.x;
    dst_frect.y = (float)dst_rect.y;
    dst_frect.w = (float)dst_rect.w;
    dst_frect.h = (float)dst_rect.h;
#endif

	// Clear the window and blit the output texture to it
	SDL_SetRenderDrawColor(main_window_renderer, 0, 0, 0, 255);
    SDL_RenderClear(main_window_renderer);

#ifdef WITH_SDL3
    SDL_RenderTexture(main_window_renderer, main_window_texture, NULL, &dst_frect);
#else
	SDL_RenderCopy(main_window_renderer, main_window_texture, NULL, &dst_rect);
#endif

	SDL_RenderPresent(main_window_renderer);

#ifdef WITH_SDL3
    dst_rect.x = (int)dst_frect.x;
    dst_rect.y = (int)dst_frect.y;
    dst_rect.w = (int)dst_frect.w;
    dst_rect.h = (int)dst_frect.h;
#endif

	// Save output rect to be used by mouse functions
	last_output_rect = dst_rect;
}

/** Maps a specified point in game screen coordinates to window coordinates. */
void mapScreenPointToWindow(Sint32 *const inout_x, Sint32 *const inout_y)
{
	*inout_x = (2 * *inout_x + 1) * last_output_rect.w / (2 * VGAScreen->w) + last_output_rect.x;
	*inout_y = (2 * *inout_y + 1) * last_output_rect.h / (2 * VGAScreen->h) + last_output_rect.y;
}

/** Maps a specified point in window coordinates to game screen coordinates. */
void mapWindowPointToScreen(Sint32 *const inout_x, Sint32 *const inout_y)
{
	*inout_x = (2 * (*inout_x - last_output_rect.x) + 1) * VGAScreen->w / (2 * last_output_rect.w);
	*inout_y = (2 * (*inout_y - last_output_rect.y) + 1) * VGAScreen->h / (2 * last_output_rect.h);
}

/** Scales a distance in window coordinates to game screen coordinates. */
void scaleWindowDistanceToScreen(Sint32 *const inout_x, Sint32 *const inout_y)
{
	*inout_x = (2 * *inout_x + 1) * VGAScreen->w / (2 * last_output_rect.w);
	*inout_y = (2 * *inout_y + 1) * VGAScreen->h / (2 * last_output_rect.h);
}
