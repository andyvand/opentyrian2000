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
#include "joystick.h"

#include "config.h"
#include "config_file.h"
#include "file.h"
#include "keyboard.h"
#include "nortsong.h"
#include "opentyr.h"
#include "params.h"
#include "varz.h"
#include "video.h"

#include <assert.h>
#include <ctype.h>
#include <string.h>

#ifndef LC_ALL_MASK
#define LC_ALL_MASK ((int)(~0))
#endif

#if defined(_MSC_VER) && __STDC_WANT_SECURE_LIB__
#define sscanf(a, ...) sscanf_s(a, __VA_ARGS__)
#endif

int joystick_axis_threshold(int j, int value);
int check_assigned(SDL_Joystick *joystick_handle, const Joystick_assignment assignment[2]);

const char *assignment_to_code(const Joystick_assignment *assignment);
void code_to_assignment(Joystick_assignment *assignment, const char *buffer);

int joystick_repeat_delay = 300; // milliseconds, repeat delay for buttons
bool joydown = false;            // any joystick buttons down, updated by poll_joysticks()
bool ignore_joystick = false;

int joysticks = 0;
Joystick *joystick = NULL;

static const int joystick_analog_max = 32767;

// eliminates axis movement below the threshold
int joystick_axis_threshold(int j, int value)
{
	assert(j < joysticks);
	
	bool negative = value < 0;
	if (negative)
		value = -value;
	
	if (value <= joystick[j].threshold * 1000)
		return 0;
	
	value -= joystick[j].threshold * 1000;
	
	return negative ? -value : value;
}

// converts joystick axis to sane Tyrian-usable value (based on sensitivity)
int joystick_axis_reduce(int j, int value)
{
	assert(j < joysticks);
	
	value = joystick_axis_threshold(j, value);
	
	if (value == 0)
		return 0;
	
	return value / (3000 - 200 * joystick[j].sensitivity);
}

// converts analog joystick axes to an angle
// returns false if axes are centered (there is no angle)
bool joystick_analog_angle(int j, float *angle)
{
	assert(j < joysticks);
	
	float x = joystick_axis_threshold(j, joystick[j].x), y = joystick_axis_threshold(j, joystick[j].y);
	
	if (x != 0)
	{
		*angle += atanf(-y / x);
		*angle += (x < 0) ? -M_PI_2 : M_PI_2;
		return true;
	}
	else if (y != 0)
	{
		*angle += y < 0 ? M_PI : 0;
		return true;
	}
	
	return false;
}

/* gives back value 0..joystick_analog_max indicating that one of the assigned
 * buttons has been pressed or that one of the assigned axes/hats has been moved
 * in the assigned direction
 */
int check_assigned(SDL_Joystick *joystick_handle, const Joystick_assignment assignment[2])
{
	int result = 0;
	
	for (int i = 0; i < 2; i++)
	{
		int temp = 0;
		
		switch (assignment[i].type)
		{
		case NONE:
			continue;
			
		case AXIS:
#ifdef WITH_SDL3
            temp = SDL_GetJoystickAxis(joystick_handle, assignment[i].num);
#else
			temp = SDL_JoystickGetAxis(joystick_handle, assignment[i].num);
#endif

			if (assignment[i].negative_axis)
				temp = -temp;
			break;
		
		case BUTTON:
#ifdef WITH_SDL3
            temp = SDL_GetJoystickButton(joystick_handle, assignment[i].num) == 1 ?
                joystick_analog_max : 0;
#else
			temp = SDL_JoystickGetButton(joystick_handle, assignment[i].num) == 1 ? joystick_analog_max : 0;
#endif
			break;
		
		case HAT:
#ifdef WITH_SDL3
            temp = SDL_GetJoystickHat(joystick_handle, assignment[i].num);
#else
			temp = SDL_JoystickGetHat(joystick_handle, assignment[i].num);
#endif

			if (assignment[i].x_axis)
				temp &= SDL_HAT_LEFT | SDL_HAT_RIGHT;
			else
				temp &= SDL_HAT_UP | SDL_HAT_DOWN;
			
			if (assignment[i].negative_axis)
				temp &= SDL_HAT_LEFT | SDL_HAT_UP;
			else
				temp &= SDL_HAT_RIGHT | SDL_HAT_DOWN;
			
			temp = temp ? joystick_analog_max : 0;
			break;
		}
		
		if (temp > result)
			result = temp;
	}
	
	return result;
}

