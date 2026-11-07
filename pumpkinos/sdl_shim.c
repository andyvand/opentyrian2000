/*
 * OpenTyrian 2000 PumpkinOS port: SDL2 compatibility layer.
 *
 * Implements the subset of SDL2 declared in SDL2/SDL.h on top of the
 * PalmOS side of the port (pumpkin_app.c, reached through pumpkin_glue.h).
 *
 *  - Events: the PalmOS key state (hardware key bits, modifier bits and the
 *    128-bit ASCII bitmap reported by pumpkin_status()) and the pointer
 *    position/buttons are polled and turned into SDL key, text input and
 *    mouse events by comparing them with the previous poll.
 *  - Timing: SDL_Delay() keeps the PalmOS event loop running.
 *  - Audio: SDL_OpenAudioDevice() maps onto a PalmOS SndStream; the game's
 *    pull callback is invoked from the stream callback.
 *  - Surfaces: only 8 bpp software surfaces are needed.
 *  - Joysticks: none are available, all calls are stubs.
 */
#include "SDL2/SDL.h"
#include "pumpkin_glue.h"

#include <stdarg.h>
#include <math.h>

/* ---- error / version / init ------------------------------------------- */

static char error_buffer[256];
static Uint32 initialized_subsystems;
static int64_t ticks_origin;

static int shim_started;

void SDL_GetVersion(SDL_version *ver)
{
	if (ver != NULL)
	{
		ver->major = SDL_MAJOR_VERSION;
		ver->minor = SDL_MINOR_VERSION;
		ver->patch = SDL_PATCHLEVEL;
	}
}

const char *SDL_GetError(void)
{
	return error_buffer;
}

int SDL_SetError(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(error_buffer, sizeof(error_buffer), fmt, ap);
	va_end(ap);

	pk_log("SDL error: %s", error_buffer);

	return -1;
}

static void shim_start(void)
{
	if (!shim_started)
	{
		ticks_origin = pk_clock_us();
		shim_started = 1;
	}
}

int SDL_Init(Uint32 flags)
{
	shim_start();
	initialized_subsystems |= flags;
	return 0;
}

int SDL_InitSubSystem(Uint32 flags)
{
	shim_start();
	initialized_subsystems |= flags;
	return 0;
}

void SDL_QuitSubSystem(Uint32 flags)
{
	initialized_subsystems &= ~flags;
}

Uint32 SDL_WasInit(Uint32 flags)
{
	if (flags == 0)
		return initialized_subsystems;
	return initialized_subsystems & flags;
}

void SDL_Quit(void)
{
	initialized_subsystems = 0;
}

void *SDL_malloc(size_t size)
{
	return malloc(size);
}

void *SDL_calloc(size_t nmemb, size_t size)
{
	return calloc(nmemb, size);
}

void SDL_free(void *mem)
{
	free(mem);
}

size_t SDL_strlcpy(char *dst, const char *src, size_t maxlen)
{
	size_t srclen = strlen(src);

	if (maxlen > 0)
	{
		size_t len = srclen < maxlen - 1 ? srclen : maxlen - 1;
		memcpy(dst, src, len);
		dst[len] = '\0';
	}

	return srclen;
}

size_t SDL_strlcat(char *dst, const char *src, size_t maxlen)
{
	size_t dstlen = strlen(dst);
	size_t srclen = strlen(src);

	if (dstlen < maxlen)
		SDL_strlcpy(dst + dstlen, src, maxlen - dstlen);

	return dstlen + srclen;
}

/* ---- exit -------------------------------------------------------------- */

void tyr_exit(int code)
{
	pk_exit(code);
}

/* ---- timer ------------------------------------------------------------- */

Uint32 SDL_GetTicks(void)
{
	shim_start();
	return (Uint32)((pk_clock_us() - ticks_origin) / 1000);
}

void SDL_Delay(Uint32 ms)
{
	int64_t deadline = pk_clock_us() + (int64_t)ms * 1000;

	for (; ; )
	{
		int64_t now = pk_clock_us();
		if (now >= deadline)
			break;

		if (!pk_pump((uint32_t)(deadline - now)))
		{
			/* The application is being closed. Nothing in the game may keep
			   running; unwind to PilotMain right away. */
			tyr_exit(0);
		}
	}
}

/* ---- surfaces ---------------------------------------------------------- */

