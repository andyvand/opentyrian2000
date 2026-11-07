/*
 * OpenTyrian 2000 PumpkinOS port: SDL2 compatibility header.
 *
 * Declares the subset of the SDL2 API used by the OpenTyrian sources.
 * The implementation lives in pumpkinos/sdl_shim.c and is backed by the
 * PumpkinOS (PalmOS) API through pumpkinos/pumpkin_app.c.
 *
 * Scancode values, key modifier bits and event type numbers are the real
 * SDL2 values, so that configuration files written by the SDL2 builds of
 * the game stay compatible.
 */
#ifndef PUMPKIN_SDL_H
#define PUMPKIN_SDL_H

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "SDL_types.h"
#include "SDL_endian.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- version ---------------------------------------------------------- */

#define SDL_MAJOR_VERSION 2
#define SDL_MINOR_VERSION 0
#define SDL_PATCHLEVEL    22

typedef struct SDL_version
{
	Uint8 major;
	Uint8 minor;
	Uint8 patch;
} SDL_version;

#define SDL_VERSION_ATLEAST(X, Y, Z) \
	((SDL_MAJOR_VERSION > (X)) || \
	 (SDL_MAJOR_VERSION == (X) && SDL_MINOR_VERSION > (Y)) || \
	 (SDL_MAJOR_VERSION == (X) && SDL_MINOR_VERSION == (Y) && SDL_PATCHLEVEL >= (Z)))

void SDL_GetVersion(SDL_version *ver);

/* ---- init / error / memory / strings ---------------------------------- */

#define SDL_INIT_TIMER    0x00000001u
#define SDL_INIT_AUDIO    0x00000010u
#define SDL_INIT_VIDEO    0x00000020u
#define SDL_INIT_JOYSTICK 0x00000200u
#define SDL_INIT_EVENTS   0x00004000u

int SDL_Init(Uint32 flags);
int SDL_InitSubSystem(Uint32 flags);
void SDL_QuitSubSystem(Uint32 flags);
Uint32 SDL_WasInit(Uint32 flags);
void SDL_Quit(void);

const char *SDL_GetError(void);
int SDL_SetError(const char *fmt, ...);

void *SDL_malloc(size_t size);
void *SDL_calloc(size_t nmemb, size_t size);
void SDL_free(void *mem);

size_t SDL_strlcpy(char *dst, const char *src, size_t maxlen);
size_t SDL_strlcat(char *dst, const char *src, size_t maxlen);

/* ---- timer ------------------------------------------------------------ */

Uint32 SDL_GetTicks(void);
void SDL_Delay(Uint32 ms);

/* ---- surfaces --------------------------------------------------------- */

typedef struct SDL_Rect
{
	int x, y;
	int w, h;
} SDL_Rect;

typedef struct SDL_Color
{
	Uint8 r;
	Uint8 g;
	Uint8 b;
	Uint8 a;
} SDL_Color;

typedef struct SDL_Palette
{
	int ncolors;
	SDL_Color *colors;
} SDL_Palette;

typedef struct SDL_PixelFormat
{
	Uint32 format;
	SDL_Palette *palette;
	Uint8 BitsPerPixel;
	Uint8 BytesPerPixel;
	Uint8 padding[2];
	Uint32 Rmask;
	Uint32 Gmask;
	Uint32 Bmask;
	Uint32 Amask;
	Uint8 Rloss;
	Uint8 Gloss;
	Uint8 Bloss;
	Uint8 Aloss;
	Uint8 Rshift;
	Uint8 Gshift;
	Uint8 Bshift;
	Uint8 Ashift;
} SDL_PixelFormat;

typedef struct SDL_Surface
{
	Uint32 flags;
	SDL_PixelFormat *format;
	int w, h;
	int pitch;
	void *pixels;
	void *userdata;
	int locked;
	void *lock_data;
	SDL_Rect clip_rect;
	void *map;
	int refcount;
} SDL_Surface;

#define SDL_SWSURFACE 0

#define SDL_PIXELFORMAT_UNKNOWN 0
#define SDL_PIXELFORMAT_INDEX8  1
#define SDL_PIXELFORMAT_RGB565  2

#define SDL_MUSTLOCK(S) 0

SDL_Surface *SDL_CreateRGBSurface(Uint32 flags, int width, int height, int depth,
                                  Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask);