// updates joystick state
void poll_joystick(int j)
{
	assert(j < joysticks);
	
	if (joystick[j].handle == NULL)
		return;

#ifdef WITH_SDL3
    SDL_UpdateJoysticks();
#else
	SDL_JoystickUpdate();
#endif

	// indicates that a direction/action was pressed since last poll
	joystick[j].input_pressed = false;
	
	// indicates that an direction/action has been held long enough to fake a repeat press
	bool repeat = joystick[j].joystick_delay < SDL_GetTicks();
	
	// update direction state
	for (uint d = 0; d < COUNTOF(joystick[j].direction); d++)
	{
		bool old = joystick[j].direction[d];
		
		joystick[j].analog_direction[d] = check_assigned(joystick[j].handle, joystick[j].assignment[d]);
		joystick[j].direction[d] = joystick[j].analog_direction[d] > (joystick_analog_max / 2);
		joydown |= joystick[j].direction[d];
		
		joystick[j].direction_pressed[d] = joystick[j].direction[d] && (!old || repeat);
		joystick[j].input_pressed |= joystick[j].direction_pressed[d];
	}
	
	joystick[j].x = -joystick[j].analog_direction[3] + joystick[j].analog_direction[1];
	joystick[j].y = -joystick[j].analog_direction[0] + joystick[j].analog_direction[2];
	
	// update action state
	for (uint d = 0; d < COUNTOF(joystick[j].action); d++)
	{
		bool old = joystick[j].action[d];
		
		joystick[j].action[d] = check_assigned(joystick[j].handle, joystick[j].assignment[d + COUNTOF(joystick[j].direction)]) > (joystick_analog_max / 2);
		joydown |= joystick[j].action[d];
		
		joystick[j].action_pressed[d] = joystick[j].action[d] && (!old || repeat);
		joystick[j].input_pressed |= joystick[j].action_pressed[d];
	}
	
	joystick[j].confirm = joystick[j].action[0] || joystick[j].action[4];
	joystick[j].cancel = joystick[j].action[1] || joystick[j].action[5];
	
	// if new input, reset press-repeat delay
	if (joystick[j].input_pressed)
		joystick[j].joystick_delay = (Uint32)(SDL_GetTicks() + joystick_repeat_delay);
}

// updates all joystick states
void poll_joysticks(void)
{
	joydown = false;
	
	for (int j = 0; j < joysticks; j++)
		poll_joystick(j);
}

// sends SDL KEYDOWN and KEYUP events for a key
void push_key(SDL_Scancode key)
{
	SDL_Event e;
	
#ifdef WITH_SDL3
    memset(&e.key.scancode, 0, sizeof(e.key.scancode));

    e.key.scancode = key;
#else
    memset(&e.key.keysym, 0, sizeof(e.key.keysym));

    e.key.keysym.scancode = key;
#endif

#ifdef WITH_SDL3
    e.type = SDL_EVENT_KEY_DOWN;
    SDL_PushEvent(&e);
    
    e.type = SDL_EVENT_KEY_UP;
#else
	e.key.state = SDL_RELEASED;
	
	e.type = SDL_KEYDOWN;
	SDL_PushEvent(&e);
	
	e.type = SDL_KEYUP;
#endif

	SDL_PushEvent(&e);
}

// helps us be lazy by pretending joysticks are a keyboard (useful for menus)
void push_joysticks_as_keyboard(void)
{
	const SDL_Scancode confirm = SDL_SCANCODE_RETURN, cancel = SDL_SCANCODE_ESCAPE;
	const SDL_Scancode direction[4] = { SDL_SCANCODE_UP, SDL_SCANCODE_RIGHT, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT };
	
	poll_joysticks();
	
	for (int j = 0; j < joysticks; j++)
	{
		if (!joystick[j].input_pressed)
			continue;
		
		if (joystick[j].confirm)
			push_key(confirm);
		if (joystick[j].cancel)
			push_key(cancel);
		
		for (uint d = 0; d < COUNTOF(joystick[j].direction_pressed); d++)
		{
			if (joystick[j].direction_pressed[d])
				push_key(direction[d]);
		}
	}
}

