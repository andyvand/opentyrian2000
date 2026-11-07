/*
 * OpenTyrian 2000 PumpkinOS port: PalmOS side.
 *
 * Provides the PalmOS application entry point (PilotMain), the main form,
 * the event pump, the window blitter, input polling, the SndStream based
 * audio output and the file layer (bundled data resources + VFS user
 * files) behind the plain C interface declared in pumpkin_glue.h and
 * pumpkin_stdio.h.
 *
 * This file only sees the PalmOS/PumpkinOS headers. The game sources and
 * the SDL2 compatibility layer never include them (their types clash).
 *
 * Model: the game's main loop runs on the application thread inside
 * PilotMain. Whenever the game sleeps (SDL_Delay) or polls for input the
 * PalmOS event loop is pumped, so the form, its menu and the PumpkinOS
 * window manager stay responsive. When the game calls exit() or the
 * application is closed, control unwinds back to PilotMain with longjmp.
 */

/* the C runtime headers must come before PalmOS.h: libpit/sys.h (pulled in
 * by PalmOS.h) defines __attribute__ away on WINDOWS, which would turn the
 * MinGW CRT's "extern inline __attribute__((__gnu_inline__))" helpers into
 * real out-of-line definitions in every object that includes them */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#ifndef DOS
#include <setjmp.h>
#endif

#include <PalmOS.h>
#include <VFSMgr.h>

#include "sys.h"
#include "thread.h"
#include "mutex.h"
#include "pumpkin.h"
#include "debug.h"

#include "pumpkin_glue.h"
#include "pumpkin_stdio.h"
#include "resource.h"

#define TAG "TYRIAN"

#define APP_DIR "/" VFS_PROGRAMS_DIR "/OpenTyrian"

/* resource type of the bundled data files (see gen_data.sh) */
#define DATA_RES_TYPE 'tdat'

/* the list of bundled data files: { "name", resource id } */
static const struct
{
	const char *name;
	uint16_t id;
} data_resources[] = {
#include "gen/data_index.inc"
	{ NULL, 0 }
};

/* ------------------------------------------------------------------ */
/* state                                                              */
/* ------------------------------------------------------------------ */

static UInt16 volref;
static UInt32 screenWidth, screenHeight;   /* window size in pixels */
static int coordMul;                        /* pixels per standard coordinate */
static int titleH;                          /* title bar height in pixels */

static Boolean ready;                       /* MainForm is open and may be drawn into */
static Boolean mainFormOpen;
static Boolean quitApp;
static Boolean appStopped;

static char title[64];

#ifdef DOS
/* the DOS build is a DXE3 module and pumpkin.exe does not export the C
   library's setjmp/longjmp to modules: use the compiler's built-in ones */
static void *exitJmp[5];
#define PK_SETJMP(buf)  __builtin_setjmp(buf)
#define PK_LONGJMP(buf) __builtin_longjmp(buf, 1)
#else
static jmp_buf exitJmp;
#define PK_SETJMP(buf)  setjmp(buf)
#define PK_LONGJMP(buf) longjmp(buf, 1)
#endif
static int exitArmed;
static int exitCode;

/* video */
static int scaleMode = PK_SCALE_FIT_8_5;    /* PK_SCALE_* */
static int dstW, dstH;                      /* size of the game image (pixels) */
static int screenX0, screenY0;              /* position of the game image (pixels) */
static int layoutValid;
static uint16_t *xmap, *ymap;               /* nearest neighbour lookup tables */
static int fmtProbed;
static int swap16;                          /* 16 bpp window bitmap stores big-endian pixels */
static int idxR, idxG, idxB, idxA;          /* byte offsets within a 32 bpp pixel */

/* audio */
static SndStreamRef stream;
static int streamActive;
static int audioPaused = 1;
static mutex_t *audioMutex;
static pk_audio_fill audioFill;
static void *audioUserdata;
static int audioRate;
static int16_t *monoBuffer;
static uint32_t monoBufferFrames;
static int64_t pullDue;

/* ------------------------------------------------------------------ */
/* helpers                                                            */
/* ------------------------------------------------------------------ */

static Boolean validwindow(void)
{
	FormType *frm = FrmGetActiveForm();
	return frm && FrmGetWindowHandle(frm) == WinGetActiveWindow();
}

void pk_log(const char *fmt, ...)
{
	char buf[512];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	debug(DEBUG_INFO, TAG, "%s", buf);
}

void pk_error(const char *msg)
{
	debug(DEBUG_ERROR, TAG, "%s", msg);
	pumpkin_error_dialog((char *)msg);
}

int64_t pk_clock_us(void)
{
	return sys_get_clock();
}

const char *pk_user_dir(void)
{
	return APP_DIR;
}

