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
#include "keyboard.h"

#include "config.h"
#include "joystick.h"
#include "mouse.h"
#include "network.h"
#include "opentyr.h"
#include "video.h"
#include "video_scale.h"
#include "font.h"

#if defined(ANDROID) || defined(__ANDROID__) || defined(IOS) || defined(VITA) || defined(WITH_SDL)
#include "player.h"
#include "network.h"
#endif

#ifdef WITH_SDL3
#include <SDL3/SDL.h>
#else
#ifdef WITH_SDL
#include <SDL.h>
#else
#include <SDL2/SDL.h>
#endif
#endif

#include <stdio.h>

JE_boolean ESCPressed;

JE_boolean newkey, newmouse, keydown, mousedown;
SDL_Scancode lastkey_scan;
SDL_Keymod lastkey_mod;
Uint8 lastmouse_but;
Sint32 lastmouse_x, lastmouse_y;
JE_boolean mouse_pressed[4] = {false, false, false, false};
Sint32 mouse_x, mouse_y;
bool windowHasFocus = false;

#ifdef WITH_SDL3
Uint8 keysactive[SDL_SCANCODE_COUNT] = { 0 };
#else
Uint8 keysactive[SDL_NUM_SCANCODES] = { 0 };
#endif

bool new_text;
char last_text[SDL_TEXTINPUTEVENT_TEXT_SIZE];

static bool mouseRelativeEnabled;

// Relative mouse position in window coordinates.
static Sint32 mouseWindowXRelative;
static Sint32 mouseWindowYRelative;

void flush_events_buffer(void)
{
	SDL_Event ev;

	while (SDL_PollEvent(&ev));
}

void wait_input(JE_boolean keyboard, JE_boolean mouse, JE_boolean joystick)
{
	service_SDL_events(false);
	while (!((keyboard && keydown) || (mouse && mousedown) || (joystick && joydown)))
	{
		SDL_Delay(SDL_POLL_INTERVAL);
		push_joysticks_as_keyboard();
		service_SDL_events(false);

#ifdef WITH_NETWORK
		if (isNetworkGame)
			network_check();
#endif
	}
}

void wait_noinput(JE_boolean keyboard, JE_boolean mouse, JE_boolean joystick)
{
	service_SDL_events(false);
	while ((keyboard && keydown) || (mouse && mousedown) || (joystick && joydown))
	{
		SDL_Delay(SDL_POLL_INTERVAL);
		poll_joysticks();
		service_SDL_events(false);

#ifdef WITH_NETWORK
		if (isNetworkGame)
			network_check();
#endif
	}
}

void init_keyboard(void)
{
	//SDL_EnableKeyRepeat(500, 60); TODO Find if SDL2 has an equivalent.

	newkey = newmouse = false;
	keydown = mousedown = false;

#ifdef WITH_SDL
    inputInit();
#endif

#ifdef WITH_SDL3
    SDL_HideCursor();
#else
	SDL_ShowCursor(SDL_FALSE);
#endif

#ifndef WITH_SDL
#if SDL_VERSION_ATLEAST(2, 26, 0)
	SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_SYSTEM_SCALE, "1");
#endif
#ifdef WITH_SDL3
    SDL_SetHint(SDL_HINT_MOUSE_EMULATE_WARP_WITH_RELATIVE, "0");
#endif
#endif
}

#ifdef WITH_SDL3
void mouseSetRelative(SDL_Window *window, bool enable)
#else
void mouseSetRelative(bool enable)
#endif
{
#if !defined(ANDROID) && !defined(__ANDROID__) && !defined(IOS) && !defined(VITA) && !defined(WITH_SDL)
#ifdef WITH_SDL3
    SDL_SetWindowRelativeMouseMode(window, enable && windowHasFocus);
#else
	SDL_SetRelativeMouseMode(enable && windowHasFocus);
#endif
#else
#ifdef WITH_SDL3
    (void)window;
#endif
#endif

    mouseRelativeEnabled = enable;

	mouseWindowXRelative = 0;
	mouseWindowYRelative = 0;
}

JE_word JE_mousePosition(JE_word *mouseX, JE_word *mouseY)
{
	service_SDL_events(false);
	*mouseX = mouse_x;
	*mouseY = mouse_y;
	return mousedown ? lastmouse_but : 0;
}

void mouseGetRelativePosition(Sint32 *const out_x, Sint32 *const out_y)
{
	service_SDL_events(false);

    scaleWindowDistanceToScreen(&mouseWindowXRelative, &mouseWindowYRelative);
	*out_x = mouseWindowXRelative;
	*out_y = mouseWindowYRelative;

    mouseWindowXRelative = 0;
	mouseWindowYRelative = 0;
}