// initializes SDL joystick system and loads assignments for joysticks found
void init_joysticks(void)
{
#ifdef WITH_SDL3
    SDL_JoystickID *joyIDs = NULL;
#endif

	if (ignore_joystick)
		return;
	
#ifdef WITH_SDL3
    if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) == false)
#else
	if (SDL_InitSubSystem(SDL_INIT_JOYSTICK))
#endif
	{
		_fprintf(stderr, "warning: failed to initialize joystick system: %s\n", SDL_GetError());
		ignore_joystick = true;
		return;
	}
	
#ifdef WITH_SDL3
    joyIDs = SDL_GetJoysticks(&joysticks);
#else
    SDL_JoystickEventState(SDL_IGNORE);

    joysticks = SDL_NumJoysticks();
#endif

	joystick = malloc(joysticks * sizeof(*joystick));
	
	for (int j = 0; j < joysticks; j++)
	{
		memset(&joystick[j], 0, sizeof(*joystick));

#ifdef WITH_SDL3
        joystick[j].handle = SDL_OpenJoystick(joyIDs[j]);
#else
		joystick[j].handle = SDL_JoystickOpen(j);
#endif

		if (joystick[j].handle != NULL)
		{
#ifdef WITH_SDL3
            _fprintf(stdout, "joystick detected: %s ", SDL_GetJoystickName(joystick[j].handle));
#else
			_fprintf(stdout, "joystick detected: %s ", SDL_JoystickName(joystick[j].handle));
#endif

			_fprintf(stdout, "(%d axes, %d buttons, %d hats)\n",
#ifdef WITH_SDL3
                   SDL_GetNumJoystickAxes(joystick[j].handle),
                   SDL_GetNumJoystickButtons(joystick[j].handle),
                   SDL_GetNumJoystickHats(joystick[j].handle));
#else
                   SDL_JoystickNumAxes(joystick[j].handle),
                   SDL_JoystickNumButtons(joystick[j].handle),
			       SDL_JoystickNumHats(joystick[j].handle));
#endif

			if (!load_joystick_assignments(&opentyrian_config, j))
				reset_joystick_assignments(j);
		}
	}
	
	if (joysticks == 0)
		_fprintf(stdout, "no joysticks detected\n");
}

// deinitializes SDL joystick system and saves joystick assignments
void deinit_joysticks(void)
{
	if (ignore_joystick)
		return;
	
	for (int j = 0; j < joysticks; j++)
	{
		if (joystick[j].handle != NULL)
		{
			save_joystick_assignments(&opentyrian_config, j);

#ifdef WITH_SDL3
            SDL_CloseJoystick(joystick[j].handle);
#else
			SDL_JoystickClose(joystick[j].handle);
#endif
		}
	}
	
	free(joystick);
	
	SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
}

