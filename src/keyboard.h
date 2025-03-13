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
#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "opentyr.h"

#ifdef WITH_SDL3
#include <SDL3/SDL.h>

#ifndef SDL_SCANCODE_COUNT
#define SDL_SCANCODE_COUNT 512
#endif

#ifndef SDL_TEXTINPUTEVENT_TEXT_SIZE
#define SDL_TEXTINPUTEVENT_TEXT_SIZE (32)
#endif
#else
#ifdef WITH_SDL
#include <SDL.h>
#else
#include <SDL2/SDL.h>
#endif
#endif

#ifdef WITH_SDL1
#ifndef SDL_Scancode
#define SDL_Scancode SDLKey
#endif

#ifndef SDL_Keymod
#define SDL_Keymod SDLMod
#endif

#ifndef SDL_NUM_SCANCODES
#define SDL_NUM_SCANCODES SDLK_LAST
#endif

#ifndef SDL_TEXTINPUTEVENT_TEXT_SIZE
#define SDL_TEXTINPUTEVENT_TEXT_SIZE (32)
#endif

#ifndef SDL_SCANCODE_UP
#define SDL_SCANCODE_UP SDLK_UP
#endif

#ifndef SDL_SCANCODE_DOWN
#define SDL_SCANCODE_DOWN SDLK_DOWN
#endif

#ifndef SDL_SCANCODE_LEFT
#define SDL_SCANCODE_LEFT SDLK_LEFT
#endif

#ifndef SDL_SCANCODE_RIGHT
#define SDL_SCANCODE_RIGHT SDLK_RIGHT
#endif

#ifndef SDL_SCANCODE_BACKSLASH
#define SDL_SCANCODE_BACKSLASH SDLK_BACKSLASH
#endif

#ifndef SDL_SCANCODE_SLASH
#define SDL_SCANCODE_SLASH SDLK_SLASH
#endif

#ifndef SDL_SCANCODE_HOME
#define SDL_SCANCODE_HOME SDLK_HOME
#endif

#ifndef SDL_SCANCODE_END
#define SDL_SCANCODE_END SDLK_END
#endif

#ifndef SDL_SCANCODE_DELETE
#define SDL_SCANCODE_DELETE SDLK_DELETE
#endif

#ifndef SDL_SCANCODE_TAB
#define SDL_SCANCODE_TAB SDLK_TAB
#endif

#ifndef SDL_SCANCODE_COMMA
#define SDL_SCANCODE_COMMA SDLK_COMMA
#endif

#ifndef SDL_SCANCODE_PERIOD
#define SDL_SCANCODE_PERIOD SDLK_PERIOD
#endif

#ifndef SDL_SCANCODE_SEMICOLON
#define SDL_SCANCODE_SEMICOLON SDLK_SEMICOLON
#endif

#ifndef SDL_SCANCODE_CAPSLOCK
#define SDL_SCANCODE_CAPSLOCK SDLK_CAPSLOCK
#endif

#ifndef SDL_SCANCODE_SCROLLLOCK
#define SDL_SCANCODE_SCROLLLOCK SDLK_SCROLLOCK
#endif

#ifndef SDL_SCANCODE_NUMLOCKCLEAR
#define SDL_SCANCODE_NUMLOCKCLEAR SDLK_NUMLOCK
#endif

#ifndef SDL_SCANCODE_MINUS
#define SDL_SCANCODE_MINUS SDLK_MINUS
#endif

#ifndef SDL_SCANCODE_0
#define SDL_SCANCODE_0 SDLK_0
#endif

#ifndef SDL_SCANCODE_1
#define SDL_SCANCODE_1 SDLK_1
#endif

#ifndef SDL_SCANCODE_2
#define SDL_SCANCODE_2 SDLK_2
#endif

#ifndef SDL_SCANCODE_3
#define SDL_SCANCODE_3 SDLK_3
#endif

#ifndef SDL_SCANCODE_4
#define SDL_SCANCODE_4 SDLK_4
#endif

#ifndef SDL_SCANCODE_5
#define SDL_SCANCODE_5 SDLK_5
#endif

#ifndef SDL_SCANCODE_6
#define SDL_SCANCODE_6 SDLK_6
#endif

#ifndef SDL_SCANCODE_7
#define SDL_SCANCODE_7 SDLK_7
#endif

#ifndef SDL_SCANCODE_8
#define SDL_SCANCODE_8 SDLK_8
#endif

#ifndef SDL_SCANCODE_9
#define SDL_SCANCODE_9 SDLK_9
#endif