SDL_Surface *SDL_CreateRGBSurface(Uint32 flags, int width, int height, int depth,
                                  Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask)
{
	SDL_Surface *surface;
	SDL_PixelFormat *format;
	int bytes_per_pixel = (depth + 7) / 8;

	(void)flags;

	if (width <= 0 || height <= 0 || bytes_per_pixel <= 0)
	{
		SDL_SetError("invalid surface size");
		return NULL;
	}

	surface = calloc(1, sizeof(*surface));
	format = calloc(1, sizeof(*format));
	if (surface == NULL || format == NULL)
	{
		free(surface);
		free(format);
		SDL_SetError("out of memory");
		return NULL;
	}

	format->format = depth == 8 ? SDL_PIXELFORMAT_INDEX8 : SDL_PIXELFORMAT_UNKNOWN;
	format->BitsPerPixel = (Uint8)depth;
	format->BytesPerPixel = (Uint8)bytes_per_pixel;
	format->Rmask = Rmask;
	format->Gmask = Gmask;
	format->Bmask = Bmask;
	format->Amask = Amask;

	surface->format = format;
	surface->w = width;
	surface->h = height;
	surface->pitch = width * bytes_per_pixel;
	surface->pixels = calloc((size_t)surface->pitch, (size_t)height);
	surface->clip_rect.w = width;
	surface->clip_rect.h = height;
	surface->refcount = 1;

	if (surface->pixels == NULL)
	{
		free(format);
		free(surface);
		SDL_SetError("out of memory");
		return NULL;
	}

	return surface;
}

void SDL_FreeSurface(SDL_Surface *surface)
{
	if (surface != NULL)
	{
		free(surface->pixels);
		free(surface->format);
		free(surface);
	}
}

int SDL_FillRect(SDL_Surface *dst, const SDL_Rect *rect, Uint32 color)
{
	int x, y, w, h;

	if (dst == NULL || dst->pixels == NULL)
		return SDL_SetError("invalid surface");

	if (rect != NULL)
	{
		x = rect->x;
		y = rect->y;
		w = rect->w;
		h = rect->h;
	}
	else
	{
		x = 0;
		y = 0;
		w = dst->w;
		h = dst->h;
	}

	if (x < 0)
	{
		w += x;
		x = 0;
	}
	if (y < 0)
	{
		h += y;
		y = 0;
	}
	if (x + w > dst->w)
		w = dst->w - x;
	if (y + h > dst->h)
		h = dst->h - y;

	if (w <= 0 || h <= 0)
		return 0;

	if (dst->format->BytesPerPixel == 1)
	{
		Uint8 *row = (Uint8 *)dst->pixels + y * dst->pitch + x;
		for (int j = 0; j < h; ++j, row += dst->pitch)
			memset(row, (int)(color & 0xFF), (size_t)w);
	}
	else if (dst->format->BytesPerPixel == 2)
	{
		Uint8 *row = (Uint8 *)dst->pixels + y * dst->pitch + x * 2;
		for (int j = 0; j < h; ++j, row += dst->pitch)
		{
			Uint16 *p = (Uint16 *)row;
			for (int i = 0; i < w; ++i)
				p[i] = (Uint16)color;
		}
	}
	else
	{
		Uint8 *row = (Uint8 *)dst->pixels + y * dst->pitch + x * 4;
		for (int j = 0; j < h; ++j, row += dst->pitch)
		{
			Uint32 *p = (Uint32 *)row;
			for (int i = 0; i < w; ++i)
				p[i] = color;
		}
	}

	return 0;
}

Uint32 SDL_MapRGB(const SDL_PixelFormat *format, Uint8 r, Uint8 g, Uint8 b)
{
	if (format == NULL || format->BitsPerPixel == 8)
		return 0;

	return ((Uint32)(r >> format->Rloss) << format->Rshift) |
	       ((Uint32)(g >> format->Gloss) << format->Gshift) |
	       ((Uint32)(b >> format->Bloss) << format->Bshift);
}

/* ---- event queue ------------------------------------------------------- */

#define EVENT_QUEUE_SIZE 256

static SDL_Event event_queue[EVENT_QUEUE_SIZE];
static unsigned int event_queue_read, event_queue_write;

static Uint32 event_timestamp(void)
{
	return SDL_GetTicks();
}

int SDL_PushEvent(SDL_Event *event)
{
	if (event == NULL)
		return -1;

	if (event_queue_write - event_queue_read >= EVENT_QUEUE_SIZE)
		return 0;  /* queue full: drop */

	event->common.timestamp = event_timestamp();
	event_queue[event_queue_write % EVENT_QUEUE_SIZE] = *event;
	event_queue_write++;

	return 1;
}