void reset_joystick_assignments(int j)
{
	assert(j < joysticks);
	
	// defaults: first 2 axes, first hat, first 6 buttons
	for (uint a = 0; a < COUNTOF(joystick[j].assignment); a++)
	{
		// clear assignments
		for (uint i = 0; i < COUNTOF(joystick[j].assignment[a]); i++)
			joystick[j].assignment[a][i].type = NONE;
		
		if (a < 4)
		{
#ifdef WITH_SDL3
            if (SDL_GetNumJoystickAxes(joystick[j].handle) >= 2)
#else
			if (SDL_JoystickNumAxes(joystick[j].handle) >= 2)
#endif
			{
				joystick[j].assignment[a][0].type = AXIS;
				joystick[j].assignment[a][0].num = (a + 1) % 2;
				joystick[j].assignment[a][0].negative_axis = (a == 0 || a == 3);
			}

#ifdef WITH_SDL3
            if (SDL_GetNumJoystickHats(joystick[j].handle) >= 1)
#else
			if (SDL_JoystickNumHats(joystick[j].handle) >= 1)
#endif
			{
				joystick[j].assignment[a][1].type = HAT;
				joystick[j].assignment[a][1].num = 0;
				joystick[j].assignment[a][1].x_axis = (a == 1 || a == 3);
				joystick[j].assignment[a][1].negative_axis = (a == 0 || a == 3);
			}
#if defined(VITA) || defined(PSP)
            static const char remap[4] = { 8, 9, 6, 7 }; // maps to correct button (UP, RIGHT, DOWN, LEFT)

            joystick[j].assignment[a][1].type = BUTTON;
            joystick[j].assignment[a][1].num = remap[a];
#endif
		}
#if !defined(VITA) && !defined(PSP)
		else
		{
#ifdef WITH_SDL3
            if (a - 4 < (unsigned)SDL_GetNumJoystickButtons(joystick[j].handle))
#else
			if (a - 4 < (unsigned)SDL_JoystickNumButtons(joystick[j].handle))
#endif
			{
				joystick[j].assignment[a][0].type = BUTTON;
				joystick[j].assignment[a][0].num = a - 4;
			}
		}
#endif
	}

#if defined(VITA) || defined(PSP)
    joystick[j].assignment[4][0].type = BUTTON;
    joystick[j].assignment[4][0].num = 2; // X (fire)

    joystick[j].assignment[5][0].type = BUTTON;
    joystick[j].assignment[5][0].num = 1; // O (change fire)

    joystick[j].assignment[6][0].type = BUTTON;
    joystick[j].assignment[6][0].num = 4; // L (left sidekick)

    joystick[j].assignment[7][0].type = BUTTON;
    joystick[j].assignment[7][0].num = 5; // R (right sidekick)

    joystick[j].assignment[8][0].type = BUTTON;
    joystick[j].assignment[8][0].num = 10; // SELECT (menu)

    joystick[j].assignment[9][0].type = BUTTON;
    joystick[j].assignment[9][0].num = 11; // START (pause)
#elif defined(__3DS__)
    joystick[j].assignment[4][0].type = BUTTON;
    joystick[j].assignment[4][0].num = 10; // A (fire)

    joystick[j].assignment[5][0].type = BUTTON;
    joystick[j].assignment[5][0].num = 11; // B (change fire)

    joystick[j].assignment[6][0].type = BUTTON;
    joystick[j].assignment[6][0].num = 0; // X (left sidekick)

    joystick[j].assignment[7][0].type = BUTTON;
    joystick[j].assignment[7][0].num = 1; // Y (right sidekick)

    joystick[j].assignment[8][0].type = BUTTON;
    joystick[j].assignment[8][0].num = 2; // back (menu)

    joystick[j].assignment[9][0].type = BUTTON;
    joystick[j].assignment[9][0].num = 3; // START (pause)
#endif

	joystick[j].analog = false;
	joystick[j].sensitivity = 5;
	joystick[j].threshold = 5;
}

static const char* const assignment_names[] =
{
	"up",
	"right",
	"down",
	"left",
	"fire",
	"change fire",
	"left sidekick",
	"right sidekick",
	"menu",
	"pause",
};

bool load_joystick_assignments(Config *config, int j)
{
#ifdef WITH_SDL3
	ConfigSection *section = config_find_section(config, "joystick", SDL_GetJoystickName(joystick[j].handle));
#else
    ConfigSection *section = config_find_section(config, "joystick", SDL_JoystickName(joystick[j].handle));
#endif

	if (section == NULL)
		return false;
	
	if (!config_get_bool_option(section, "analog", &joystick[j].analog))
		joystick[j].analog = false;
	
	joystick[j].sensitivity = config_get_or_set_int_option(section, "sensitivity", 5);

	joystick[j].threshold = config_get_or_set_int_option(section, "threshold", 5);
	
	for (size_t a = 0; a < COUNTOF(assignment_names); ++a)
	{
		for (unsigned int i = 0; i < COUNTOF(joystick[j].assignment[a]); ++i)
			joystick[j].assignment[a][i].type = NONE;
		
		ConfigOption *option = config_get_option(section, assignment_names[a]);
		if (option == NULL)
			continue;
		
		foreach_option_i_value(i, value, option)
		{
			if (i >= COUNTOF(joystick[j].assignment[a]))
				break;
			
			code_to_assignment(&joystick[j].assignment[a][i], value);
		}
	}
	
	return true;
}