#ifndef SDL_SCANCODE_GRAVE
#define SDL_SCANCODE_GRAVE SDLK_QUOTE
#endif

#ifndef SDL_SCANCODE_LEFTBRACKET
#define SDL_SCANCODE_LEFTBRACKET SDLK_LEFTBRACKET
#endif

#ifndef SDL_SCANCODE_RIGHTBRACKET
#define SDL_SCANCODE_RIGHTBRACKET SDLK_RIGHTBRACKET
#endif

#ifndef SDL_SCANCODE_A
#define SDL_SCANCODE_A SDLK_a
#endif

#ifndef SDL_SCANCODE_C
#define SDL_SCANCODE_C SDLK_c
#endif

#ifndef SDL_SCANCODE_D
#define SDL_SCANCODE_D SDLK_d
#endif

#ifndef SDL_SCANCODE_F
#define SDL_SCANCODE_F SDLK_f
#endif

#ifndef SDL_SCANCODE_G
#define SDL_SCANCODE_G SDLK_g
#endif

#ifndef SDL_SCANCODE_L
#define SDL_SCANCODE_L SDLK_l
#endif

#ifndef SDL_SCANCODE_N
#define SDL_SCANCODE_N SDLK_n
#endif

#ifndef SDL_SCANCODE_O
#define SDL_SCANCODE_O SDLK_o
#endif

#ifndef SDL_SCANCODE_P
#define SDL_SCANCODE_P SDLK_p
#endif

#ifndef SDL_SCANCODE_Q
#define SDL_SCANCODE_Q SDLK_q
#endif

#ifndef SDL_SCANCODE_R
#define SDL_SCANCODE_R SDLK_r
#endif

#ifndef SDL_SCANCODE_S
#define SDL_SCANCODE_S SDLK_s
#endif

#ifndef SDL_SCANCODE_V
#define SDL_SCANCODE_V SDLK_v
#endif

#ifndef SDL_SCANCODE_W
#define SDL_SCANCODE_W SDLK_w
#endif

#ifndef SDL_SCANCODE_X
#define SDL_SCANCODE_X SDLK_x
#endif

#ifndef SDL_SCANCODE_Z
#define SDL_SCANCODE_Z SDLK_z
#endif

#ifndef SDL_SCANCODE_EQUALS
#define SDL_SCANCODE_EQUALS SDLK_EQUALS
#endif

#ifndef SDL_SCANCODE_KP_MINUS
#define SDL_SCANCODE_KP_MINUS SDLK_KP_MINUS
#endif

#ifndef SDL_SCANCODE_KP_PLUS
#define SDL_SCANCODE_KP_PLUS SDLK_KP_PLUS
#endif

#ifndef SDL_SCANCODE_KP_ENTER
#define SDL_SCANCODE_KP_ENTER SDLK_KP_ENTER
#endif

#ifndef SDL_SCANCODE_INSERT
#define SDL_SCANCODE_INSERT SDLK_INSERT
#endif

#ifndef SDL_SCANCODE_PAGEUP
#define SDL_SCANCODE_PAGEUP SDLK_PAGEUP
#endif

#ifndef SDL_SCANCODE_PAGEDOWN
#define SDL_SCANCODE_PAGEDOWN SDLK_PAGEDOWN
#endif

#ifndef SDL_SCANCODE_KP_0
#define SDL_SCANCODE_KP_0 SDLK_KP0
#endif

#ifndef SDL_SCANCODE_KP_1
#define SDL_SCANCODE_KP_1 SDLK_KP1
#endif

#ifndef SDL_SCANCODE_KP_2
#define SDL_SCANCODE_KP_2 SDLK_KP2
#endif

#ifndef SDL_SCANCODE_KP_3
#define SDL_SCANCODE_KP_3 SDLK_KP3
#endif

#ifndef SDL_SCANCODE_KP_4
#define SDL_SCANCODE_KP_4 SDLK_KP4
#endif

#ifndef SDL_SCANCODE_KP_5
#define SDL_SCANCODE_KP_5 SDLK_KP5
#endif

#ifndef SDL_SCANCODE_KP_6
#define SDL_SCANCODE_KP_6 SDLK_KP6
#endif

#ifndef SDL_SCANCODE_KP_7
#define SDL_SCANCODE_KP_7 SDLK_KP7
#endif

#ifndef SDL_SCANCODE_KP_8
#define SDL_SCANCODE_KP_8 SDLK_KP8
#endif

#ifndef SDL_SCANCODE_KP_9
#define SDL_SCANCODE_KP_9 SDLK_KP9
#endif