/* ------------------------------------------------------------------ */
/* layout and drawing                                                 */
/* ------------------------------------------------------------------ */

static void clear_game_area(void)
{
	WinHandle wh;
	BitmapType *bmp;
	Coord bw, bh;
	UInt16 rowBytes;
	uint8_t *bits;
	int y;

	if (!ready || !validwindow()) return;

	wh = WinGetDisplayWindow();
	bmp = WinGetBitmap(wh);
	if (!bmp) return;

	BmpGetDimensions(bmp, &bw, &bh, &rowBytes);
	bits = (uint8_t *)BmpGetBits(bmp);
	if (!bits) return;

	/* black is all zero bits in every supported pixel format */
	for (y = titleH; y < bh; y++)
		memset(bits + y * rowBytes, 0, rowBytes);

	pumpkin_dirty_region_mode(dirtyRegionBegin);
	pumpkin_screen_dirty(wh, 0, titleH, bw, bh - titleH);
	pumpkin_dirty_region_mode(dirtyRegionEnd);
}

static void layout(void)
{
	int w = (int)screenWidth, h = (int)screenHeight - titleH;
	int s, i;

	switch (scaleMode)
	{
	case PK_SCALE_INTEGER:
		for (s = 8; s > 1; s--)
			if (PK_VGA_WIDTH * s <= w && PK_VGA_HEIGHT * s <= h) break;
		dstW = PK_VGA_WIDTH * s;
		dstH = PK_VGA_HEIGHT * s;
		break;
	case PK_SCALE_FIT_8_5:
		if (h * 8 / 5 > w) { dstW = w; dstH = w * 5 / 8; }
		else { dstH = h; dstW = h * 8 / 5; }
		break;
	case PK_SCALE_FIT_4_3:
		if (h * 4 / 3 > w) { dstW = w; dstH = w * 3 / 4; }
		else { dstH = h; dstW = h * 4 / 3; }
		break;
	case PK_SCALE_CENTER:
	default:
		dstW = PK_VGA_WIDTH;
		dstH = PK_VGA_HEIGHT;
		break;
	}

	if (dstW > w) dstW = w;
	if (dstH > h) dstH = h;
	if (dstW < 1) dstW = 1;
	if (dstH < 1) dstH = 1;

	screenX0 = (w - dstW) / 2;
	screenY0 = titleH + (h - dstH) / 2;
	if (screenX0 < 0) screenX0 = 0;
	if (screenY0 < titleH) screenY0 = titleH;

	free(xmap);
	free(ymap);
	xmap = malloc(dstW * sizeof(uint16_t));
	ymap = malloc(dstH * sizeof(uint16_t));
	if (xmap && ymap)
	{
		for (i = 0; i < dstW; i++) xmap[i] = (uint16_t)((i * PK_VGA_WIDTH) / dstW);
		for (i = 0; i < dstH; i++) ymap[i] = (uint16_t)((i * PK_VGA_HEIGHT) / dstH);
		layoutValid = 1;
	}
	else
	{
		layoutValid = 0;
	}

	debug(DEBUG_INFO, TAG, "layout: window %dx%d mode %d image %dx%d at %d,%d",
		(int)screenWidth, (int)screenHeight, scaleMode, dstW, dstH, screenX0, screenY0);
}

void pk_set_scaling(int mode)
{
	if (mode < PK_SCALE_CENTER || mode > PK_SCALE_FIT_4_3) mode = PK_SCALE_FIT_8_5;

	if (mode != scaleMode || !layoutValid)
	{
		scaleMode = mode;
		layout();
		clear_game_area();
	}
}

void pk_get_layout(int *x0, int *y0, int *w, int *h)
{
	if (!layoutValid) layout();
	if (x0) *x0 = screenX0;
	if (y0) *y0 = screenY0;
	if (w) *w = dstW > 0 ? dstW : PK_VGA_WIDTH;
	if (h) *h = dstH > 0 ? dstH : PK_VGA_HEIGHT;
}

/* paint one pixel of the given colour at the screen origin and look at
   the raw bytes to learn the byte order of the window bitmap */
static void probe_pixel(const RGBColorType *c, uint8_t *bits, UInt16 rowBytes, int bpp, uint8_t out[4])
{
	RGBColorType old;
	UInt16 prev;
	uint8_t *p;

	prev = WinGetCoordinateSystem();
	if (coordMul == 2) WinSetCoordinateSystem(kCoordinatesDouble);
	WinSetForeColorRGB(c, &old);
	WinPaintPixel(screenX0, screenY0);
	WinSetForeColorRGB(&old, NULL);
	WinSetCoordinateSystem(prev);

	p = bits + screenY0 * rowBytes + screenX0 * bpp;
	memcpy(out, p, bpp);
}

