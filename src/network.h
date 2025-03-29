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
#ifndef NETWORK_H
#define NETWORK_H

#include "opentyr.h"

#ifdef WITH_SDL3
#   include <SDL3/SDL.h>
#else
#ifdef WITH_SDL
#   include <SDL.h>
#else
#   include <SDL2/SDL.h>
#endif
#endif

#ifdef WITH_NETWORK
/*#ifndef WITH_SDL2NET
#define WITH_SDL2NET 1
#endif*/

#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
#include "SDL3_net/SDL_net.h"

SDL_FORCE_INLINE void SDLNet_Write16(Uint16 value, void *areap)
{
    *(Uint16 *)areap = SDL_Swap16BE(value);
}

SDL_FORCE_INLINE void SDLNet_Write32(Uint32 value, void *areap)
{
    *(Uint32 *)areap = SDL_Swap32BE(value);
}

SDL_FORCE_INLINE Uint16 SDLNet_Read16(const void *areap)
{
    return SDL_Swap16BE(*(const Uint16 *)areap);
}

SDL_FORCE_INLINE Uint32 SDLNet_Read32(const void *areap)
{
    return SDL_Swap32BE(*(const Uint32 *)areap);
}

#   else
#      ifdef WITH_SDL
#          include <SDL_net.h>
#      else
#          ifdef WITH_SDL3_ESP
#              include "SDL_net.h"
#          else
#	           include "SDL2/SDL_net.h"
#          endif
#      endif
#   endif
#endif

#define PACKET_ACKNOWLEDGE   0x00    // 
#define PACKET_KEEP_ALIVE    0x01    // 

#define PACKET_CONNECT       0x10    // version, delay, episodes, player_number, name
#define PACKET_DETAILS       0x11    // episode, difficulty

#define PACKET_QUIT          0x20    // 
#define PACKET_WAITING       0x21    // 
#define PACKET_BUSY          0x22    // 

#define PACKET_GAME_QUIT     0x30    // 
#define PACKET_GAME_PAUSE    0x31    // 
#define PACKET_GAME_MENU     0x32    // 

#define PACKET_STATE_RESEND  0x40    // state_id
#define PACKET_STATE         0x41    // <state>  (not acknowledged)
#define PACKET_STATE_XOR     0x42    // <xor state>  (not acknowledged)

extern bool isNetworkGame;
extern int network_delay;

extern char *network_opponent_host;
extern Uint16 network_player_port, network_opponent_port;
extern char *network_player_name, *network_opponent_name;

#ifdef WITH_NETWORK
#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
extern SDLNet_Datagram *packet_out_temp;
extern SDLNet_Datagram *packet_in[], *packet_out[],
*packet_state_in[], *packet_state_out[];
#else
extern UDPpacket *packet_out_temp;
extern UDPpacket *packet_in[], *packet_out[],
                 *packet_state_in[], *packet_state_out[];
#endif
#endif

extern uint thisPlayerNum;
extern JE_boolean haltGame;
extern JE_boolean moveOk;
extern JE_boolean pauseRequest, skipLevelRequest, helpRequest, nortShipRequest;
extern JE_boolean yourInGameMenuRequest, inGameMenuRequest;

#ifdef WITH_NETWORK
void network_prepare(Uint16 type);
bool network_send(int len);

int network_check(void);
bool network_update(void);

bool network_is_sync(void);

void network_state_prepare(void);
int network_state_send(void);
bool network_state_update(void);
bool network_state_is_reset(void);
void network_state_reset(void);

int network_connect(void);
void network_tyrian_halt(unsigned int err, bool attempt_sync);

int network_init(void);

void JE_clearSpecialRequests(void);

#define NETWORK_KEEP_ALIVE() \
		if (isNetworkGame) \
			network_check();
#else
#define NETWORK_KEEP_ALIVE()
#endif

#endif /* NETWORK_H */