void SDL_FreeSurface(SDL_Surface *surface);
int SDL_FillRect(SDL_Surface *dst, const SDL_Rect *rect, Uint32 color);
Uint32 SDL_MapRGB(const SDL_PixelFormat *format, Uint8 r, Uint8 g, Uint8 b);

typedef struct SDL_Window SDL_Window;
typedef struct SDL_Texture SDL_Texture;
typedef struct SDL_Renderer SDL_Renderer;

/* ---- keyboard --------------------------------------------------------- */

typedef enum
{
	SDL_SCANCODE_UNKNOWN = 0,

	SDL_SCANCODE_A = 4,
	SDL_SCANCODE_B = 5,
	SDL_SCANCODE_C = 6,
	SDL_SCANCODE_D = 7,
	SDL_SCANCODE_E = 8,
	SDL_SCANCODE_F = 9,
	SDL_SCANCODE_G = 10,
	SDL_SCANCODE_H = 11,
	SDL_SCANCODE_I = 12,
	SDL_SCANCODE_J = 13,
	SDL_SCANCODE_K = 14,
	SDL_SCANCODE_L = 15,
	SDL_SCANCODE_M = 16,
	SDL_SCANCODE_N = 17,
	SDL_SCANCODE_O = 18,
	SDL_SCANCODE_P = 19,
	SDL_SCANCODE_Q = 20,
	SDL_SCANCODE_R = 21,
	SDL_SCANCODE_S = 22,
	SDL_SCANCODE_T = 23,
	SDL_SCANCODE_U = 24,
	SDL_SCANCODE_V = 25,
	SDL_SCANCODE_W = 26,
	SDL_SCANCODE_X = 27,
	SDL_SCANCODE_Y = 28,
	SDL_SCANCODE_Z = 29,

	SDL_SCANCODE_1 = 30,
	SDL_SCANCODE_2 = 31,
	SDL_SCANCODE_3 = 32,
	SDL_SCANCODE_4 = 33,
	SDL_SCANCODE_5 = 34,
	SDL_SCANCODE_6 = 35,
	SDL_SCANCODE_7 = 36,
	SDL_SCANCODE_8 = 37,
	SDL_SCANCODE_9 = 38,
	SDL_SCANCODE_0 = 39,

	SDL_SCANCODE_RETURN = 40,
	SDL_SCANCODE_ESCAPE = 41,
	SDL_SCANCODE_BACKSPACE = 42,
	SDL_SCANCODE_TAB = 43,
	SDL_SCANCODE_SPACE = 44,

	SDL_SCANCODE_MINUS = 45,
	SDL_SCANCODE_EQUALS = 46,
	SDL_SCANCODE_LEFTBRACKET = 47,
	SDL_SCANCODE_RIGHTBRACKET = 48,
	SDL_SCANCODE_BACKSLASH = 49,
	SDL_SCANCODE_NONUSHASH = 50,
	SDL_SCANCODE_SEMICOLON = 51,
	SDL_SCANCODE_APOSTROPHE = 52,
	SDL_SCANCODE_GRAVE = 53,
	SDL_SCANCODE_COMMA = 54,
	SDL_SCANCODE_PERIOD = 55,
	SDL_SCANCODE_SLASH = 56,

	SDL_SCANCODE_CAPSLOCK = 57,

	SDL_SCANCODE_F1 = 58,
	SDL_SCANCODE_F2 = 59,
	SDL_SCANCODE_F3 = 60,
	SDL_SCANCODE_F4 = 61,
	SDL_SCANCODE_F5 = 62,
	SDL_SCANCODE_F6 = 63,
	SDL_SCANCODE_F7 = 64,
	SDL_SCANCODE_F8 = 65,
	SDL_SCANCODE_F9 = 66,
	SDL_SCANCODE_F10 = 67,
	SDL_SCANCODE_F11 = 68,
	SDL_SCANCODE_F12 = 69,

	SDL_SCANCODE_PRINTSCREEN = 70,
	SDL_SCANCODE_SCROLLLOCK = 71,
	SDL_SCANCODE_PAUSE = 72,
	SDL_SCANCODE_INSERT = 73,
	SDL_SCANCODE_HOME = 74,
	SDL_SCANCODE_PAGEUP = 75,
	SDL_SCANCODE_DELETE = 76,
	SDL_SCANCODE_END = 77,
	SDL_SCANCODE_PAGEDOWN = 78,
	SDL_SCANCODE_RIGHT = 79,
	SDL_SCANCODE_LEFT = 80,
	SDL_SCANCODE_DOWN = 81,
	SDL_SCANCODE_UP = 82,

	SDL_SCANCODE_NUMLOCKCLEAR = 83,
	SDL_SCANCODE_KP_DIVIDE = 84,
	SDL_SCANCODE_KP_MULTIPLY = 85,
	SDL_SCANCODE_KP_MINUS = 86,
	SDL_SCANCODE_KP_PLUS = 87,
	SDL_SCANCODE_KP_ENTER = 88,
	SDL_SCANCODE_KP_1 = 89,
	SDL_SCANCODE_KP_2 = 90,
	SDL_SCANCODE_KP_3 = 91,
	SDL_SCANCODE_KP_4 = 92,
	SDL_SCANCODE_KP_5 = 93,
	SDL_SCANCODE_KP_6 = 94,
	SDL_SCANCODE_KP_7 = 95,
	SDL_SCANCODE_KP_8 = 96,
	SDL_SCANCODE_KP_9 = 97,
	SDL_SCANCODE_KP_0 = 98,
	SDL_SCANCODE_KP_PERIOD = 99,

	SDL_SCANCODE_NONUSBACKSLASH = 100,
	SDL_SCANCODE_APPLICATION = 101,
	SDL_SCANCODE_POWER = 102,
	SDL_SCANCODE_KP_EQUALS = 103,

	SDL_SCANCODE_LCTRL = 224,
	SDL_SCANCODE_LSHIFT = 225,
	SDL_SCANCODE_LALT = 226,
	SDL_SCANCODE_LGUI = 227,
	SDL_SCANCODE_RCTRL = 228,
	SDL_SCANCODE_RSHIFT = 229,
	SDL_SCANCODE_RALT = 230,
	SDL_SCANCODE_RGUI = 231,

	SDL_NUM_SCANCODES = 512
} SDL_Scancode;