static void probe_format(uint8_t *bits, UInt16 rowBytes, int depth)
{
	static const RGBColorType red   = { 0, 0xFF, 0x00, 0x00 };
	static const RGBColorType green = { 0, 0x00, 0xFF, 0x00 };
	static const RGBColorType blue  = { 0, 0x00, 0x00, 0xFF };
	uint8_t px[4];
	int i;

	swap16 = 0;
	idxA = 0; idxR = 1; idxG = 2; idxB = 3;

	if (depth == 16)
	{
		probe_pixel(&red, bits, rowBytes, 2, px);
		/* native little-endian 0xF800 is stored as 00 F8 */
		swap16 = !(px[0] == 0x00 && px[1] == 0xF8);
	}
	else if (depth == 32)
	{
		probe_pixel(&red, bits, rowBytes, 4, px);
		for (i = 0; i < 4; i++) if (px[i] == 0xFF) { idxR = i; break; }
		probe_pixel(&green, bits, rowBytes, 4, px);
		for (i = 0; i < 4; i++) if (px[i] == 0xFF) { idxG = i; break; }
		probe_pixel(&blue, bits, rowBytes, 4, px);
		for (i = 0; i < 4; i++) if (px[i] == 0xFF) { idxB = i; break; }
		for (i = 0; i < 4; i++) if (i != idxR && i != idxG && i != idxB) idxA = i;
	}

	debug(DEBUG_INFO, TAG, "window bitmap depth %d swap16 %d rgba idx %d %d %d %d",
		depth, swap16, idxR, idxG, idxB, idxA);
	fmtProbed = 1;
}

void pk_present(const uint16_t *frame)
{
	WinHandle wh;
	BitmapType *bmp;
	Coord bw, bh;
	UInt16 rowBytes;
	UInt8 depth;
	uint8_t *bits;
	const uint16_t *src;
	uint16_t c, *dst16;
	uint8_t *dst8;
	int x, y;

	if (!ready || !mainFormOpen || !frame || !validwindow()) return;
	if (!layoutValid) layout();
	if (!layoutValid) return;

	wh = WinGetDisplayWindow();
	bmp = WinGetBitmap(wh);
	if (!bmp) return;

	BmpGetDimensions(bmp, &bw, &bh, &rowBytes);
	depth = BmpGetBitDepth(bmp);
	bits = (uint8_t *)BmpGetBits(bmp);
	if (!bits) return;

	if (screenX0 + dstW > bw || screenY0 + dstH > bh) return;

	if (!fmtProbed) probe_format(bits, rowBytes, depth);

	if (depth == 16)
	{
		for (y = 0; y < dstH; y++)
		{
			src = frame + ymap[y] * PK_VGA_WIDTH;
			dst16 = (uint16_t *)(bits + (screenY0 + y) * rowBytes) + screenX0;
			if (swap16)
			{
				for (x = 0; x < dstW; x++)
				{
					c = src[xmap[x]];
					dst16[x] = (uint16_t)((c << 8) | (c >> 8));
				}
			}
			else
			{
				for (x = 0; x < dstW; x++)
					dst16[x] = src[xmap[x]];
			}
		}
	}
	else if (depth == 32)
	{
		for (y = 0; y < dstH; y++)
		{
			src = frame + ymap[y] * PK_VGA_WIDTH;
			dst8 = bits + (screenY0 + y) * rowBytes + screenX0 * 4;
			for (x = 0; x < dstW; x++, dst8 += 4)
			{
				c = src[xmap[x]];
				dst8[idxA] = 0xFF;
				dst8[idxR] = (uint8_t)((((c >> 11) & 0x1F) * 255) / 31);
				dst8[idxG] = (uint8_t)((((c >> 5) & 0x3F) * 255) / 63);
				dst8[idxB] = (uint8_t)(((c & 0x1F) * 255) / 31);
			}
		}
	}
	else
	{
		return;
	}

	pumpkin_dirty_region_mode(dirtyRegionBegin);
	pumpkin_screen_dirty(wh, screenX0, screenY0, dstW, dstH);
	pumpkin_dirty_region_mode(dirtyRegionEnd);
}

/* ------------------------------------------------------------------ */
/* forms and events                                                   */
/* ------------------------------------------------------------------ */

static void resize(FormType *frm)
{
	UInt32 sw, sh;
	RectangleType rect;

	WinScreenMode(winScreenModeGet, &sw, &sh, NULL, NULL);
	RctSetRectangle(&rect, 0, 0, sw, sh);
	WinSetBounds(FrmGetWindowHandle(frm), &rect);
}