bool save_joystick_assignments(Config *config, int j)
{
#ifdef WITH_SDL3
    ConfigSection *section = config_find_or_add_section(config, "joystick", SDL_GetJoystickName(joystick[j].handle));
#else
	ConfigSection *section = config_find_or_add_section(config, "joystick", SDL_JoystickName(joystick[j].handle));
#endif

	if (section == NULL)
		exit(EXIT_FAILURE);  // out of memory
	
	config_set_bool_option(section, "analog", joystick[j].analog, NO_YES);
	
	config_set_int_option(section, "sensitivity", joystick[j].sensitivity);
	
	config_set_int_option(section, "threshold", joystick[j].threshold);
	
	for (size_t a = 0; a < COUNTOF(assignment_names); ++a)
	{
		ConfigOption *option = config_set_option(section, assignment_names[a], NULL);
		if (option == NULL)
			exit(EXIT_FAILURE);  // out of memory
		
		option = config_set_value(option, NULL);
		if (option == NULL)
			exit(EXIT_FAILURE);  // out of memory

		for (size_t i = 0; i < COUNTOF(joystick[j].assignment[a]); ++i)
		{
			if (joystick[j].assignment[a][i].type == NONE)
				continue;
			
			option = config_add_value(option, assignment_to_code(&joystick[j].assignment[a][i]));
			if (option == NULL)
				exit(EXIT_FAILURE);  // out of memory
		}
	}
	
	return true;
}

// fills buffer with comma separated list of assigned joystick functions
void joystick_assignments_to_string(char *buffer, size_t buffer_len, const Joystick_assignment *assignments)
{
	strlcpy(buffer, "", buffer_len);
	
	bool comma = false;
	for (uint i = 0; i < COUNTOF(*joystick->assignment); ++i)
	{
		if (assignments[i].type == NONE)
			continue;
		
		size_t len = snprintf(buffer, buffer_len, "%s%s",
		                      comma ? ", " : "",
		                      assignment_to_code(&assignments[i]));
		buffer += len;
		buffer_len -= len;
		
		comma = true;
	}
}

// reverse of assignment_to_code()
void code_to_assignment(Joystick_assignment *assignment, const char *buffer)
{
	memset(assignment, 0, sizeof(*assignment));
	
	char axis = 0, direction = 0;
	
	if (sscanf(buffer, " AX %d%c", &assignment->num, &direction) == 2)
		assignment->type = AXIS;
	else if (sscanf(buffer, " BTN %d", &assignment->num) == 1)
		assignment->type = BUTTON;
	else if (sscanf(buffer, " H %d%c%c", &assignment->num, &axis, &direction) == 3)
		assignment->type = HAT;
	
	if (assignment->num == 0)
		assignment->type = NONE;
	else
		--assignment->num;
	
	assignment->x_axis = (toupper(axis) == 'X');
	assignment->negative_axis = (toupper(direction) == '-');
}

/* gives the short (6 or less characters) identifier for a joystick assignment
 * 
 * two of these per direction/action is all that can fit on the joystick config screen,
 * assuming two digits for the axis/button/hat number
 */
const char *assignment_to_code(const Joystick_assignment *assignment)
{
	static char name[7];
	
	switch (assignment->type)
	{
	case NONE:
		strlcpy(name, "", sizeof(name));
		break;
		
	case AXIS:
		snprintf(name, sizeof(name), "AX %d%c",
		         assignment->num + 1,
		         assignment->negative_axis ? '-' : '+');
		break;
		
	case BUTTON:
		snprintf(name, sizeof(name), "BTN %d",
		         assignment->num + 1);
		break;
		
	case HAT:
		snprintf(name, sizeof(name), "H %d%c%c",
		         assignment->num + 1,
		         assignment->x_axis ? 'X' : 'Y',
		         assignment->negative_axis ? '-' : '+');
		break;
	}
	
	return name;
}

