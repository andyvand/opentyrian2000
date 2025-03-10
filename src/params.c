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
#include "params.h"

#include "arg_parse.h"
#include "file.h"
#include "joystick.h"
#include "loudness.h"
#include "network.h"
#include "opentyr.h"
#include "varz.h"
#include "xmas.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>

#ifndef SDL_NET_HINT_IP_DEFAULT_VERSION
#define SDL_NET_HINT_IP_DEFAULT_VERSION "SDL_NET_HINT_IP_DEFAULT_VERSION"
#endif

JE_boolean richMode = false, constantPlay = false, constantDie = false;

/* YKS: Note: LOOT cheat had non letters removed. */
const char pars[][9] = {
	"LOOT", "RECORD", "NOJOY", "CONSTANT", "DEATH", "NOSOUND", "NOXMAS", "YESXMAS"
};

void JE_paramCheck(int argc, char *argv[])
{
	const Options options[] =
	{
		{ 'h', 'h', "help",              false },

		{ 's', 's', "no-sound",          false },
		{ 'j', 'j', "no-joystick",       false },
		{ 'x', 'x', "no-xmas",           false },

		{ 't', 't', "data",              true },
		{ 'f', 'f', "soundfont",         true },

		{ 'n', 'n', "net",               true },
		{ 256, 0,   "net-player-name",   true }, // TODO: no short codes because there should
		{ 257, 0,   "net-player-number", true }, //       be a menu for entering these in the future
#ifndef WITH_SDL
        { 258, 0,   "net-type",          true },
#endif
		{ 'p', 'p', "net-port",          true },
		{ 'd', 'd', "net-delay",         true },

		{ 'X', 'X', "xmas",              false },
		{ 'c', 'c', "constant",          false },
		{ 'k', 'k', "death",             false },
		{ 'r', 'r', "record",            false },
		{ 'l', 'l', "loot",              false },

		{ 0, 0, NULL, false}
	};

	Option option;

	for (; ; )
	{
		option = parse_args(argc, (const char **)argv, options);
		
		if (option.value == NOT_OPTION)
			break;
		
		switch (option.value)
		{
		case INVALID_OPTION:
		case AMBIGUOUS_OPTION:
		case OPTION_MISSING_ARG:
			_fprintf(stderr, "Try `%s --help' for more information.\n", argv[0]);
			exit(EXIT_FAILURE);
			break;
			
		case 'h':
			printf("Usage: %s [OPTION...]\n\n"
			       "Options:\n"
			       "  -h, --help                   Show help about options\n\n"
			       "  -s, --no-sound               Disable audio\n"
			       "  -j, --no-joystick            Disable joystick/gamepad input\n"
			       "  -x, --no-xmas                Disable Christmas mode\n\n"
			       "  -t, --data=DIR               Set Tyrian data directory\n\n"
			       "  -n, --net=HOST               Start a networked game\n"
			       "  --net-player-name=NAME       Sets local player name in a networked game\n"
			       "  --net-player-number=NUMBER   Sets local player number in a networked game\n"
			       "                               (1 or 2)\n"
#ifndef WITH_SDL
                   "  --net-type=[ipv4|ipv6]       Sets network type for socket\n"
#endif
			       "  -p, --net-port=PORT          Local port to bind (default is 1333)\n"
			       "  -d, --net-delay=FRAMES       Set lag-compensation delay (default is 1)\n"
				   "  -f, --soundfont=FILE         Set the soundfont for MIDI playback\n\n"
				   , argv[0]);
			exit(0);
			break;
			
		case 's':
			// Disables sound/music usage
			audio_disabled = true;
			break;
		case 'f':
			// Set the soundfont for MIDI playback
			strlcpy(soundfont, option.arg, MAX(strlen(option.arg) + 1, 4096));
			break;
		case 'j':
			// Disables joystick detection
			ignore_joystick = true;
			break;
			
		case 'x':
			override_xmas = true;
			xmas = false;
			break;
			
		case 'X':
			override_xmas = true;
			xmas = true;
			break;
			
		// set custom Tyrian data directory
		case 't':
			custom_data_dir = option.arg;
			break;
			
		case 'n':
			isNetworkGame = true;
            network_opponent_host = malloc(strlen(option.arg) + 1);
            strlcpy(network_opponent_host, option.arg, strlen(option.arg) + 1);
			break;

		case 256: // --net-player-name
			network_player_name = malloc(strlen(option.arg) + 1);
			strlcpy(network_player_name, option.arg, (strlen(option.arg) + 1));
			break;

		case 257: // --net-player-number
		{
			int temp = atoi(option.arg);
			if (temp >= 1 && temp <= 2)
				thisPlayerNum = temp;
			else
			{
				_fprintf(stderr, "%s: error: invalid network player number\n", argv[0]);
				exit(EXIT_FAILURE);
			}
			break;
		}

#ifndef WITH_SDL
        case 258:
            if (strncmp(option.arg, "ipv4", 4) == 0 || strncmp(option.arg, "ipv6", 4) == 0)
            {
                SDL_SetHint(SDL_NET_HINT_IP_DEFAULT_VERSION, option.arg);
            } else {
                _fprintf(stderr, "%s: error: invalid network type\n", option.arg);
                exit(EXIT_FAILURE);
            }
            break;
#endif

		case 'p':
		{
			int temp = atoi(option.arg);
			if (temp > 0 && temp < 49152)
				network_player_port = temp;
			else
			{
				_fprintf(stderr, "%s: error: invalid network port number\n", argv[0]);
				exit(EXIT_FAILURE);
			}
			break;
		}
		case 'd':
		{
			int temp = 0;

#if defined(_MSC_VER) && __STDC_WANT_SECURE_LIB__
            if (sscanf_s(option.arg, "%d", &temp) == 1)
#else
			if (sscanf(option.arg, "%d", &temp) == 1)
#endif
				network_delay = 1 + temp;
			else
			{
				_fprintf(stderr, "%s: error: invalid network delay value\n", argv[0]);
				exit(EXIT_FAILURE);
			}
			break;
		}

        case 'c':
			/* Constant play for testing purposes (C key activates invincibility)
			   This might be useful for publishers to see everything - especially
			   those who can't play it */
			constantPlay = true;
			break;
			
		case 'k':
			constantDie = true;
			break;
			
		case 'r':
			record_demo = true;
			break;
			
		case 'l':
			// Gives you mucho bucks
			richMode = true;
			break;
			
		default:
			assert(false);
			break;
		}
	}
	
	// legacy parameter support
	for (int i = option.argn; i < argc; ++i)
	{
		for (uint j = 0; j < strlen(argv[i]); ++j)
			argv[i][j] = toupper((unsigned char)argv[i][j]);
		
		for (uint j = 0; j < COUNTOF(pars); ++j)
		{
			if (strcmp(argv[i], pars[j]) == 0)
			{
				switch (j)
				{
				case 0:
					richMode = true;
					break;
				case 1:
					record_demo = true;
					break;
				case 2:
					ignore_joystick = true;
					break;
				case 3:
					constantPlay = true;
					break;
				case 4:
					constantDie = true;
					break;
				case 5:
					audio_disabled = true;
					break;
				case 6:
					override_xmas = true;
					xmas = false;
					break;
				case 7:
					override_xmas = true;
					xmas = true;
					break;
					
				default:
					assert(false);
					break;
				}
			}
		}
	}
}
