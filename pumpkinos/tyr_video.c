/*
 * OpenTyrian 2000 PumpkinOS port: video layer.
 *
 * Replaces src/video.c and src/video_scale*.c. The game renders into the
 * usual 320x200 8 bpp software surfaces; JE_showVGA() converts the frame
 * to RGB565 through the current palette and hands it to the PalmOS side
 * (pk_present()), which scales it into the application window.
 *
 * Only one "scaler" (none) exists. The scaling mode from the setup menu
 * selects between an unscaled, centered image and the largest integer
 * scale that fits the window.
 */
#include "video.h"
#include "video_scale.h"

#include "opentyr.h"
#include "palette.h"

#include "pumpkin_glue.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *const scaling_mode_names[ScalingMode_MAX] = {
	"Center",
	"Integer",
	"Fit 8:5",
	"Fit 4:3",
};

int fullscreen_display = -1;
/* PumpkinOS windows are a little smaller than 640 pixels because of the
   window border, so the 2x integer scale never fits: fill the window. */
ScalingMode scaling_mode = SCALE_ASPECT_8_5;

SDL_Surface *VGAScreen, *VGAScreenSeg;
SDL_Surface *VGAScreen2;
SDL_Surface *game_screen;

SDL_Window *main_window = NULL;
SDL_PixelFormat *main_window_tex_format = NULL;

uint scaler = 0;
const struct Scalers scalers[] = {
	{ vga_width, vga_height, NULL, NULL, "None" },
};
const uint scalers_count = 1;

static Uint16 frame_buffer[vga_width * vga_height];

void set_scaler_by_name(const char *name)
{
	(void)name;
	scaler = 0;
}

static void sync_scaling(void)
{
	pk_set_scaling((int)scaling_mode);
}

void init_video(void)
{
	if (SDL_WasInit(SDL_INIT_VIDEO))
		return;

	SDL_InitSubSystem(SDL_INIT_VIDEO);

	main_window_tex_format = calloc(1, sizeof(*main_window_tex_format));
	if (main_window_tex_format == NULL)
	{
		_fprintf(stderr, "error: out of memory\n");
		exit(EXIT_FAILURE);
	}

	/* RGB565, the format pk_present() expects */
	main_window_tex_format->format = SDL_PIXELFORMAT_RGB565;
	main_window_tex_format->palette = NULL;
	main_window_tex_format->BitsPerPixel = 16;
	main_window_tex_format->BytesPerPixel = 2;
	main_window_tex_format->Rshift = 11;
	main_window_tex_format->Gshift = 5;
	main_window_tex_format->Bshift = 0;
	main_window_tex_format->Ashift = 0;
	main_window_tex_format->Rmask = 0x1Fu << 11;
	main_window_tex_format->Gmask = 0x3Fu << 5;
	main_window_tex_format->Bmask = 0x1Fu;
	main_window_tex_format->Amask = 0;
	main_window_tex_format->Rloss = 3;
	main_window_tex_format->Gloss = 2;
	main_window_tex_format->Bloss = 3;
	main_window_tex_format->Aloss = 8;

	VGAScreen = VGAScreenSeg = SDL_CreateRGBSurface(0, vga_width, vga_height, 8, 0, 0, 0, 0);
	VGAScreen2 = SDL_CreateRGBSurface(0, vga_width, vga_height, 8, 0, 0, 0, 0);
	game_screen = SDL_CreateRGBSurface(0, vga_width, vga_height, 8, 0, 0, 0, 0);

	if (VGAScreen == NULL || VGAScreen2 == NULL || game_screen == NULL)
	{
		_fprintf(stderr, "error: failed to create video surfaces: %s\n", SDL_GetError());
		exit(EXIT_FAILURE);
	}

	JE_clr256(VGAScreen);

	sync_scaling();
	init_scaler(scaler);
}

void deinit_video(void)
{
	SDL_FreeSurface(VGAScreenSeg);
	SDL_FreeSurface(VGAScreen2);
	SDL_FreeSurface(game_screen);
	VGAScreen = VGAScreenSeg = NULL;
	VGAScreen2 = NULL;
	game_screen = NULL;

	free(main_window_tex_format);
	main_window_tex_format = NULL;

	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

void video_on_win_resize(void)
{
}

void reinit_fullscreen(int new_display)
{
	(void)new_display;

	/* The PumpkinOS window manager owns the window; there is no
	   fullscreen mode to switch to. */
	fullscreen_display = -1;
}

void toggle_fullscreen(void)
{
	reinit_fullscreen(-1);
}

bool init_scaler(unsigned int new_scaler)
{
	if (new_scaler >= scalers_count)
		return false;

	scaler = new_scaler;
	return true;
}

bool set_scaling_mode_by_name(const char *name)
{
	for (int i = 0; i < ScalingMode_MAX; ++i)
	{
		if (strcmp(name, scaling_mode_names[i]) == 0)
		{
			scaling_mode = (ScalingMode)i;
			sync_scaling();
			return true;
		}
	}
	return false;
}

void JE_clr256(SDL_Surface *screen)
{
	SDL_FillRect(screen, NULL, 0);
}

void JE_showVGA(void)
{
	const Uint8 *src = (const Uint8 *)VGAScreen->pixels;
	Uint16 *dst = frame_buffer;

	for (int y = 0; y < vga_height; ++y)
	{
		const Uint8 *row = src + y * VGAScreen->pitch;
		for (int x = 0; x < vga_width; ++x)
			*dst++ = (Uint16)rgb_palette[row[x]];
	}

	sync_scaling();
	pk_present(frame_buffer);
}

/* The formulas below are the ones of the SDL2 video.c, with the output
   rectangle supplied by the PalmOS side. */

/** Maps a specified point in game screen coordinates to window coordinates. */
void mapScreenPointToWindow(Sint32 *const inout_x, Sint32 *const inout_y)
{
	int x0, y0, w, h;

	pk_get_layout(&x0, &y0, &w, &h);

	*inout_x = (2 * *inout_x + 1) * w / (2 * vga_width) + x0;
	*inout_y = (2 * *inout_y + 1) * h / (2 * vga_height) + y0;
}

/** Maps a specified point in window coordinates to game screen coordinates. */
void mapWindowPointToScreen(Sint32 *const inout_x, Sint32 *const inout_y)
{
	int x0, y0, w, h;

	pk_get_layout(&x0, &y0, &w, &h);

	*inout_x = (2 * (*inout_x - x0) + 1) * vga_width / (2 * w);
	*inout_y = (2 * (*inout_y - y0) + 1) * vga_height / (2 * h);
}

/** Scales a distance in window coordinates to game screen coordinates. */
void scaleWindowDistanceToScreen(Sint32 *const inout_x, Sint32 *const inout_y)
{
	int x0, y0, w, h;

	pk_get_layout(&x0, &y0, &w, &h);

	*inout_x = (2 * *inout_x + 1) * vga_width / (2 * w);
	*inout_y = (2 * *inout_y + 1) * vga_height / (2 * h);
}