// captures joystick input for configuring assignments
// returns false if non-joystick input was detected
// TODO: input from joystick other than the one being configured probably should not be ignored
bool detect_joystick_assignment(int j, Joystick_assignment *assignment)
{
	// get initial joystick state to compare against to see if anything was pressed

#ifdef WITH_SDL3
    const int axes = SDL_GetNumJoystickAxes(joystick[j].handle);
#else
	const int axes = SDL_JoystickNumAxes(joystick[j].handle);
#endif

	Sint16 *axis = malloc(axes * sizeof(*axis));
	for (int i = 0; i < axes; i++)
#ifdef WITH_SDL3
        axis[i] = SDL_GetJoystickAxis(joystick[j].handle, i);
#else
		axis[i] = SDL_JoystickGetAxis(joystick[j].handle, i);
#endif
	
#ifdef WITH_SDL3
    const int buttons = SDL_GetNumJoystickButtons(joystick[j].handle);
#else
	const int buttons = SDL_JoystickNumButtons(joystick[j].handle);
#endif

	Uint8 *button = malloc(buttons * sizeof(*button));
	for (int i = 0; i < buttons; i++)
#ifdef WITH_SDL3
        button[i] = SDL_GetJoystickButton(joystick[j].handle, i);

    const int hats = SDL_GetNumJoystickHats(joystick[j].handle);
#else
		button[i] = SDL_JoystickGetButton(joystick[j].handle, i);

    const int hats = SDL_JoystickNumHats(joystick[j].handle);
#endif

	Uint8 *hat = malloc(hats * sizeof(*hat));
	for (int i = 0; i < hats; i++)
#ifdef WITH_SDL3
        hat[i] = SDL_GetJoystickHat(joystick[j].handle, i);
#else
		hat[i] = SDL_JoystickGetHat(joystick[j].handle, i);
#endif

	
	bool detected = false;
	
	do
	{
		setDelay(1);

#ifdef WITH_SDL3
        SDL_UpdateJoysticks();
#else
		SDL_JoystickUpdate();
#endif

		for (int i = 0; i < axes; ++i)
		{
#ifdef WITH_SDL3
            Sint16 temp = SDL_GetJoystickAxis(joystick[j].handle, i);
#else
			Sint16 temp = SDL_JoystickGetAxis(joystick[j].handle, i);
#endif

			if (abs(temp - axis[i]) > joystick_analog_max * 2 / 3)
			{
				assignment->type = AXIS;
				assignment->num = i;
				assignment->negative_axis = temp < axis[i];
				detected = true;
				break;
			}
		}
		
		for (int i = 0; i < buttons; ++i)
		{
#ifdef WITH_SDL3
            Uint8 new_button = SDL_GetJoystickButton(joystick[j].handle, i),
#else
			Uint8 new_button = SDL_JoystickGetButton(joystick[j].handle, i),
#endif
			      changed = button[i] ^ new_button;
			
			if (!changed)
				continue;
			
			if (new_button == 0) // button was released
			{
				button[i] = new_button;
			}
			else                 // button was pressed
			{
				assignment->type = BUTTON;
				assignment->num = i;
				detected = true;
				break;
			}
		}
		
		for (int i = 0; i < hats; ++i)
		{
#ifdef WITH_SDL3
            Uint8 new_hat = SDL_GetJoystickHat(joystick[j].handle, i),
#else
			Uint8 new_hat = SDL_JoystickGetHat(joystick[j].handle, i),
#endif
			      changed = hat[i] ^ new_hat;
			
			if (!changed)
				continue;
			
			if ((new_hat & changed) == SDL_HAT_CENTERED) // hat was centered
			{
				hat[i] = new_hat;
			}
			else
			{
				assignment->type = HAT;
				assignment->num = i;
				assignment->x_axis = changed & (SDL_HAT_LEFT | SDL_HAT_RIGHT);
				assignment->negative_axis = changed & (SDL_HAT_LEFT | SDL_HAT_UP);
				detected = true;
			}
		}
		
		service_SDL_events(true);
		JE_showVGA();
		
		wait_delay();
	} while (!detected && !newkey && !newmouse);
	
	free(axis);
	free(button);
	free(hat);
	
	return detected;
}

// compares relevant parts of joystick assignments for equality
bool joystick_assignment_cmp(const Joystick_assignment *a, const Joystick_assignment *b)
{
	if (a->type == b->type)
	{
		switch (a->type)
		{
		case NONE:
			return true;
		case AXIS:
			return (a->num == b->num) &&
			       (a->negative_axis == b->negative_axis);
		case BUTTON:
			return (a->num == b->num);
		case HAT:
			return (a->num == b->num) &&
			       (a->x_axis == b->x_axis) &&
			       (a->negative_axis == b->negative_axis);
		}
	}
	
	return false;
}