static void menu_event(UInt16 id)
{
	switch (id)
	{
	case menuAbout:
		AbtShowAboutPumpkin(pumpkin_get_app_creator());
		break;
	case menuQuit:
		quitApp = true;
		break;
	}
}

static Boolean MainFormHandleEvent(EventType *event)
{
	FormType *frm;
	Boolean handled = false;

	switch (event->eType)
	{
	case frmOpenEvent:
		frm = FrmGetActiveForm();
		resize(frm);
		FrmSetTitle(frm, title);
		FrmDrawForm(frm);
		mainFormOpen = true;
		ready = true;
		layoutValid = 0;
		fmtProbed = 0;
		clear_game_area();
		handled = true;
		break;
	case frmUpdateEvent:
		frm = FrmGetActiveForm();
		FrmDrawForm(frm);
		clear_game_area();
		handled = true;
		break;
	case frmCloseEvent:
		mainFormOpen = false;
		ready = false;
		break;
	case penDownEvent:
	case penMoveEvent:
	case penUpEvent:
		/* the pointer is polled with pumpkin_status(); swallow the events
		   so that the form does not try to interpret them */
		handled = true;
		break;
	case menuEvent:
		menu_event(event->data.menu.itemID);
		handled = true;
		break;
	default:
		break;
	}

	return handled;
}

static Boolean ApplicationHandleEvent(EventType *event)
{
	FormType *frm;
	UInt16 form;
	Boolean handled = false;

	if (event->eType == frmLoadEvent)
	{
		form = event->data.frmLoad.formID;
		frm = FrmInitForm(form);
		FrmSetActiveForm(frm);
		switch (form)
		{
		case MainForm:
			FrmSetEventHandler(frm, MainFormHandleEvent);
			break;
		}
		handled = true;
	}

	return handled;
}

/* pump one event and drain the task mailbox; us is the maximum wait */
int pk_pump(uint32_t us)
{
	EventType event;
	Err err;
	unsigned char *buf = NULL;
	unsigned int len = 0;

	if (thread_must_end())
	{
		appStopped = true;
		return 0;
	}

	EvtGetEventUs(&event, us);
	if (!SysHandleEvent(&event))
	{
		if (!MenuHandleEvent(NULL, &event, &err))
		{
			if (!ApplicationHandleEvent(&event))
			{
				FrmDispatchEvent(&event);
			}
		}
	}

	if (thread_server_read_timeout(0, &buf, &len) == -1)
	{
		appStopped = true;
	}
	else if (buf)
	{
		sys_free(buf);
	}

	if (event.eType == appStopEvent) appStopped = true;

	return (quitApp || appStopped) ? 0 : 1;
}

int pk_quit_requested(void)
{
	return (quitApp || appStopped) ? 1 : 0;
}

void pk_exit(int code)
{
	debug(DEBUG_INFO, TAG, "exit(%d) requested", code);

	if (exitArmed)
	{
		exitCode = code;
		PK_LONGJMP(exitJmp);
	}

	/* nowhere to unwind to (should not happen): park the thread until
	   the application is stopped */
	quitApp = true;
	for (;;)
	{
		if (thread_must_end()) break;
		pk_pump(100000);
	}
	pumpkin_fatal_error(0);
	for (;;) sys_usleep(1000000);
}

/* ------------------------------------------------------------------ */
/* input                                                              */
/* ------------------------------------------------------------------ */