static void push_key_event(int down, SDL_Scancode scancode, SDL_Keycode sym, Uint16 mod)
{
	SDL_Event ev;

	memset(&ev, 0, sizeof(ev));
	ev.type = down ? SDL_KEYDOWN : SDL_KEYUP;
	ev.key.state = down ? SDL_PRESSED : SDL_RELEASED;
	ev.key.keysym.scancode = scancode;
	ev.key.keysym.sym = sym;
	ev.key.keysym.mod = mod;
	SDL_PushEvent(&ev);
}

static void push_text_event(char c)
{
	SDL_Event ev;

	memset(&ev, 0, sizeof(ev));
	ev.type = SDL_TEXTINPUT;
	ev.text.text[0] = c;
	ev.text.text[1] = '\0';
	SDL_PushEvent(&ev);
}

static void push_window_event(Uint8 id)
{
	SDL_Event ev;

	memset(&ev, 0, sizeof(ev));
	ev.type = SDL_WINDOWEVENT;
	ev.window.event = id;
	SDL_PushEvent(&ev);
}

/* ---- key translation --------------------------------------------------- */

/* Maps an ASCII code delivered by PumpkinOS to the SDL scancode of the key
   that produces it on a US keyboard. Shifted and unshifted characters map
   to the same scancode, so key up events match key down events even when
   the shift state changed in between. */
static SDL_Scancode ascii_to_scancode(int c)
{
	if (c >= 'a' && c <= 'z')
		return (SDL_Scancode)(SDL_SCANCODE_A + (c - 'a'));
	if (c >= 'A' && c <= 'Z')
		return (SDL_Scancode)(SDL_SCANCODE_A + (c - 'A'));
	if (c >= '1' && c <= '9')
		return (SDL_Scancode)(SDL_SCANCODE_1 + (c - '1'));

	switch (c)
	{
	case '0': return SDL_SCANCODE_0;
	case ')': return SDL_SCANCODE_0;
	case '!': return SDL_SCANCODE_1;
	case '@': return SDL_SCANCODE_2;
	case '#': return SDL_SCANCODE_3;
	case '$': return SDL_SCANCODE_4;
	case '%': return SDL_SCANCODE_5;
	case '^': return SDL_SCANCODE_6;
	case '&': return SDL_SCANCODE_7;
	case '*': return SDL_SCANCODE_8;
	case '(': return SDL_SCANCODE_9;

	case '\r': return SDL_SCANCODE_RETURN;
	case '\n': return SDL_SCANCODE_RETURN;
	case 27:   return SDL_SCANCODE_ESCAPE;
	case '\b': return SDL_SCANCODE_BACKSPACE;
	case 127:  return SDL_SCANCODE_DELETE;
	case '\t': return SDL_SCANCODE_TAB;
	case ' ':  return SDL_SCANCODE_SPACE;

	case '-': case '_':  return SDL_SCANCODE_MINUS;
	case '=': case '+':  return SDL_SCANCODE_EQUALS;
	case '[': case '{':  return SDL_SCANCODE_LEFTBRACKET;
	case ']': case '}':  return SDL_SCANCODE_RIGHTBRACKET;
	case '\\': case '|': return SDL_SCANCODE_BACKSLASH;
	case ';': case ':':  return SDL_SCANCODE_SEMICOLON;
	case '\'': case '"': return SDL_SCANCODE_APOSTROPHE;
	case '`': case '~':  return SDL_SCANCODE_GRAVE;
	case ',': case '<':  return SDL_SCANCODE_COMMA;
	case '.': case '>':  return SDL_SCANCODE_PERIOD;
	case '/': case '?':  return SDL_SCANCODE_SLASH;
	default:
		return SDL_SCANCODE_UNKNOWN;
	}
}

static const struct
{
	uint32_t bit;
	SDL_Scancode scancode;
} special_keys[] = {
	{ PK_KEY_UP,    SDL_SCANCODE_UP },
	{ PK_KEY_DOWN,  SDL_SCANCODE_DOWN },
	{ PK_KEY_LEFT,  SDL_SCANCODE_LEFT },
	{ PK_KEY_RIGHT, SDL_SCANCODE_RIGHT },
	{ PK_KEY_F1,    SDL_SCANCODE_F1 },
	{ PK_KEY_F2,    SDL_SCANCODE_F2 },
	{ PK_KEY_F3,    SDL_SCANCODE_F3 },
	{ PK_KEY_F4,    SDL_SCANCODE_F4 },
	{ PK_KEY_END,   SDL_SCANCODE_END },
};