typedef Sint32 SDL_Keycode;

typedef enum
{
	KMOD_NONE = 0x0000,
	KMOD_LSHIFT = 0x0001,
	KMOD_RSHIFT = 0x0002,
	KMOD_LCTRL = 0x0040,
	KMOD_RCTRL = 0x0080,
	KMOD_LALT = 0x0100,
	KMOD_RALT = 0x0200,
	KMOD_LGUI = 0x0400,
	KMOD_RGUI = 0x0800,
	KMOD_NUM = 0x1000,
	KMOD_CAPS = 0x2000,
	KMOD_MODE = 0x4000,
	KMOD_RESERVED = 0x8000
} SDL_Keymod;

#define KMOD_CTRL  (KMOD_LCTRL | KMOD_RCTRL)
#define KMOD_SHIFT (KMOD_LSHIFT | KMOD_RSHIFT)
#define KMOD_ALT   (KMOD_LALT | KMOD_RALT)
#define KMOD_GUI   (KMOD_LGUI | KMOD_RGUI)
#define KMOD_META  KMOD_GUI

typedef struct SDL_Keysym
{
	SDL_Scancode scancode;
	SDL_Keycode sym;
	Uint16 mod;
	Uint32 unused;
} SDL_Keysym;

SDL_Keymod SDL_GetModState(void);
const char *SDL_GetScancodeName(SDL_Scancode scancode);
SDL_Scancode SDL_GetScancodeFromName(const char *name);

/* ---- events ----------------------------------------------------------- */

#define SDL_RELEASED 0
#define SDL_PRESSED  1

typedef enum
{
	SDL_FIRSTEVENT = 0,
	SDL_QUIT = 0x100,
	SDL_WINDOWEVENT = 0x200,
	SDL_KEYDOWN = 0x300,
	SDL_KEYUP = 0x301,
	SDL_TEXTEDITING = 0x302,
	SDL_TEXTINPUT = 0x303,
	SDL_MOUSEMOTION = 0x400,
	SDL_MOUSEBUTTONDOWN = 0x401,
	SDL_MOUSEBUTTONUP = 0x402,
	SDL_LASTEVENT = 0xFFFF
} SDL_EventType;