void pk_poll_input(pk_input_state *st)
{
	uint32_t keyMask = 0, modMask = 0, buttonMask = 0;
	uint64_t ext[2] = { 0, 0 };
	int x = 0, y = 0;

	memset(st, 0, sizeof(*st));

	/* dispatch whatever is pending without waiting */
	if (!quitApp && !appStopped && EvtEventAvail())
		pk_pump(0);

	if (!mainFormOpen || !validwindow())
	{
		/* keep the last known pointer position while a menu or dialog
		   is up, release all keys */
		pumpkin_status(&x, &y, NULL, NULL, NULL, NULL);
		st->mouse_x = x;
		st->mouse_y = y;
		return;
	}

	pumpkin_status(&x, &y, &keyMask, &modMask, &buttonMask, ext);

	st->focused = 1;
	st->mouse_x = x;
	st->mouse_y = y;

	if (keyMask & keyBitPageUp)   st->keys |= PK_KEY_UP;
	if (keyMask & keyBitPageDown) st->keys |= PK_KEY_DOWN;
	if (keyMask & keyBitLeft)     st->keys |= PK_KEY_LEFT;
	if (keyMask & keyBitRight)    st->keys |= PK_KEY_RIGHT;
	if (keyMask & keyBitHard1)    st->keys |= PK_KEY_F1;
	if (keyMask & keyBitHard2)    st->keys |= PK_KEY_F2;
	if (keyMask & keyBitHard3)    st->keys |= PK_KEY_F3;
	if (keyMask & keyBitHard4)    st->keys |= PK_KEY_F4;
	if (keyMask & keyBitPower)    st->keys |= PK_KEY_END;

	if (modMask & WINDOW_MOD_SHIFT) st->mods |= PK_MOD_SHIFT;
	if (modMask & WINDOW_MOD_CTRL)  st->mods |= PK_MOD_CTRL;
	if (modMask & WINDOW_MOD_RCTRL) st->mods |= PK_MOD_RCTRL;
	if (modMask & WINDOW_MOD_LALT)  st->mods |= PK_MOD_LALT;
	if (modMask & WINDOW_MOD_RALT)  st->mods |= PK_MOD_RALT;

	st->ascii[0] = ext[0];
	st->ascii[1] = ext[1];

	if (buttonMask & 1) st->buttons |= PK_BUTTON_LEFT;
	if (buttonMask & 2) st->buttons |= PK_BUTTON_RIGHT;

	{
		/* log key and button changes (not pointer motion) for debugging */
		static uint32_t lastKeys, lastMods, lastButtons;
		static uint64_t lastAscii[2];

		if (st->keys != lastKeys || st->mods != lastMods || st->buttons != lastButtons ||
		    st->ascii[0] != lastAscii[0] || st->ascii[1] != lastAscii[1])
		{
			debug(DEBUG_INFO, TAG, "input: keys 0x%03x mods 0x%02x ascii %08x%08x%08x%08x buttons 0x%x at %d,%d",
				(unsigned int)st->keys, (unsigned int)st->mods,
				(unsigned int)(st->ascii[1] >> 32), (unsigned int)st->ascii[1],
				(unsigned int)(st->ascii[0] >> 32), (unsigned int)st->ascii[0],
				(unsigned int)st->buttons, x, y);
			lastKeys = st->keys;
			lastMods = st->mods;
			lastButtons = st->buttons;
			lastAscii[0] = st->ascii[0];
			lastAscii[1] = st->ascii[1];
		}
	}
}

/* ------------------------------------------------------------------ */
/* audio                                                              */
/* ------------------------------------------------------------------ */

/* PumpkinOS audio contract: the audio thread pulls from the stream
   callback in a loop and treats a short return as end-of-stream, so the
   callback always returns a full buffer (silence while paused) and paces
   itself to real time. */
static Err stream_callback(void *userdata, SndStreamRef channel, void *buffer, UInt32 numberofframes)
{
	int16_t *out = (int16_t *)buffer;
	UInt32 i;
	int64_t now;

	(void)userdata;
	(void)channel;

#ifndef DOS
	now = sys_get_clock();
	if (pullDue == 0 || pullDue < now - 500000) pullDue = now;
	if (pullDue > now) sys_usleep((uint32_t)(pullDue - now));
	pullDue += (int64_t)numberofframes * 1000000 / audioRate;
#else
	(void)now;
#endif

	if (numberofframes > monoBufferFrames)
	{
		int16_t *p = realloc(monoBuffer, numberofframes * sizeof(int16_t));
		if (p == NULL)
		{
			memset(out, 0, numberofframes * 2 * sizeof(int16_t));
			return errNone;
		}
		monoBuffer = p;
		monoBufferFrames = numberofframes;
	}

	if (!audioPaused && audioFill && audioMutex && mutex_lock(audioMutex) == 0)
	{
		if (!audioPaused && audioFill)
			audioFill(audioUserdata, monoBuffer, (int)numberofframes);
		else
			memset(monoBuffer, 0, numberofframes * sizeof(int16_t));
		mutex_unlock(audioMutex);
	}
	else
	{
		memset(monoBuffer, 0, numberofframes * sizeof(int16_t));
	}

	for (i = 0; i < numberofframes; i++)
	{
		out[i * 2] = monoBuffer[i];
		out[i * 2 + 1] = monoBuffer[i];
	}

	return errNone;
}