static const struct
{
	uint32_t bit;
	SDL_Scancode scancode;
	Uint16 kmod;
} modifier_keys[] = {
	{ PK_MOD_SHIFT, SDL_SCANCODE_LSHIFT, KMOD_LSHIFT },
	{ PK_MOD_CTRL,  SDL_SCANCODE_LCTRL,  KMOD_LCTRL },
	{ PK_MOD_RCTRL, SDL_SCANCODE_RCTRL,  KMOD_RCTRL },
	{ PK_MOD_LALT,  SDL_SCANCODE_LALT,   KMOD_LALT },
	{ PK_MOD_RALT,  SDL_SCANCODE_RALT,   KMOD_RALT },
};

static Uint16 mods_to_kmod(uint32_t mods)
{
	Uint16 kmod = KMOD_NONE;

	for (size_t i = 0; i < sizeof(modifier_keys) / sizeof(modifier_keys[0]); ++i)
		if (mods & modifier_keys[i].bit)
			kmod |= modifier_keys[i].kmod;

	return kmod;
}

/* ---- input polling ----------------------------------------------------- */

static pk_input_state prev_input;
static int input_started;
static int quit_event_sent;
static Uint16 current_kmod;

/* scancodes currently reported as pressed, so that key up events are only
   generated for keys that were seen going down */
static Uint8 scancode_down[SDL_NUM_SCANCODES];

static void set_scancode(SDL_Scancode scancode, int down, SDL_Keycode sym, Uint16 kmod, int text)
{
	if (scancode == SDL_SCANCODE_UNKNOWN || scancode >= SDL_NUM_SCANCODES)
		return;

	if (down)
	{
		if (!scancode_down[scancode])
		{
			scancode_down[scancode] = 1;
			push_key_event(1, scancode, sym, kmod);
			if (text && sym >= 32 && sym < 127)
				push_text_event((char)sym);
		}
	}
	else
	{
		if (scancode_down[scancode])
		{
			scancode_down[scancode] = 0;
			push_key_event(0, scancode, sym, kmod);
		}
	}
}

static void poll_ascii_keys(uint64_t old, uint64_t cur, int offset, Uint16 kmod)
{
	uint64_t diff = old ^ cur;

	for (int i = 0; diff != 0 && i < 64; ++i, diff >>= 1, cur >>= 1)
	{
		if (diff & 1)
		{
			int c = offset + i;
			set_scancode(ascii_to_scancode(c), (int)(cur & 1), c, kmod, 1);
		}
	}
}

static void poll_platform(void)
{
	pk_input_state st;
	Uint16 kmod;

	pk_poll_input(&st);

	if (!input_started)
	{
		input_started = 1;
		memset(&prev_input, 0, sizeof(prev_input));
		prev_input.mouse_x = st.mouse_x;
		prev_input.mouse_y = st.mouse_y;
		prev_input.focused = -1;
	}

	if (st.focused != prev_input.focused)
		push_window_event(st.focused ? SDL_WINDOWEVENT_FOCUS_GAINED : SDL_WINDOWEVENT_FOCUS_LOST);

	/* modifiers first, so that the key events that follow carry them */
	kmod = mods_to_kmod(st.mods);
	current_kmod = kmod;
	for (size_t i = 0; i < sizeof(modifier_keys) / sizeof(modifier_keys[0]); ++i)
	{
		uint32_t bit = modifier_keys[i].bit;
		if ((st.mods & bit) != (prev_input.mods & bit))
			set_scancode(modifier_keys[i].scancode, (st.mods & bit) != 0, 0, kmod, 0);
	}

	for (size_t i = 0; i < sizeof(special_keys) / sizeof(special_keys[0]); ++i)
	{
		uint32_t bit = special_keys[i].bit;
		if ((st.keys & bit) != (prev_input.keys & bit))
			set_scancode(special_keys[i].scancode, (st.keys & bit) != 0, 0, kmod, 0);
	}

	poll_ascii_keys(prev_input.ascii[0], st.ascii[0], 0, kmod);
	poll_ascii_keys(prev_input.ascii[1], st.ascii[1], 64, kmod);

	/* mouse */
	if (st.mouse_x != prev_input.mouse_x || st.mouse_y != prev_input.mouse_y)
	{
		SDL_Event ev;

		memset(&ev, 0, sizeof(ev));
		ev.type = SDL_MOUSEMOTION;
		ev.motion.x = st.mouse_x;
		ev.motion.y = st.mouse_y;
		ev.motion.xrel = st.mouse_x - prev_input.mouse_x;
		ev.motion.yrel = st.mouse_y - prev_input.mouse_y;
		ev.motion.state = ((st.buttons & PK_BUTTON_LEFT) ? 1u : 0u) | ((st.buttons & PK_BUTTON_RIGHT) ? 4u : 0u);
		SDL_PushEvent(&ev);
	}

	if (st.buttons != prev_input.buttons)
	{
		static const struct { uint32_t bit; Uint8 button; } buttons[] = {
			{ PK_BUTTON_LEFT, SDL_BUTTON_LEFT },
			{ PK_BUTTON_RIGHT, SDL_BUTTON_RIGHT },
		};

		for (size_t i = 0; i < sizeof(buttons) / sizeof(buttons[0]); ++i)
		{
			uint32_t bit = buttons[i].bit;
			if ((st.buttons & bit) != (prev_input.buttons & bit))
			{
				SDL_Event ev;
				int down = (st.buttons & bit) != 0;

				memset(&ev, 0, sizeof(ev));
				ev.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
				ev.button.button = buttons[i].button;
				ev.button.state = down ? SDL_PRESSED : SDL_RELEASED;
				ev.button.clicks = 1;
				ev.button.x = st.mouse_x;
				ev.button.y = st.mouse_y;
				SDL_PushEvent(&ev);
			}
		}
	}

	prev_input = st;

	if (pk_quit_requested() && !quit_event_sent)
	{
		SDL_Event ev;

		memset(&ev, 0, sizeof(ev));
		ev.type = SDL_QUIT;
		SDL_PushEvent(&ev);
		quit_event_sent = 1;
	}
}