void service_SDL_events(JE_boolean clear_new)
{
	SDL_Event ev;
#ifdef WITH_SDL3
    Sint32 mx = 0;
    Sint32 my = 0;
#endif

#if defined(WITH_SDL3) || defined(WITH_SDL)
    Sint32 mxrel = 0;
    Sint32 myrel = 0;
#endif

#ifndef WITH_SDL
    Sint32 bx = 0;
    Sint32 by = 0;
#endif

	if (clear_new)
	{
		newkey = false;
		newmouse = false;
		new_text = false;
	}

#ifndef WITH_SDL
	while (SDL_PollEvent(&ev))
	{
		switch (ev.type)
		{
#ifndef WITH_SDL3
			case SDL_WINDOWEVENT:
				switch (ev.window.event)
                {
#endif
#ifdef WITH_SDL3
                case SDL_EVENT_WINDOW_FOCUS_LOST:
                        windowHasFocus = false;

                        mouseSetRelative(SDL_GetWindowFromID(ev.window.windowID), mouseRelativeEnabled);
#else
				case SDL_WINDOWEVENT_FOCUS_LOST:
                        windowHasFocus = false;

                        mouseSetRelative(mouseRelativeEnabled);
#endif
					break;

#ifdef WITH_SDL3
                case SDL_EVENT_WINDOW_FOCUS_GAINED:
                        windowHasFocus = true;

                        mouseSetRelative(SDL_GetWindowFromID(ev.window.windowID), mouseRelativeEnabled);
#else
				case SDL_WINDOWEVENT_FOCUS_GAINED:
                        windowHasFocus = true;

                        mouseSetRelative(mouseRelativeEnabled);
#endif
					break;

#ifdef WITH_SDL3
                case SDL_EVENT_WINDOW_RESIZED:
#else
				case SDL_WINDOWEVENT_RESIZED:
#endif
					video_on_win_resize();
					break;
#ifndef WITH_SDL3
				}
				break;
#endif

#ifdef WITH_SDL3
            case SDL_EVENT_KEY_DOWN:
                if (ev.key.mod & SDL_KMOD_ALT && ev.key.scancode == SDL_SCANCODE_RETURN)
#else
			case SDL_KEYDOWN:
#ifndef WITH_SDL
                if (ev.key.keysym.mod & KMOD_ALT && ev.key.keysym.scancode == SDL_SCANCODE_RETURN)
#endif
#endif
#ifndef WITH_SDL
				/* <alt><enter> toggle fullscreen */
				{
					toggle_fullscreen();
					break;
				}
#endif

#ifdef WITH_SDL3
                keysactive[ev.key.scancode] = 1;
#else
				keysactive[ev.key.keysym.scancode] = 1;
#endif

				newkey = true;
#ifdef WITH_SDL3
                lastkey_scan = ev.key.scancode;
                lastkey_mod = ev.key.mod;
#else
				lastkey_scan = ev.key.keysym.scancode;
				lastkey_mod = ev.key.keysym.mod;
#endif
				keydown = true;
                mouseInactive = false;
				return;

#ifdef WITH_SDL3
            case SDL_EVENT_KEY_UP:
                keysactive[ev.key.scancode] = 0;
#else
			case SDL_KEYUP:
                keysactive[ev.key.keysym.scancode] = 0;
#endif
				keydown = false;
				return;

#ifdef WITH_SDL3
            case SDL_EVENT_MOUSE_MOTION:
                mx = (Sint32)ev.motion.x;
                my = (Sint32)ev.motion.y;
                mouse_x = mx;
                mouse_y = my;
#else
			case SDL_MOUSEMOTION:
                mouse_x = ev.motion.x;
                mouse_y = ev.motion.y;
#endif

				mapWindowPointToScreen(&mouse_x, &mouse_y);

				if (mouseRelativeEnabled && windowHasFocus)
				{
#ifdef WITH_SDL3
#if defined(ANDROID) || defined(__ANDROID__) || defined(IOS) || defined(VITA) || defined(WITH_SDL)
                    if (isNetworkGame)
                    {
                        mxrel = mouse_x - player[thisPlayerNum ? thisPlayerNum - 1 : 0].x;
                        myrel = mouse_y - player[thisPlayerNum ? thisPlayerNum - 1 : 0].y;
                    } else if (twoPlayerMode) {
                        mxrel = mouse_x - player[mousePlayerNumber ? mousePlayerNumber - 1 : 0].x;
                        myrel = mouse_y - player[mousePlayerNumber ? mousePlayerNumber - 1 : 0].y;
                    } else {
                        mxrel = mouse_x - player[0].x;
                        myrel = mouse_y - player[0].y;
                    }

                    mouseWindowXRelative += mxrel;
                    mouseWindowYRelative += myrel;
#else
                    mxrel = (Sint32)ev.motion.xrel;
                    myrel = (Sint32)ev.motion.yrel;
					mouseWindowXRelative += mxrel;
					mouseWindowYRelative += myrel;
#endif
#else
                    mouseWindowXRelative += ev.motion.xrel;
                    mouseWindowYRelative += ev.motion.yrel;
#endif
                }

				// Show system mouse pointer if outside screen.
#ifdef WITH_SDL3
                if (mouse_x < 0 || mouse_x >= vga_width ||
                    mouse_y < 0 || mouse_y >= vga_height ? true : false)
                {
                    SDL_ShowCursor();
                } else {
                    SDL_HideCursor();
                }
#else
				SDL_ShowCursor(mouse_x < 0 || mouse_x >= vga_width ||
				               mouse_y < 0 || mouse_y >= vga_height ? SDL_TRUE : SDL_FALSE);
#endif

#ifdef WITH_SDL3
                if (mxrel != 0 || myrel != 0)
#else
				if (ev.motion.xrel != 0 || ev.motion.yrel != 0)
#endif
					mouseInactive = false;
				break;

#ifdef WITH_SDL3
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
#else
			case SDL_MOUSEBUTTONDOWN:
#endif
				mouseInactive = false;

				// fall through
#ifdef WITH_SDL3
            case SDL_EVENT_MOUSE_BUTTON_UP:
                bx = (Sint32)ev.button.x;
                by = (Sint32)ev.button.y;

                mapWindowPointToScreen((Sint32 *)&bx, (Sint32 *)&by);

#else
			case SDL_MOUSEBUTTONUP:
                bx = ev.button.x;
                by = ev.button.y;

                mapWindowPointToScreen((Sint32 *)&bx, (Sint32 *)&by);
#endif

#ifdef WITH_SDL3
                if (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
#else
				if (ev.type == SDL_MOUSEBUTTONDOWN)
#endif
				{
					newmouse = true;
					lastmouse_but = ev.button.button;
                    lastmouse_x = bx;
                    lastmouse_y = by;
					mousedown = true;
				}
				else
				{
					mousedown = false;
				}

				int whichMB = -1;
				switch (ev.button.button)
				{
					case SDL_BUTTON_LEFT:   whichMB = 0; break;
					case SDL_BUTTON_RIGHT:  whichMB = 1; break;
					case SDL_BUTTON_MIDDLE: whichMB = 2; break;
				}

				if (whichMB < 0)
					break;

				switch (mouseSettings[whichMB])
				{
					case 1: // Fire Main Weapons
						mouse_pressed[0] = mousedown;
						break;
					case 2: // Fire Left Sidekick
						mouse_pressed[1] = mousedown;
						break;
					case 3: // Fire Right Sidekick
						mouse_pressed[2] = mousedown;
						break;
					case 4: // Fire BOTH Sidekicks
						mouse_pressed[1] = mousedown;
						mouse_pressed[2] = mousedown;
						break;
					case 5: // Change Rear Mode
						mouse_pressed[3] = mousedown;
						break;
				}
				break;

#ifdef WITH_SDL3
            case SDL_EVENT_TEXT_INPUT:
#else
			case SDL_TEXTINPUT:
#endif
				SDL_strlcpy(last_text, ev.text.text, COUNTOF(last_text));
				new_text = true;
				break;

#ifdef WITH_SDL3
            case SDL_EVENT_TEXT_EDITING:
#else
			case SDL_TEXTEDITING:
#endif
				break;

#ifdef WITH_SDL3
            case SDL_EVENT_QUIT:
#else
			case SDL_QUIT:
#endif
				/* TODO: Call the cleanup code here. */
				exit(0);
				break;
		}
	}
#else
    while(SDL_PollEvent(&ev))
    {
        mouseInactive = false;
        newmouse = true;

        if (ev.motion.state == SDL_PRESSED)
        {
            mouse_x = ev.motion.x;
            mouse_y = ev.motion.y;

            mapWindowPointToScreen(&mouse_x, &mouse_y);

            lastmouse_x = mouse_x;
            lastmouse_y = mouse_y;
        }
                
        if (mouseRelativeEnabled)
        {
            if (isNetworkGame)
            {
                mxrel = mouse_x - player[thisPlayerNum ? thisPlayerNum - 1 : 0].x;
                myrel = mouse_y - player[thisPlayerNum ? thisPlayerNum - 1 : 0].y;
            } else if (twoPlayerMode) {
                mxrel = mouse_x - player[mousePlayerNumber ? mousePlayerNumber - 1 : 0].x;
                myrel = mouse_y - player[mousePlayerNumber ? mousePlayerNumber - 1 : 0].y;
            } else {
                mxrel = mouse_x - player[0].x;
                myrel = mouse_y - player[0].y;
            }

            if (ev.motion.state == SDL_PRESSED)
            {
                mouseWindowXRelative += mxrel;
                mouseWindowYRelative += myrel;
            }
        }

        mousedown = ev.motion.state == SDL_PRESSED ? true : false;
        mouse_pressed[0] = ev.motion.state == SDL_PRESSED ? true : false;

        keysactive[ev.key.keysym.sym] = ev.key.state;
        if(ev.key.state)
        {
            keysactive[ev.key.keysym.scancode] = 1;
        } else {
            keysactive[ev.key.keysym.scancode] = 0;
        }
        keydown = ev.key.state;
        newkey = ev.key.state;
        lastkey_scan = ev.key.keysym.scancode;
        lastkey_mod = ev.key.keysym.mod;
    }
#endif
}

void JE_clearKeyboard(void)
{
	// /!\ Doesn't seems important. I think. D:
}