int pk_audio_open(int rate, pk_audio_fill fill, void *userdata)
{
	Err err;

	if (!audioMutex) audioMutex = mutex_create("tyriansnd");

	/* PumpkinOS audio contract: a stream that stops delivering data is
	   torn down by the audio thread, so the stream is kept alive across
	   close/open cycles (restart_audio) and only muted in between. */
	if (streamActive && rate != audioRate)
	{
		pk_audio_shutdown();
	}

	if (audioMutex && mutex_lock(audioMutex) == 0)
	{
		audioRate = rate;
		audioFill = fill;
		audioUserdata = userdata;
		audioPaused = 1;
		pullDue = 0;
		mutex_unlock(audioMutex);
	}

	if (streamActive) return 1;

	err = SndStreamCreate(&stream, sndOutput, (UInt32)rate, sndInt16, sndStereo,
			stream_callback, NULL, 0, false);
	if (err != errNone)
	{
		debug(DEBUG_ERROR, TAG, "SndStreamCreate failed: %d", err);
		audioFill = NULL;
		return 0;
	}
	if ((err = SndStreamStart(stream)) != errNone)
	{
		debug(DEBUG_ERROR, TAG, "SndStreamStart failed: %d (sound enabled %d)", err, pumpkin_sound_enabled());
		SndStreamDelete(stream);
		stream = 0;
		audioFill = NULL;
		return 0;
	}

	streamActive = 1;
	debug(DEBUG_INFO, TAG, "audio stream started (%d Hz)", rate);

	return 1;
}

void pk_audio_close(void)
{
	/* mute only; see pk_audio_open() */
	if (audioMutex && mutex_lock(audioMutex) == 0)
	{
		audioPaused = 1;
		audioFill = NULL;
		mutex_unlock(audioMutex);
	}
	else
	{
		audioPaused = 1;
		audioFill = NULL;
	}
}

void pk_audio_shutdown(void)
{
	pk_audio_close();

	if (streamActive)
	{
		SndStreamDelete(stream);
		stream = 0;
		streamActive = 0;
		debug(DEBUG_INFO, TAG, "audio stream closed");
	}
}

void pk_audio_pause(int paused)
{
	audioPaused = paused ? 1 : 0;
	if (!paused) pullDue = 0;
}

void pk_audio_lock(void)
{
	if (audioMutex) mutex_lock(audioMutex);
}

void pk_audio_unlock(void)
{
	if (audioMutex) mutex_unlock(audioMutex);
}

/* ------------------------------------------------------------------ */
/* files: bundled resources and VFS                                   */
/* ------------------------------------------------------------------ */

enum { TYR_FILE_RES = 1, TYR_FILE_VFS = 2 };

struct tyr_FILE
{
	int kind;
	/* resource */
	MemHandle h;
	const uint8_t *data;
	uint32_t size;
	uint32_t pos;
	/* vfs */
	FileRef fr;
	int writable;
	int eof;
	int err;
	/* list of open files, see pk_files_close_all() */
	struct tyr_FILE *next, *prev;
};

static tyr_FILE *openFiles;

static void link_file(tyr_FILE *f)
{
	f->prev = NULL;
	f->next = openFiles;
	if (openFiles) openFiles->prev = f;
	openFiles = f;
}

static void unlink_file(tyr_FILE *f)
{
	if (f->prev) f->prev->next = f->next;
	else openFiles = f->next;
	if (f->next) f->next->prev = f->prev;
	f->next = f->prev = NULL;
}

static int ascii_casecmp(const char *a, const char *b)
{
	for (; ; ++a, ++b)
	{
		int ca = (unsigned char)*a;
		int cb = (unsigned char)*b;

		if (ca >= 'A' && ca <= 'Z') ca += 'a' - 'A';
		if (cb >= 'A' && cb <= 'Z') cb += 'a' - 'A';
		if (ca != cb) return ca - cb;
		if (ca == 0) return 0;
	}
}

static int find_data_resource(const char *name)
{
	int i;

	for (i = 0; data_resources[i].name != NULL; i++)
	{
		if (ascii_casecmp(data_resources[i].name, name) == 0)
			return data_resources[i].id;
	}

	return -1;
}

static tyr_FILE *open_resource(const char *name, const char *mode)
{
	tyr_FILE *f;
	MemHandle h;
	int id;

	if (mode[0] != 'r')
	{
		debug(DEBUG_ERROR, TAG, "data resource \"%s\" is read only", name);
		return NULL;
	}

	if ((id = find_data_resource(name)) < 0)
	{
		debug(DEBUG_ERROR, TAG, "no data resource \"%s\"", name);
		return NULL;
	}

	if ((h = DmGetResource(DATA_RES_TYPE, (DmResID)id)) == NULL)
	{
		debug(DEBUG_ERROR, TAG, "data resource \"%s\" (id %d) not found in PRC", name, id);
		return NULL;
	}

	if ((f = sys_calloc(1, sizeof(tyr_FILE))) == NULL)
	{
		DmReleaseResource(h);
		return NULL;
	}

	f->kind = TYR_FILE_RES;
	f->h = h;
	f->size = (uint32_t)MemHandleSize(h);
	f->data = (const uint8_t *)MemHandleLock(h);
	f->pos = 0;

	if (f->data == NULL)
	{
		DmReleaseResource(h);
		sys_free(f);
		return NULL;
	}

	debug(DEBUG_TRACE, TAG, "opened data resource \"%s\" (id %d, %u bytes)", name, id, (unsigned int)f->size);
	link_file(f);

	return f;
}