int SDL_PollEvent(SDL_Event *event)
{
	if (event_queue_read == event_queue_write)
		poll_platform();

	if (event_queue_read == event_queue_write)
		return 0;

	if (event != NULL)
		*event = event_queue[event_queue_read % EVENT_QUEUE_SIZE];
	event_queue_read++;

	return 1;
}

SDL_Keymod SDL_GetModState(void)
{
	return (SDL_Keymod)current_kmod;
}

int SDL_ShowCursor(int toggle)
{
	(void)toggle;
	return SDL_ENABLE;
}

int SDL_SetRelativeMouseMode(SDL_bool enabled)
{
	(void)enabled;
	return 0;
}

SDL_bool SDL_SetHint(const char *name, const char *value)
{
	(void)name;
	(void)value;
	return SDL_TRUE;
}

SDL_Window *SDL_GetWindowFromID(Uint32 id)
{
	(void)id;
	return NULL;
}

int SDL_GetNumVideoDisplays(void)
{
	/* the PumpkinOS window manager owns the display: no fullscreen displays */
	return 0;
}

/* ---- scancode names ---------------------------------------------------- */

static const struct
{
	SDL_Scancode scancode;
	const char *name;
} scancode_names[] = {
	{ SDL_SCANCODE_A, "A" }, { SDL_SCANCODE_B, "B" }, { SDL_SCANCODE_C, "C" },
	{ SDL_SCANCODE_D, "D" }, { SDL_SCANCODE_E, "E" }, { SDL_SCANCODE_F, "F" },
	{ SDL_SCANCODE_G, "G" }, { SDL_SCANCODE_H, "H" }, { SDL_SCANCODE_I, "I" },
	{ SDL_SCANCODE_J, "J" }, { SDL_SCANCODE_K, "K" }, { SDL_SCANCODE_L, "L" },
	{ SDL_SCANCODE_M, "M" }, { SDL_SCANCODE_N, "N" }, { SDL_SCANCODE_O, "O" },
	{ SDL_SCANCODE_P, "P" }, { SDL_SCANCODE_Q, "Q" }, { SDL_SCANCODE_R, "R" },
	{ SDL_SCANCODE_S, "S" }, { SDL_SCANCODE_T, "T" }, { SDL_SCANCODE_U, "U" },
	{ SDL_SCANCODE_V, "V" }, { SDL_SCANCODE_W, "W" }, { SDL_SCANCODE_X, "X" },
	{ SDL_SCANCODE_Y, "Y" }, { SDL_SCANCODE_Z, "Z" },
	{ SDL_SCANCODE_1, "1" }, { SDL_SCANCODE_2, "2" }, { SDL_SCANCODE_3, "3" },
	{ SDL_SCANCODE_4, "4" }, { SDL_SCANCODE_5, "5" }, { SDL_SCANCODE_6, "6" },
	{ SDL_SCANCODE_7, "7" }, { SDL_SCANCODE_8, "8" }, { SDL_SCANCODE_9, "9" },
	{ SDL_SCANCODE_0, "0" },
	{ SDL_SCANCODE_RETURN, "Return" },
	{ SDL_SCANCODE_ESCAPE, "Escape" },
	{ SDL_SCANCODE_BACKSPACE, "Backspace" },
	{ SDL_SCANCODE_TAB, "Tab" },
	{ SDL_SCANCODE_SPACE, "Space" },
	{ SDL_SCANCODE_MINUS, "-" },
	{ SDL_SCANCODE_EQUALS, "=" },
	{ SDL_SCANCODE_LEFTBRACKET, "[" },
	{ SDL_SCANCODE_RIGHTBRACKET, "]" },
	{ SDL_SCANCODE_BACKSLASH, "\\" },
	{ SDL_SCANCODE_NONUSHASH, "#" },
	{ SDL_SCANCODE_SEMICOLON, ";" },
	{ SDL_SCANCODE_APOSTROPHE, "'" },
	{ SDL_SCANCODE_GRAVE, "`" },
	{ SDL_SCANCODE_COMMA, "," },
	{ SDL_SCANCODE_PERIOD, "." },
	{ SDL_SCANCODE_SLASH, "/" },
	{ SDL_SCANCODE_CAPSLOCK, "CapsLock" },
	{ SDL_SCANCODE_F1, "F1" }, { SDL_SCANCODE_F2, "F2" }, { SDL_SCANCODE_F3, "F3" },
	{ SDL_SCANCODE_F4, "F4" }, { SDL_SCANCODE_F5, "F5" }, { SDL_SCANCODE_F6, "F6" },
	{ SDL_SCANCODE_F7, "F7" }, { SDL_SCANCODE_F8, "F8" }, { SDL_SCANCODE_F9, "F9" },
	{ SDL_SCANCODE_F10, "F10" }, { SDL_SCANCODE_F11, "F11" }, { SDL_SCANCODE_F12, "F12" },
	{ SDL_SCANCODE_PRINTSCREEN, "PrintScreen" },
	{ SDL_SCANCODE_SCROLLLOCK, "ScrollLock" },
	{ SDL_SCANCODE_PAUSE, "Pause" },
	{ SDL_SCANCODE_INSERT, "Insert" },
	{ SDL_SCANCODE_HOME, "Home" },
	{ SDL_SCANCODE_PAGEUP, "PageUp" },
	{ SDL_SCANCODE_DELETE, "Delete" },
	{ SDL_SCANCODE_END, "End" },
	{ SDL_SCANCODE_PAGEDOWN, "PageDown" },
	{ SDL_SCANCODE_RIGHT, "Right" },
	{ SDL_SCANCODE_LEFT, "Left" },
	{ SDL_SCANCODE_DOWN, "Down" },
	{ SDL_SCANCODE_UP, "Up" },
	{ SDL_SCANCODE_NUMLOCKCLEAR, "Numlock" },
	{ SDL_SCANCODE_KP_DIVIDE, "Keypad /" },
	{ SDL_SCANCODE_KP_MULTIPLY, "Keypad *" },
	{ SDL_SCANCODE_KP_MINUS, "Keypad -" },
	{ SDL_SCANCODE_KP_PLUS, "Keypad +" },
	{ SDL_SCANCODE_KP_ENTER, "Keypad Enter" },
	{ SDL_SCANCODE_KP_1, "Keypad 1" }, { SDL_SCANCODE_KP_2, "Keypad 2" },
	{ SDL_SCANCODE_KP_3, "Keypad 3" }, { SDL_SCANCODE_KP_4, "Keypad 4" },
	{ SDL_SCANCODE_KP_5, "Keypad 5" }, { SDL_SCANCODE_KP_6, "Keypad 6" },
	{ SDL_SCANCODE_KP_7, "Keypad 7" }, { SDL_SCANCODE_KP_8, "Keypad 8" },
	{ SDL_SCANCODE_KP_9, "Keypad 9" }, { SDL_SCANCODE_KP_0, "Keypad 0" },
	{ SDL_SCANCODE_KP_PERIOD, "Keypad ." },
	{ SDL_SCANCODE_LCTRL, "Left Ctrl" },
	{ SDL_SCANCODE_LSHIFT, "Left Shift" },
	{ SDL_SCANCODE_LALT, "Left Alt" },
	{ SDL_SCANCODE_LGUI, "Left GUI" },
	{ SDL_SCANCODE_RCTRL, "Right Ctrl" },
	{ SDL_SCANCODE_RSHIFT, "Right Shift" },
	{ SDL_SCANCODE_RALT, "Right Alt" },
	{ SDL_SCANCODE_RGUI, "Right GUI" },
};