typedef enum
{
	SDL_WINDOWEVENT_NONE = 0,
	SDL_WINDOWEVENT_SHOWN = 1,
	SDL_WINDOWEVENT_HIDDEN = 2,
	SDL_WINDOWEVENT_EXPOSED = 3,
	SDL_WINDOWEVENT_MOVED = 4,
	SDL_WINDOWEVENT_RESIZED = 5,
	SDL_WINDOWEVENT_SIZE_CHANGED = 6,
	SDL_WINDOWEVENT_MINIMIZED = 7,
	SDL_WINDOWEVENT_MAXIMIZED = 8,
	SDL_WINDOWEVENT_RESTORED = 9,
	SDL_WINDOWEVENT_ENTER = 10,
	SDL_WINDOWEVENT_LEAVE = 11,
	SDL_WINDOWEVENT_FOCUS_GAINED = 12,
	SDL_WINDOWEVENT_FOCUS_LOST = 13,
	SDL_WINDOWEVENT_CLOSE = 14
} SDL_WindowEventID;

#define SDL_BUTTON_LEFT   1
#define SDL_BUTTON_MIDDLE 2
#define SDL_BUTTON_RIGHT  3

#define SDL_TEXTINPUTEVENT_TEXT_SIZE 32

typedef struct SDL_CommonEvent
{
	Uint32 type;
	Uint32 timestamp;
} SDL_CommonEvent;

typedef struct SDL_WindowEvent
{
	Uint32 type;
	Uint32 timestamp;
	Uint32 windowID;
	Uint8 event;
	Uint8 padding1;
	Uint8 padding2;
	Uint8 padding3;
	Sint32 data1;
	Sint32 data2;
} SDL_WindowEvent;

typedef struct SDL_KeyboardEvent
{
	Uint32 type;
	Uint32 timestamp;
	Uint32 windowID;
	Uint8 state;
	Uint8 repeat;
	Uint8 padding2;
	Uint8 padding3;
	SDL_Keysym keysym;
} SDL_KeyboardEvent;

typedef struct SDL_TextEditingEvent
{
	Uint32 type;
	Uint32 timestamp;
	Uint32 windowID;
	char text[SDL_TEXTINPUTEVENT_TEXT_SIZE];
	Sint32 start;
	Sint32 length;
} SDL_TextEditingEvent;

typedef struct SDL_TextInputEvent
{
	Uint32 type;
	Uint32 timestamp;
	Uint32 windowID;
	char text[SDL_TEXTINPUTEVENT_TEXT_SIZE];
} SDL_TextInputEvent;

typedef struct SDL_MouseMotionEvent
{
	Uint32 type;
	Uint32 timestamp;
	Uint32 windowID;
	Uint32 which;
	Uint32 state;
	Sint32 x;
	Sint32 y;
	Sint32 xrel;
	Sint32 yrel;
} SDL_MouseMotionEvent;

typedef struct SDL_MouseButtonEvent
{
	Uint32 type;
	Uint32 timestamp;
	Uint32 windowID;
	Uint32 which;
	Uint8 button;
	Uint8 state;
	Uint8 clicks;
	Uint8 padding1;
	Sint32 x;
	Sint32 y;
} SDL_MouseButtonEvent;

typedef struct SDL_QuitEvent
{
	Uint32 type;
	Uint32 timestamp;
} SDL_QuitEvent;

typedef union SDL_Event
{
	Uint32 type;
	SDL_CommonEvent common;
	SDL_WindowEvent window;
	SDL_KeyboardEvent key;
	SDL_TextEditingEvent edit;
	SDL_TextInputEvent text;
	SDL_MouseMotionEvent motion;
	SDL_MouseButtonEvent button;
	SDL_QuitEvent quit;
	Uint8 padding[64];
} SDL_Event;

int SDL_PollEvent(SDL_Event *event);
int SDL_PushEvent(SDL_Event *event);

#define SDL_QUERY   -1
#define SDL_IGNORE   0
#define SDL_DISABLE  0
#define SDL_ENABLE   1

int SDL_ShowCursor(int toggle);
int SDL_SetRelativeMouseMode(SDL_bool enabled);
SDL_bool SDL_SetHint(const char *name, const char *value);
SDL_Window *SDL_GetWindowFromID(Uint32 id);
int SDL_GetNumVideoDisplays(void);

#define SDL_HINT_MOUSE_RELATIVE_SYSTEM_SCALE "SDL_MOUSE_RELATIVE_SYSTEM_SCALE"