static tyr_FILE *open_vfs(const char *path, const char *mode)
{
	tyr_FILE *f;
	UInt16 openMode;
	Err err;

	if (mode[0] == 'w')
	{
		openMode = vfsModeWrite | vfsModeCreate | vfsModeTruncate;
	}
	else if (mode[0] == 'a')
	{
		openMode = vfsModeWrite | vfsModeCreate;
	}
	else
	{
		openMode = vfsModeRead;
	}
	if (strchr(mode, '+')) openMode |= vfsModeRead | vfsModeWrite;

	if ((f = sys_calloc(1, sizeof(tyr_FILE))) == NULL) return NULL;

	f->kind = TYR_FILE_VFS;
	f->writable = (openMode & vfsModeWrite) ? 1 : 0;

	if ((err = VFSFileOpen(volref, path, openMode, &f->fr)) != errNone)
	{
		debug(DEBUG_TRACE, TAG, "VFSFileOpen \"%s\" mode %s failed: %d", path, mode, err);
		sys_free(f);
		return NULL;
	}

	if (mode[0] == 'a') VFSFileSeek(f->fr, vfsOriginEnd, 0);
	link_file(f);

	return f;
}

tyr_FILE *tyr_fopen(const char *path, const char *mode)
{
	size_t plen = strlen(TYR_RES_PREFIX);

	if (path == NULL || mode == NULL) return NULL;

	if (strncmp(path, TYR_RES_PREFIX, plen) == 0)
		return open_resource(path + plen, mode);

	return open_vfs(path, mode);
}

int tyr_fclose(tyr_FILE *f)
{
	if (f == NULL) return -1;

	unlink_file(f);

	if (f->kind == TYR_FILE_RES)
	{
		MemHandleUnlock(f->h);
		DmReleaseResource(f->h);
	}
	else
	{
		VFSFileClose(f->fr);
	}

	sys_free(f);
	return 0;
}

size_t tyr_fread(void *ptr, size_t size, size_t nmemb, tyr_FILE *f)
{
	size_t total = size * nmemb;

	if (f == NULL || ptr == NULL || total == 0) return 0;

	if (f->kind == TYR_FILE_RES)
	{
		size_t avail = f->pos < f->size ? f->size - f->pos : 0;
		size_t n = total < avail ? total : avail;

		memcpy(ptr, f->data + f->pos, n);
		f->pos += (uint32_t)n;
		if (n < total) f->eof = 1;
		return n / size;
	}
	else
	{
		UInt32 nread = 0;
		Err err = VFSFileRead(f->fr, (UInt32)total, ptr, &nread);

		if (err != errNone && err != vfsErrFileEOF)
		{
			f->err = 1;
			return 0;
		}
		if (nread < total) f->eof = 1;
		return nread / size;
	}
}

size_t tyr_fwrite(const void *ptr, size_t size, size_t nmemb, tyr_FILE *f)
{
	size_t total = size * nmemb;
	UInt32 nwritten = 0;

	if (f == NULL || ptr == NULL || total == 0) return 0;
	if (f->kind != TYR_FILE_VFS || !f->writable)
	{
		f->err = 1;
		return 0;
	}

	if (VFSFileWrite(f->fr, (UInt32)total, ptr, &nwritten) != errNone)
	{
		f->err = 1;
		return nwritten / size;
	}

	return nwritten / size;
}

int tyr_fseek(tyr_FILE *f, long offset, int whence)
{
	if (f == NULL) return -1;

	f->eof = 0;

	if (f->kind == TYR_FILE_RES)
	{
		long base;

		switch (whence)
		{
		case SEEK_SET: base = 0; break;
		case SEEK_CUR: base = (long)f->pos; break;
		case SEEK_END: base = (long)f->size; break;
		default: return -1;
		}
		if (base + offset < 0 || base + offset > (long)f->size) return -1;
		f->pos = (uint32_t)(base + offset);
		return 0;
	}
	else
	{
		FileOrigin origin;

		switch (whence)
		{
		case SEEK_SET: origin = vfsOriginBeginning; break;
		case SEEK_CUR: origin = vfsOriginCurrent; break;
		case SEEK_END: origin = vfsOriginEnd; break;
		default: return -1;
		}
		return VFSFileSeek(f->fr, origin, (Int32)offset) == errNone ? 0 : -1;
	}
}