const char *SDL_GetScancodeName(SDL_Scancode scancode)
{
	for (size_t i = 0; i < sizeof(scancode_names) / sizeof(scancode_names[0]); ++i)
		if (scancode_names[i].scancode == scancode)
			return scancode_names[i].name;

	return "";
}

static int ascii_casecmp(const char *a, const char *b)
{
	for (; ; ++a, ++b)
	{
		int ca = (unsigned char)*a;
		int cb = (unsigned char)*b;

		if (ca >= 'A' && ca <= 'Z')
			ca += 'a' - 'A';
		if (cb >= 'A' && cb <= 'Z')
			cb += 'a' - 'A';

		if (ca != cb)
			return ca - cb;
		if (ca == 0)
			return 0;
	}
}

SDL_Scancode SDL_GetScancodeFromName(const char *name)
{
	if (name == NULL || name[0] == '\0')
		return SDL_SCANCODE_UNKNOWN;

	for (size_t i = 0; i < sizeof(scancode_names) / sizeof(scancode_names[0]); ++i)
		if (ascii_casecmp(scancode_names[i].name, name) == 0)
			return scancode_names[i].scancode;

	return SDL_SCANCODE_UNKNOWN;
}

/* ---- joystick stubs ---------------------------------------------------- */

int SDL_NumJoysticks(void)
{
	return 0;
}