/* ---- joystick (no joysticks on PumpkinOS: stubs) ----------------------- */

typedef struct SDL_Joystick SDL_Joystick;
typedef Sint32 SDL_JoystickID;

#define SDL_HAT_CENTERED 0x00
#define SDL_HAT_UP       0x01
#define SDL_HAT_RIGHT    0x02
#define SDL_HAT_DOWN     0x04
#define SDL_HAT_LEFT     0x08

int SDL_NumJoysticks(void);
SDL_Joystick *SDL_JoystickOpen(int device_index);
void SDL_JoystickClose(SDL_Joystick *joystick);
const char *SDL_JoystickName(SDL_Joystick *joystick);
int SDL_JoystickNumAxes(SDL_Joystick *joystick);
int SDL_JoystickNumButtons(SDL_Joystick *joystick);
int SDL_JoystickNumHats(SDL_Joystick *joystick);
Sint16 SDL_JoystickGetAxis(SDL_Joystick *joystick, int axis);
Uint8 SDL_JoystickGetButton(SDL_Joystick *joystick, int button);
Uint8 SDL_JoystickGetHat(SDL_Joystick *joystick, int hat);
void SDL_JoystickUpdate(void);
int SDL_JoystickEventState(int state);

/* ---- audio ------------------------------------------------------------ */

typedef Uint16 SDL_AudioFormat;

#define AUDIO_U8     0x0008
#define AUDIO_S8     0x8008
#define AUDIO_U16LSB 0x0010
#define AUDIO_S16LSB 0x8010
#define AUDIO_U16MSB 0x1010
#define AUDIO_S16MSB 0x9010
#define AUDIO_U16    AUDIO_U16LSB
#define AUDIO_S16    AUDIO_S16LSB

#if SDL_BYTEORDER == SDL_LIL_ENDIAN
#define AUDIO_U16SYS AUDIO_U16LSB
#define AUDIO_S16SYS AUDIO_S16LSB
#else
#define AUDIO_U16SYS AUDIO_U16MSB
#define AUDIO_S16SYS AUDIO_S16MSB
#endif

#define SDL_AUDIO_BITSIZE(x) ((x) & 0xFF)

#define SDL_AUDIO_ALLOW_FREQUENCY_CHANGE 0x00000001
#define SDL_AUDIO_ALLOW_FORMAT_CHANGE    0x00000002
#define SDL_AUDIO_ALLOW_CHANNELS_CHANGE  0x00000004
#define SDL_AUDIO_ALLOW_SAMPLES_CHANGE   0x00000008
#define SDL_AUDIO_ALLOW_ANY_CHANGE       0x0000000F

typedef void (*SDL_AudioCallback)(void *userdata, Uint8 *stream, int len);

typedef struct SDL_AudioSpec
{
	int freq;
	SDL_AudioFormat format;
	Uint8 channels;
	Uint8 silence;
	Uint16 samples;
	Uint16 padding;
	Uint32 size;
	SDL_AudioCallback callback;
	void *userdata;
} SDL_AudioSpec;

typedef Uint32 SDL_AudioDeviceID;

SDL_AudioDeviceID SDL_OpenAudioDevice(const char *device, int iscapture,
                                      const SDL_AudioSpec *desired, SDL_AudioSpec *obtained,
                                      int allowed_changes);
void SDL_CloseAudioDevice(SDL_AudioDeviceID dev);
void SDL_PauseAudioDevice(SDL_AudioDeviceID dev, int pause_on);
void SDL_LockAudioDevice(SDL_AudioDeviceID dev);
void SDL_UnlockAudioDevice(SDL_AudioDeviceID dev);

typedef struct SDL_AudioCVT
{
	int needed;
	SDL_AudioFormat src_format;
	SDL_AudioFormat dst_format;
	double rate_incr;
	Uint8 *buf;
	int len;
	int len_cvt;
	int len_mult;
	double len_ratio;
} SDL_AudioCVT;

int SDL_BuildAudioCVT(SDL_AudioCVT *cvt,
                      SDL_AudioFormat src_format, Uint8 src_channels, int src_rate,
                      SDL_AudioFormat dst_format, Uint8 dst_channels, int dst_rate);
int SDL_ConvertAudio(SDL_AudioCVT *cvt);

#ifdef __cplusplus
}
#endif

#endif /* PUMPKIN_SDL_H */