#ifndef SDL_SCANCODE_RETURN
#define SDL_SCANCODE_RETURN SDLK_RETURN
#endif

#ifndef SDL_SCANCODE_SPACE
#define SDL_SCANCODE_SPACE SDLK_SPACE
#endif

#ifndef SDL_SCANCODE_LSHIFT
#define SDL_SCANCODE_LSHIFT SDLK_LSHIFT
#endif

#ifndef SDL_SCANCODE_RSHIFT
#define SDL_SCANCODE_RSHIFT SDLK_RSHIFT
#endif

#ifndef SDL_SCANCODE_LCTRL
#define SDL_SCANCODE_LCTRL SDLK_LCTRL
#endif

#ifndef SDL_SCANCODE_RCTRL
#define SDL_SCANCODE_RCTRL SDLK_RCTRL
#endif

#ifndef SDL_SCANCODE_LALT
#define SDL_SCANCODE_LALT SDLK_LALT
#endif

#ifndef SDL_SCANCODE_RALT
#define SDL_SCANCODE_RALT SDLK_RALT
#endif

#ifndef SDL_SCANCODE_UNKNOWN
#define SDL_SCANCODE_UNKNOWN SDLK_UNKNOWN
#endif

#ifndef SDL_SCANCODE_ESCAPE
#define SDL_SCANCODE_ESCAPE SDLK_ESCAPE
#endif

#ifndef SDL_SCANCODE_BACKSPACE
#define SDL_SCANCODE_BACKSPACE SDLK_BACKSPACE
#endif

#ifndef SDL_SCANCODE_F1
#define SDL_SCANCODE_F1 SDLK_F1
#endif

#ifndef SDL_SCANCODE_F2
#define SDL_SCANCODE_F2 SDLK_F2
#endif

#ifndef SDL_SCANCODE_F3
#define SDL_SCANCODE_F3 SDLK_F3
#endif

#ifndef SDL_SCANCODE_F4
#define SDL_SCANCODE_F4 SDLK_F4
#endif

#ifndef SDL_SCANCODE_F5
#define SDL_SCANCODE_F5 SDLK_F5
#endif

#ifndef SDL_SCANCODE_F6
#define SDL_SCANCODE_F6 SDLK_F6
#endif

#ifndef SDL_SCANCODE_F7
#define SDL_SCANCODE_F7 SDLK_F7
#endif

#ifndef SDL_SCANCODE_F8
#define SDL_SCANCODE_F8 SDLK_F8
#endif

#ifndef SDL_SCANCODE_F9
#define SDL_SCANCODE_F9 SDLK_F9
#endif

#ifndef SDL_SCANCODE_F10
#define SDL_SCANCODE_F10 SDLK_F10
#endif

#ifndef SDL_SCANCODE_F11
#define SDL_SCANCODE_F11 SDLK_F11
#endif

#ifndef SDL_SCANCODE_F12
#define SDL_SCANCODE_F12 SDLK_F12
#endif

#ifndef KMOD_GUI
#define KMOD_GUI KMOD_META
#endif
#endif

#include <stdbool.h>

#define SDL_POLL_INTERVAL 10

extern JE_boolean ESCPressed;
extern JE_boolean newkey, newmouse, keydown, mousedown;
extern SDL_Scancode lastkey_scan;
extern SDL_Keymod lastkey_mod;
extern Uint8 lastmouse_but;
extern Sint32 lastmouse_x, lastmouse_y;
extern JE_boolean mouse_pressed[4];
extern Sint32 mouse_x, mouse_y;

#ifdef WITH_SDL3
extern Uint8 keysactive[SDL_SCANCODE_COUNT];
#else
extern Uint8 keysactive[SDL_NUM_SCANCODES];
#endif

extern bool windowHasFocus;

extern bool new_text;
extern char last_text[SDL_TEXTINPUTEVENT_TEXT_SIZE];

void flush_events_buffer(void);
void wait_input(JE_boolean keyboard, JE_boolean mouse, JE_boolean joystick);
void wait_noinput(JE_boolean keyboard, JE_boolean mouse, JE_boolean joystick);
void init_keyboard(void);

#ifdef WITH_SDL3
void mouseSetRelative(SDL_Window *window, bool enable);
#else
void mouseSetRelative(bool enable);
#endif

JE_word JE_mousePosition(JE_word *mouseX, JE_word *mouseY);
void mouseGetRelativePosition(Sint32 *out_x, Sint32 *out_y);

void service_SDL_events(JE_boolean clear_new);

void sleep_game(void);

void JE_clearKeyboard(void);

#endif /* KEYBOARD_H */