SDL_Joystick *SDL_JoystickOpen(int device_index)
{
	(void)device_index;
	SDL_SetError("no joysticks on PumpkinOS");
	return NULL;
}

void SDL_JoystickClose(SDL_Joystick *joystick)
{
	(void)joystick;
}

const char *SDL_JoystickName(SDL_Joystick *joystick)
{
	(void)joystick;
	return "";
}

int SDL_JoystickNumAxes(SDL_Joystick *joystick)
{
	(void)joystick;
	return 0;
}

int SDL_JoystickNumButtons(SDL_Joystick *joystick)
{
	(void)joystick;
	return 0;
}

int SDL_JoystickNumHats(SDL_Joystick *joystick)
{
	(void)joystick;
	return 0;
}

Sint16 SDL_JoystickGetAxis(SDL_Joystick *joystick, int axis)
{
	(void)joystick;
	(void)axis;
	return 0;
}

Uint8 SDL_JoystickGetButton(SDL_Joystick *joystick, int button)
{
	(void)joystick;
	(void)button;
	return 0;
}

Uint8 SDL_JoystickGetHat(SDL_Joystick *joystick, int hat)
{
	(void)joystick;
	(void)hat;
	return SDL_HAT_CENTERED;
}

void SDL_JoystickUpdate(void)
{
}

int SDL_JoystickEventState(int state)
{
	(void)state;
	return SDL_IGNORE;
}

/* ---- audio device ------------------------------------------------------ */

static SDL_AudioSpec audio_spec;
static int audio_device_open;

static void audio_fill(void *userdata, int16_t *buf, int frames)
{
	(void)userdata;

	if (audio_spec.callback != NULL)
		audio_spec.callback(audio_spec.userdata, (Uint8 *)buf, frames * (int)sizeof(int16_t));
	else
		memset(buf, 0, (size_t)frames * sizeof(int16_t));
}

SDL_AudioDeviceID SDL_OpenAudioDevice(const char *device, int iscapture,
                                      const SDL_AudioSpec *desired, SDL_AudioSpec *obtained,
                                      int allowed_changes)
{
	(void)device;
	(void)allowed_changes;

	if (iscapture || desired == NULL)
	{
		SDL_SetError("unsupported audio device request");
		return 0;
	}

	if (audio_device_open)
	{
		SDL_SetError("audio device already open");
		return 0;
	}

	if (desired->format != AUDIO_S16SYS || desired->channels != 1 || desired->callback == NULL)
	{
		SDL_SetError("only mono S16 callback audio is supported on PumpkinOS");
		return 0;
	}

	audio_spec = *desired;
	if (audio_spec.samples == 0)
		audio_spec.samples = 1024;
	audio_spec.silence = 0;
	audio_spec.size = (Uint32)audio_spec.samples * sizeof(Sint16);

	if (!pk_audio_open(audio_spec.freq, audio_fill, NULL))
	{
		SDL_SetError("failed to open the PumpkinOS sound stream");
		return 0;
	}

	audio_device_open = 1;

	if (obtained != NULL)
		*obtained = audio_spec;

	return 1;
}

