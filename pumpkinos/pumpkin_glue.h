/*
 * OpenTyrian 2000 PumpkinOS port: interface between the game side and the
 * PalmOS side.
 *
 * The port is split in two halves that never share headers:
 *
 *  - the game side (sdl_shim.c, tyr_video.c, tyr_file.c and the original
 *    OpenTyrian sources) only sees the SDL2 compatibility headers;
 *  - the PalmOS side (pumpkin_app.c) only sees PalmOS.h and the PumpkinOS
 *    headers, which define types that clash with the SDL ones.
 *
 * Everything crossing the boundary is declared here with plain C types.
 */
#ifndef PUMPKIN_GLUE_H
#define PUMPKIN_GLUE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- application control ---------------------------------------------- */

/* Pump PalmOS events for at most wait_us microseconds (0 = poll only).
   Returns 0 when the application must terminate (window closed, Quit
   menu item, PumpkinOS shutting down), 1 otherwise. */
int pk_pump(uint32_t wait_us);

/* Non-zero once termination has been requested. */
int pk_quit_requested(void);

/* Unwind to PilotMain. Never returns. */
void pk_exit(int code)
#if defined(__GNUC__) || defined(__clang__)
	__attribute__((noreturn))
#endif
	;

/* Monotonic clock in microseconds. */
int64_t pk_clock_us(void);

/* Write a line to the PumpkinOS log. */
void pk_log(const char *fmt, ...);

/* Show a modal error dialog. */
void pk_error(const char *msg);

/* PalmOS VFS directory holding the user's configuration and save games. */
const char *pk_user_dir(void);

/* ---- video ------------------------------------------------------------- */

#define PK_VGA_WIDTH  320
#define PK_VGA_HEIGHT 200

/* Present a PK_VGA_WIDTH x PK_VGA_HEIGHT frame of native-endian RGB565
   pixels in the application window. */
void pk_present(const uint16_t *rgb565);

/* Position and size (window pixels) of the game screen inside the window,
   as used by the last/next pk_present(). */
void pk_get_layout(int *x0, int *y0, int *w, int *h);

/* Scaling modes (same order as the game's ScalingMode enum). */
#define PK_SCALE_CENTER  0   /* unscaled, centered */
#define PK_SCALE_INTEGER 1   /* largest integer scale that fits */
#define PK_SCALE_FIT_8_5 2   /* fill the window keeping the 8:5 aspect ratio */
#define PK_SCALE_FIT_4_3 3   /* fill the window keeping the 4:3 aspect ratio */

void pk_set_scaling(int mode);

/* ---- input ------------------------------------------------------------- */

/* Key bits reported in pk_input_state.keys (keys that are not plain ASCII
   characters). */
#define PK_KEY_UP     0x0001
#define PK_KEY_DOWN   0x0002
#define PK_KEY_LEFT   0x0004
#define PK_KEY_RIGHT  0x0008
#define PK_KEY_F1     0x0010
#define PK_KEY_F2     0x0020
#define PK_KEY_F3     0x0040
#define PK_KEY_F4     0x0080
#define PK_KEY_END    0x0100

/* Modifier bits reported in pk_input_state.mods. */
#define PK_MOD_SHIFT  0x0001
#define PK_MOD_CTRL   0x0002
#define PK_MOD_LALT   0x0004
#define PK_MOD_RALT   0x0008
#define PK_MOD_RCTRL  0x0010

/* Mouse button bits reported in pk_input_state.buttons. */
#define PK_BUTTON_LEFT  0x01
#define PK_BUTTON_RIGHT 0x02

typedef struct pk_input_state
{
	int focused;          /* application window is active and may receive input */
	uint32_t keys;        /* PK_KEY_* bits currently held */
	uint32_t mods;        /* PK_MOD_* bits currently held */
	uint64_t ascii[2];    /* 128-bit set of ASCII codes currently held */
	uint32_t buttons;     /* PK_BUTTON_* bits currently held */
	int mouse_x, mouse_y; /* pointer position in window pixels */
} pk_input_state;

/* Pump pending PalmOS events (without waiting) and read the current input
   state. */
void pk_poll_input(pk_input_state *st);

/* ---- audio ------------------------------------------------------------- */

/* Mono S16 pull callback: fill frames samples into buf. Called on the
   audio thread while the stream is not paused. */
typedef void (*pk_audio_fill)(void *userdata, int16_t *buf, int frames);

/* Open the output stream at the given rate. Returns 1 on success. The
   stream starts paused. */
int pk_audio_open(int rate, pk_audio_fill fill, void *userdata);
void pk_audio_close(void);
void pk_audio_pause(int paused);

/* Tear the stream down for good (application exit). */
void pk_audio_shutdown(void);

/* Close every file the game left open (application exit). */
void pk_files_close_all(void);

/* Serialize access between the game thread and the audio callback. */
void pk_audio_lock(void);
void pk_audio_unlock(void);

/* ---- game entry point -------------------------------------------------- */

/* OpenTyrian's main(), renamed for PumpkinOS (see src/opentyr.c). */
int tyrian_main(int argc, char *argv[]);

/* Called by the PalmOS side after the game returned or unwound. */
void sdl_shim_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* PUMPKIN_GLUE_H */