long tyr_ftell(tyr_FILE *f)
{
	if (f == NULL) return -1;

	if (f->kind == TYR_FILE_RES)
		return (long)f->pos;

	{
		UInt32 pos = 0;
		if (VFSFileTell(f->fr, &pos) != errNone) return -1;
		return (long)pos;
	}
}

void tyr_rewind(tyr_FILE *f)
{
	tyr_fseek(f, 0, SEEK_SET);
}

int tyr_feof(tyr_FILE *f)
{
	if (f == NULL) return 1;
	return f->eof;
}

int tyr_ferror(tyr_FILE *f)
{
	if (f == NULL) return 1;
	return f->err;
}

int tyr_fflush(tyr_FILE *f)
{
	(void)f;
	return 0;
}

int tyr_fgetc(tyr_FILE *f)
{
	unsigned char c;

	if (tyr_fread(&c, 1, 1, f) != 1) return EOF;
	return c;
}

int tyr_fputc(int c, tyr_FILE *f)
{
	unsigned char b = (unsigned char)c;

	if (tyr_fwrite(&b, 1, 1, f) != 1) return EOF;
	return b;
}

int tyr_fputs(const char *s, tyr_FILE *f)
{
	size_t len = strlen(s);

	if (len == 0) return 0;
	if (tyr_fwrite(s, 1, len, f) != len) return EOF;
	return 1;
}

char *tyr_fgets(char *s, int size, tyr_FILE *f)
{
	int i = 0, c;

	if (s == NULL || size <= 0 || f == NULL) return NULL;

	while (i < size - 1)
	{
		c = tyr_fgetc(f);
		if (c == EOF) break;
		s[i++] = (char)c;
		if (c == '\n') break;
	}

	if (i == 0) return NULL;
	s[i] = '\0';
	return s;
}

void pk_files_close_all(void)
{
	int n = 0;

	while (openFiles)
	{
		tyr_fclose(openFiles);
		n++;
	}

	if (n) debug(DEBUG_INFO, TAG, "closed %d file(s) left open by the game", n);
}

/* ------------------------------------------------------------------ */
/* directories                                                        */
/* ------------------------------------------------------------------ */

static void ensure_dir(const char *path)
{
	FileRef fr;

	if (VFSFileOpen(volref, path, vfsModeRead, &fr) == errNone)
	{
		VFSFileClose(fr);
	}
	else
	{
		VFSDirCreate(volref, path);
	}
}

/* ------------------------------------------------------------------ */
/* PilotMain                                                          */
/* ------------------------------------------------------------------ */

#if defined(ESP32)
UInt32 OpenTyrianPilotMain(UInt16 cmd, MemPtr cmdPBP, UInt16 launchFlags)
#else
UInt32 PilotMain(UInt16 cmd, MemPtr cmdPBP, UInt16 launchFlags)
#endif
{
	UInt32 iterator, density;
	static char *argv[2];
	int r;

	(void)cmdPBP;
	(void)launchFlags;

	if (cmd != sysAppLaunchCmdNormalLaunch) return 0;

	iterator = vfsIteratorStart;
	VFSVolumeEnumerate(&volref, &iterator);

	WinScreenGetAttribute(winScreenWidth, &screenWidth);
	WinScreenGetAttribute(winScreenHeight, &screenHeight);
	WinScreenGetAttribute(winScreenDensity, &density);
	coordMul = (density == kDensityDouble) ? 2 : 1;
	titleH = 15 * coordMul;
	FrmCenterDialogs(true);

	StrCopy(title, "OpenTyrian 2000");
	quitApp = false;
	appStopped = false;
	ready = false;
	mainFormOpen = false;
	layoutValid = 0;
	fmtProbed = 0;
	scaleMode = PK_SCALE_FIT_8_5;

	ensure_dir(APP_DIR);

	FrmGotoForm(MainForm);
	while (!ready)
	{
		if (!pk_pump(10000)) break;
	}

	if (!quitApp && !appStopped)
	{
		argv[0] = "opentyrian2000";
		argv[1] = NULL;

		exitArmed = 1;
		exitCode = 0;
		if (PK_SETJMP(exitJmp) == 0)
		{
			r = tyrian_main(1, argv);
			debug(DEBUG_INFO, TAG, "game returned %d", r);
		}
		else
		{
			debug(DEBUG_INFO, TAG, "game exited with code %d", exitCode);
		}
		exitArmed = 0;
	}

	sdl_shim_shutdown();
	pk_audio_shutdown();
	pk_files_close_all();

	if (monoBuffer)
	{
		free(monoBuffer);
		monoBuffer = NULL;
		monoBufferFrames = 0;
	}

	free(xmap);
	free(ymap);
	xmap = ymap = NULL;
	layoutValid = 0;

	FrmCloseAllForms();

	return 0;
}