void SDL_CloseAudioDevice(SDL_AudioDeviceID dev)
{
	(void)dev;

	if (audio_device_open)
	{
		pk_audio_close();
		audio_device_open = 0;
		audio_spec.callback = NULL;
	}
}

void SDL_PauseAudioDevice(SDL_AudioDeviceID dev, int pause_on)
{
	(void)dev;

	if (audio_device_open)
		pk_audio_pause(pause_on);
}

void SDL_LockAudioDevice(SDL_AudioDeviceID dev)
{
	(void)dev;

	if (audio_device_open)
		pk_audio_lock();
}

void SDL_UnlockAudioDevice(SDL_AudioDeviceID dev)
{
	(void)dev;

	if (audio_device_open)
		pk_audio_unlock();
}

/* ---- audio conversion -------------------------------------------------- */

/* Only the conversion the game needs is implemented: signed 8-bit mono at
   any rate to native signed 16-bit mono at any rate, with linear
   interpolation. */

static int cvt_src_rate, cvt_dst_rate;

int SDL_BuildAudioCVT(SDL_AudioCVT *cvt,
                      SDL_AudioFormat src_format, Uint8 src_channels, int src_rate,
                      SDL_AudioFormat dst_format, Uint8 dst_channels, int dst_rate)
{
	if (cvt == NULL)
		return SDL_SetError("invalid audio conversion");

	memset(cvt, 0, sizeof(*cvt));

	if (src_format != AUDIO_S8 || dst_format != AUDIO_S16SYS ||
	    src_channels != 1 || dst_channels != 1 || src_rate <= 0 || dst_rate <= 0)
	{
		return SDL_SetError("unsupported audio conversion");
	}

	cvt_src_rate = src_rate;
	cvt_dst_rate = dst_rate;

	cvt->needed = 1;
	cvt->src_format = src_format;
	cvt->dst_format = dst_format;
	cvt->rate_incr = (double)src_rate / (double)dst_rate;
	cvt->len_ratio = 2.0 * (double)dst_rate / (double)src_rate;
	cvt->len_mult = 2 * (int)ceil((double)dst_rate / (double)src_rate);
	if (cvt->len_mult < 2)
		cvt->len_mult = 2;

	return 1;
}

int SDL_ConvertAudio(SDL_AudioCVT *cvt)
{
	int in_count, out_count;
	const Sint8 *in;
	Sint16 *out;

	if (cvt == NULL || cvt->buf == NULL || !cvt->needed)
		return SDL_SetError("invalid audio conversion");

	in_count = cvt->len;
	if (in_count <= 0)
	{
		cvt->len_cvt = 0;
		return 0;
	}

	out_count = (int)(((int64_t)in_count * cvt_dst_rate) / cvt_src_rate);
	if (out_count <= 0)
		out_count = 1;

	out = malloc((size_t)out_count * sizeof(Sint16));
	if (out == NULL)
		return SDL_SetError("out of memory");

	in = (const Sint8 *)cvt->buf;

	for (int i = 0; i < out_count; ++i)
	{
		int64_t pos = ((int64_t)i * cvt_src_rate << 16) / cvt_dst_rate;
		int idx = (int)(pos >> 16);
		int frac = (int)(pos & 0xFFFF);
		int s0, s1, s;

		if (idx >= in_count)
			idx = in_count - 1;
		s0 = in[idx];
		s1 = (idx + 1 < in_count) ? in[idx + 1] : s0;
		s = s0 * 256 + (((s1 - s0) * 256 * frac) >> 16);

		if (s > 32767)
			s = 32767;
		else if (s < -32768)
			s = -32768;

		out[i] = (Sint16)s;
	}

	memcpy(cvt->buf, out, (size_t)out_count * sizeof(Sint16));
	cvt->len_cvt = out_count * (int)sizeof(Sint16);

	free(out);

	return 0;
}

/* ---- shutdown ---------------------------------------------------------- */

void sdl_shim_shutdown(void)
{
	if (audio_device_open)
	{
		pk_audio_close();
		audio_device_open = 0;
		audio_spec.callback = NULL;
	}

	event_queue_read = event_queue_write = 0;
	memset(scancode_down, 0, sizeof(scancode_down));
	input_started = 0;
	quit_event_sent = 0;
	initialized_subsystems = 0;
}
