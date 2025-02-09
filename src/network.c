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
#include "network.h"

#include "episodes.h"
#include "fonthand.h"
#include "helptext.h"
#include "joystick.h"
#include "keyboard.h"
#include "mainint.h"
#include "nortvars.h"
#include "opentyr.h"
#include "picload.h"
#include "sprite.h"
#include "varz.h"
#include "video.h"

#include <assert.h>

#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
#define SDLNet_GetError SDL_GetError
#endif

/*                              HERE BE DRAGONS!
 *
 * When I wrote this code I thought it was wonderful... that thought was very
 * wrong.  It works, but good luck understanding how... I don't anymore.
 *
 * Hopefully it'll be rewritten some day.
 */

#define NET_VERSION       2            // increment whenever networking changes might create incompatibility
#define NET_PORT          1333         // UDP

#define NET_PACKET_SIZE   256
#define NET_PACKET_QUEUE  16

#define NET_RETRY         640          // ticks to wait for packet acknowledgment before resending
#define NET_RESEND        320          // ticks to wait before requesting unreceived game packet
#define NET_KEEP_ALIVE    1600         // ticks to wait between keep-alive packets
#define NET_TIME_OUT      16000        // ticks to wait before considering connection dead

bool isNetworkGame = false;
int network_delay = 1 + 1;  // minimum is 1 + 0

char *network_opponent_host = NULL;

Uint16 network_player_port = NET_PORT,
       network_opponent_port = NET_PORT;

static char empty_string[] = "";
char *network_player_name = empty_string,
     *network_opponent_name = empty_string;

#ifdef WITH_NETWORK
#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
static SDLNet_DatagramSocket *socket;
static SDLNet_Address *ip;

SDLNet_Datagram *packet_out_temp;
static SDLNet_Datagram *packet_temp;

SDLNet_Datagram *packet_in[NET_PACKET_QUEUE] = { NULL },
                *packet_out[NET_PACKET_QUEUE] = { NULL };

static Uint16 last_out_sync = 0, queue_in_sync = 0, queue_out_sync = 0, last_ack_sync = 0;
static Uint32 last_in_tick = 0, last_out_tick = 0;

SDLNet_Datagram *packet_state_in[NET_PACKET_QUEUE] = { NULL };
static SDLNet_Datagram *packet_state_in_xor[NET_PACKET_QUEUE] = { NULL };
SDLNet_Datagram *packet_state_out[NET_PACKET_QUEUE] = { NULL };
#else
static UDPsocket socket;
static IPaddress ip;

UDPpacket *packet_out_temp;
static UDPpacket *packet_temp;

UDPpacket *packet_in[NET_PACKET_QUEUE] = { NULL },
          *packet_out[NET_PACKET_QUEUE] = { NULL };

static Uint16 last_out_sync = 0, queue_in_sync = 0, queue_out_sync = 0, last_ack_sync = 0;
static Uint32 last_in_tick = 0, last_out_tick = 0;

UDPpacket *packet_state_in[NET_PACKET_QUEUE] = { NULL };
static UDPpacket *packet_state_in_xor[NET_PACKET_QUEUE] = { NULL };
UDPpacket *packet_state_out[NET_PACKET_QUEUE] = { NULL };
#endif

static Uint16 last_state_in_sync = 0, last_state_out_sync = 0;
static Uint32 last_state_in_tick = 0;

static bool net_initialized = false;
static bool connected = false, quit = false;
#endif

uint thisPlayerNum = 0;  /* Player number on this PC (1 or 2) */

JE_boolean haltGame = false;

JE_boolean moveOk;

/* Special Requests */
JE_boolean pauseRequest, skipLevelRequest, helpRequest, nortShipRequest;
JE_boolean yourInGameMenuRequest, inGameMenuRequest;

#ifdef WITH_NETWORK
#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
static void packet_copy(SDLNet_Datagram *dst, SDLNet_Datagram *src)
{
    void *temp = dst->buf;
    memcpy(dst, src, sizeof(*dst));
    dst->buf = temp;
    memcpy(dst->buf, src->buf, src->buflen);
}
#else
static void packet_copy(UDPpacket *dst, UDPpacket *src)
{
    void *temp = dst->data;
    memcpy(dst, src, sizeof(*dst));
    dst->data = temp;
    memcpy(dst->data, src->data, src->len);
}
#endif

#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
static void packets_shift_up(SDLNet_Datagram **packet, int max_packets)
#else
static void packets_shift_up(UDPpacket **packet, int max_packets)
#endif
{
		if (packet[0])
		{
#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
            SDLNet_DestroyDatagram(packet[0]);
#else
			SDLNet_FreePacket(packet[0]);
#endif
		}
		for (int i = 0; i < max_packets - 1; i++)
		{
			packet[i] = packet[i + 1];
		}
		packet[max_packets - 1] = NULL;
}

#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
static void packets_shift_down(SDLNet_Datagram **packet, int max_packets)
#else
static void packets_shift_down(UDPpacket **packet, int max_packets)
#endif
{
	if (packet[max_packets - 1])
	{
#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
        SDLNet_DestroyDatagram(packet[max_packets - 1]);
#else
		SDLNet_FreePacket(packet[max_packets - 1]);
#endif
	}
	for (int i = max_packets - 1; i > 0; i--)
	{
		packet[i] = packet[i - 1];
	}
	packet[0] = NULL;
}

// prepare new packet for sending
void network_prepare(Uint16 type)
{
#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
    if (packet_out_temp == NULL)
    {
        return;
    }

    SDLNet_Write16(type,          &packet_out_temp->buf[0]);
    SDLNet_Write16(last_out_sync, &packet_out_temp->buf[2]);
#else
	SDLNet_Write16(type,          &packet_out_temp->data[0]);
	SDLNet_Write16(last_out_sync, &packet_out_temp->data[2]);
#endif
}

// send packet but don't expect acknowledgment of delivery
static bool network_send_no_ack(int len)
{
#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
    if (packet_out_temp == NULL)
    {
        return false;
    }

    packet_out_temp->buflen = len;

    if (packet_out_temp->buf)
    {
        packet_out_temp->addr = SDLNet_RefAddress(ip);
        packet_out_temp->port = network_opponent_port;
        if (SDLNet_SendDatagram(socket, packet_out_temp->addr, packet_out_temp->port, packet_out_temp->buf, packet_out_temp->buflen) == false)
        {
            fprintf(stderr, "SDLNet_SendDatagram: %s, host: %s, port: %d, length: %d (3)\n", SDL_GetError(), SDLNet_GetAddressString(ip), network_opponent_port, packet_out_temp->buflen);
#else
            packet_out_temp->len = len;

        if (!SDLNet_UDP_Send(socket, 0, packet_out_temp))
        {
            fprintf(stderr, "SDLNet_UDP_Send: %s (3)\n", SDL_GetError());
#endif
            return false;
        }
#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
    } else {
        fprintf(stderr, "network_send_no_ack: Packet buffer is NULL (3)\n");
        return false;
    }
#endif

	return true;
}

// send packet and place it in queue to be acknowledged
bool network_send(int len)
{
	bool temp = network_send_no_ack(len);

	Uint16 i = last_out_sync - queue_out_sync;
	if (i < NET_PACKET_QUEUE)
	{
#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
        packet_out[i] = malloc(sizeof(*packet_out[i]));

        if (packet_out[i] != NULL)
        {
            packet_out[i]->buf = malloc(NET_PACKET_SIZE);
            packet_out[i]->buflen = NET_PACKET_SIZE;
        }
#else
		packet_out[i] = SDLNet_AllocPacket(NET_PACKET_SIZE);
#endif

		packet_copy(packet_out[i], packet_out_temp);
	}
	else
	{
		// connection is probably bad now
		fprintf(stderr, "warning: outbound packet queue overflow\n");
		return false;
	}

	last_out_sync++;

	if (network_is_sync())
    {
        last_out_tick = SDL_GetTicks();
    }

	return temp;
}

// send acknowledgment packet
static int network_acknowledge(Uint16 sync)
{
#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
    if (packet_out_temp == NULL)
    {
        return 0;
    }

    SDLNet_Write16(PACKET_ACKNOWLEDGE, &packet_out_temp->buf[0]);
    SDLNet_Write16(sync,               &packet_out_temp->buf[2]);
#else
	SDLNet_Write16(PACKET_ACKNOWLEDGE, &packet_out_temp->data[0]);
	SDLNet_Write16(sync,               &packet_out_temp->data[2]);
#endif

	network_send_no_ack(4);

	return 0;
}

// activity lately?
static bool network_is_alive(void)
{
	return (SDL_GetTicks() - last_in_tick < NET_TIME_OUT || SDL_GetTicks() - last_state_in_tick < NET_TIME_OUT);
}

// poll for new packets received, check that connection is alive, resend queued packets if necessary
int network_check(void)
{
	if (!net_initialized)
		return -1;

	if (connected)
	{
		// timeout
		if (!network_is_alive())
		{
			if (!quit)
				network_tyrian_halt(2, false);
		}

		// keep-alive
		static Uint32 keep_alive_tick = 0;
		if (SDL_GetTicks() - keep_alive_tick > NET_KEEP_ALIVE)
		{
			network_prepare(PACKET_KEEP_ALIVE);
			network_send_no_ack(4);

			keep_alive_tick = SDL_GetTicks();
		}
	}

	// retry
	if (packet_out[0] && SDL_GetTicks() - last_out_tick > NET_RETRY)
	{
#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
        if (packet_out[0]->buf)
        {
            packet_out[0]->addr = SDLNet_RefAddress(ip);
            packet_out[0]->port = network_opponent_port;
            if (SDLNet_SendDatagram(socket, packet_out[0]->addr, packet_out[0]->port, packet_out[0]->buf, packet_out[0]->buflen) == false)
            {
                fprintf(stderr, "SDLNet_SendDatagram: %s, host: %s, port: %d, length: %d (4)\n", SDL_GetError(), SDLNet_GetAddressString(ip), network_opponent_port, packet_out[0]->buflen);
#else
            if (!SDLNet_UDP_Send(socket, 0, packet_out[0]))
            {
                fprintf(stderr, "SDLNet_UDP_Send: %s (4)\n", SDL_GetError());
#endif
                return -1;
            }
#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
        } else {
            fprintf(stderr, "network_check: Packet buffer is NULL (4)\n");
            return -1;
        }
#endif

		last_out_tick = SDL_GetTicks();
	}

#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
    SDLNet_Datagram *packet_temp_net = NULL;
	switch ((int)(SDLNet_ReceiveDatagram(socket, &packet_temp_net) && (packet_temp_net != NULL)))
#else
	switch (SDLNet_UDP_Recv(socket, packet_temp))
#endif
	{
#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
        case 0:
            if (packet_temp != NULL)
            {
                fprintf(stderr, "SDLNet_ReceiveDatagram: %s\n", SDL_GetError());
            }
#else
		case -1:
            fprintf(stderr, "SDLNet_UDP_Recv: %s\n", SDL_GetError());
#endif
			return -1;
			break;
#if !defined(WITH_SDL3) || defined(WITH_SDL2NET)
		case 0:
            break;
#endif
#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
        case 1:
#endif
		default:
#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
            packet_copy(packet_temp, packet_temp_net);

            if (packet_temp->buflen >= 4)
            {
                switch (SDLNet_Read16(&packet_temp->buf[0]))
#else
			if (packet_temp->channel == 0 && packet_temp->len >= 4)
			{
				switch (SDLNet_Read16(&packet_temp->data[0]))
#endif
				{
					case PACKET_ACKNOWLEDGE:
#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
                        if ((Uint16)(SDLNet_Read16(&packet_temp->buf[2]) - last_ack_sync) < NET_PACKET_QUEUE)
#else
						if ((Uint16)(SDLNet_Read16(&packet_temp->data[2]) - last_ack_sync) < NET_PACKET_QUEUE)
#endif
						{
#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
                            last_ack_sync = SDLNet_Read16(&packet_temp->buf[2]);
#else
							last_ack_sync = SDLNet_Read16(&packet_temp->data[2]);
#endif
						}

						{
#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
                            Uint16 i = SDLNet_Read16(&packet_temp->buf[2]) - queue_out_sync;
#else
							Uint16 i = SDLNet_Read16(&packet_temp->data[2]) - queue_out_sync;
#endif
							if (i < NET_PACKET_QUEUE)
							{
								if (packet_out[i])
								{
#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
                                    SDLNet_DestroyDatagram(packet_out[i]);
#else
									SDLNet_FreePacket(packet_out[i]);
#endif

									packet_out[i] = NULL;
								}
							}
						}

						// remove acknowledged packets from queue
						while (packet_out[0] == NULL && (Uint16)(last_ack_sync - queue_out_sync) < NET_PACKET_QUEUE)
						{
							packets_shift_up(packet_out, NET_PACKET_QUEUE);

							queue_out_sync++;
						}

						last_in_tick = SDL_GetTicks();
						break;

					case PACKET_CONNECT:
#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
                        queue_in_sync = SDLNet_Read16(&packet_temp->buf[2]);
#else
						queue_in_sync = SDLNet_Read16(&packet_temp->data[2]);
#endif

						for (int i = 0; i < NET_PACKET_QUEUE; i++)
						{
							if (packet_in[i])
							{
#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
                                SDLNet_DestroyDatagram(packet_in[i]);
#else
								SDLNet_FreePacket(packet_in[i]);
#endif
								packet_in[i] = NULL;
							}
						}
						// fall through

					case PACKET_DETAILS:
					case PACKET_WAITING:
					case PACKET_BUSY:
					case PACKET_GAME_QUIT:
					case PACKET_GAME_PAUSE:
					case PACKET_GAME_MENU:
						{
#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
                            Uint16 i = SDLNet_Read16(&packet_temp->buf[2]) - queue_in_sync;
#else
							Uint16 i = SDLNet_Read16(&packet_temp->data[2]) - queue_in_sync;
#endif
							if (i < NET_PACKET_QUEUE)
							{
								if (packet_in[i] == NULL)
                                {
#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
                                    packet_in[i] = malloc(sizeof(*packet_in[i]));

                                    if (packet_in[i] != NULL)
                                    {
                                        packet_in[i]->buf = malloc(NET_PACKET_SIZE);
                                        packet_in[i]->buflen = NET_PACKET_SIZE;
                                    }
#else
                                    packet_in[i] = SDLNet_AllocPacket(NET_PACKET_SIZE);
#endif
                                }

								packet_copy(packet_in[i], packet_temp);
							}
							else
							{
								// inbound packet queue overflow/underflow
								// under normal circumstances, this is okay
							}
						}

#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
                        network_acknowledge(SDLNet_Read16(&packet_temp->buf[2]));
#else
						network_acknowledge(SDLNet_Read16(&packet_temp->data[2]));
#endif
						// fall through

					case PACKET_KEEP_ALIVE:
						last_in_tick = SDL_GetTicks();
						break;

					case PACKET_QUIT:
						if (!quit)
						{
							network_prepare(PACKET_QUIT);
							network_send(4);  // PACKET_QUIT
						}

#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
                        network_acknowledge(SDLNet_Read16(&packet_temp->buf[2]));
#else
						network_acknowledge(SDLNet_Read16(&packet_temp->data[2]));
#endif

						if (!quit)
							network_tyrian_halt(1, true);
						break;

					case PACKET_STATE:
						// place packet in queue if within limits
						{
#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
                            Uint16 i = SDLNet_Read16(&packet_temp->buf[2]) - last_state_in_sync + 1;
#else
							Uint16 i = SDLNet_Read16(&packet_temp->data[2]) - last_state_in_sync + 1;
#endif
							if (i < NET_PACKET_QUEUE)
							{
								if (packet_state_in[i] == NULL)
                                {
#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
                                    packet_state_in[i] = malloc(sizeof(*packet_state_in[i]));

                                    if (packet_state_in[i] != NULL)
                                    {
                                        packet_state_in[i]->buf = malloc(NET_PACKET_SIZE);
                                        packet_state_in[i]->buflen = NET_PACKET_SIZE;
                                    }
#else
                                    packet_state_in[i] = SDLNet_AllocPacket(NET_PACKET_SIZE);
#endif
                                }

								packet_copy(packet_state_in[i], packet_temp);
							}
						}
						break;

					case PACKET_STATE_XOR:
						// place packet in queue if within limits
						{
#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
                            Uint16 i = SDLNet_Read16(&packet_temp->buf[2]) - last_state_in_sync + 1;
#else
							Uint16 i = SDLNet_Read16(&packet_temp->data[2]) - last_state_in_sync + 1;
#endif
							if (i < NET_PACKET_QUEUE)
							{
								if (packet_state_in_xor[i] == NULL)
								{
#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
                                    packet_state_in_xor[i] = malloc(sizeof(*packet_state_in_xor[i]));

                                    if (packet_state_in_xor[i] != NULL)
                                    {
                                        packet_state_in_xor[i]->buf = malloc(NET_PACKET_SIZE);
                                        packet_state_in_xor[i]->buflen = NET_PACKET_SIZE;
                                    }
#else
									packet_state_in_xor[i] = SDLNet_AllocPacket(NET_PACKET_SIZE);
#endif

									packet_copy(packet_state_in_xor[i], packet_temp);
								}
#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
                                else if (SDLNet_Read16(&packet_state_in_xor[i]->buf[0]) != PACKET_STATE_XOR)
                                {
                                    for (int j = 4; j < packet_state_in_xor[i]->buflen; j++)
                                        packet_state_in_xor[i]->buf[j] ^= packet_temp->buf[j];
                                    SDLNet_Write16(PACKET_STATE_XOR, &packet_state_in_xor[i]->buf[0]);
                                }
#else
								else if (SDLNet_Read16(&packet_state_in_xor[i]->data[0]) != PACKET_STATE_XOR)
								{
									for (int j = 4; j < packet_state_in_xor[i]->len; j++)
										packet_state_in_xor[i]->data[j] ^= packet_temp->data[j];
									SDLNet_Write16(PACKET_STATE_XOR, &packet_state_in_xor[i]->data[0]);
								}
#endif
							}
						}
						break;

					case PACKET_STATE_RESEND:
						// resend requested state packet if still available
						{
#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
                            Uint16 i = last_state_out_sync - SDLNet_Read16(&packet_temp->buf[2]);
#else
							Uint16 i = last_state_out_sync - SDLNet_Read16(&packet_temp->data[2]);
#endif
							if (i > 0 && i < NET_PACKET_QUEUE)
							{
								if (packet_state_out[i])
								{
#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
                                    if (packet_state_out[i]->buf)
                                    {
                                        packet_state_out[i]->addr = SDLNet_RefAddress(ip);
                                        packet_state_out[i]->port = network_opponent_port;
                                        if (SDLNet_SendDatagram(socket, packet_state_out[i]->addr, packet_state_out[i]->port, packet_state_out[i]->buf, packet_state_out[i]->buflen) == false)
                                        {
                                            fprintf(stderr, "SDLNet_SendDatagram: %s, host: %s, port: %d, length: %d (5)\n", SDL_GetError(), SDLNet_GetAddressString(ip), network_opponent_port, packet_state_out[i]->buflen);
#else
                                        if (!SDLNet_UDP_Send(socket, 0, packet_state_out[i]))
                                        {
                                            fprintf(stderr, "SDLNet_UDP_Send: %s (5)\n", SDL_GetError());
#endif
                                            return -1;
                                        }
#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
                                    } else {
                                        fprintf(stderr, "network_check: Packet buffer is NULL (5)\n");
                                        return -1;
                                    }
#endif
								}
							}
						}
						break;

					default:
#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
                        fprintf(stderr, "warning: bad packet %d received\n", SDLNet_Read16(&packet_temp->buf[0]));
#else
						fprintf(stderr, "warning: bad packet %d received\n", SDLNet_Read16(&packet_temp->data[0]));
#endif
						return 0;
						break;
				}

				return 1;
			}
			break;
	}

	return 0;
}

// discard working packet, now processing next packet in queue
bool network_update(void)
{
	if (packet_in[0])
	{
		packets_shift_up(packet_in, NET_PACKET_QUEUE);

		queue_in_sync++;

		return true;
	}

	return false;
}

// has opponent gotten all the packets we've sent?
bool network_is_sync(void)
{
	return (queue_out_sync - last_ack_sync == 1);
}

// prepare new state for sending
void network_state_prepare(void)
{
	if (packet_state_out[0])
	{
		fprintf(stderr, "warning: state packet overwritten (previous packet remains unsent)\n");
	}
	else
	{
#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
        packet_state_out[0] = malloc(sizeof(*packet_state_out[0]));

        if (packet_state_out[0] != NULL)
        {
            packet_state_out[0]->buf = malloc(NET_PACKET_SIZE);
            packet_state_out[0]->buflen = 28;
        }
#else
		packet_state_out[0] = SDLNet_AllocPacket(NET_PACKET_SIZE);
        packet_state_out[0]->len = 28;
#endif
	}

#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
    SDLNet_Write16(PACKET_STATE, &packet_state_out[0]->buf[0]);
    SDLNet_Write16(last_state_out_sync, &packet_state_out[0]->buf[2]);

    memset(&packet_state_out[0]->buf[4], 0, 28 - 4);
#else
	SDLNet_Write16(PACKET_STATE, &packet_state_out[0]->data[0]);
	SDLNet_Write16(last_state_out_sync, &packet_state_out[0]->data[2]);

    memset(&packet_state_out[0]->data[4], 0, 28 - 4);
#endif
}

// send state packet, xor packet if applicable
int network_state_send(void)
{
#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
    if (packet_state_out[0]->buf)
    {
        packet_state_out[0]->addr = SDLNet_RefAddress(ip);
        packet_state_out[0]->port = network_opponent_port;
        if (SDLNet_SendDatagram(socket, packet_state_out[0]->addr, packet_state_out[0]->port, packet_state_out[0]->buf, packet_state_out[0]->buflen) == false)
        {
            fprintf(stderr, "SDLNet_SendDatagram: %s, host: %s, port: %d, length: %d (1)\n", SDL_GetError(), SDLNet_GetAddressString(ip), network_opponent_port, packet_state_out[0]->buflen);
#else
        if (!SDLNet_UDP_Send(socket, 0, packet_state_out[0]))
        {
            fprintf(stderr, "SDLNet_UDP_Send: %s (1)\n", SDL_GetError());
#endif
            return -1;
        }
#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
    } else {
        fprintf(stderr, "network_state_send: Packet buffer is NULL (1)\n");
        return -1;
    }
#endif

	// send xor of last network_delay packets
	if (network_delay > 1 && (last_state_out_sync + 1) % network_delay == 0 && packet_state_out[network_delay - 1] != NULL)
	{
		packet_copy(packet_temp, packet_state_out[0]);
#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
        SDLNet_Write16(PACKET_STATE_XOR, &packet_temp->buf[0]);
#else
		SDLNet_Write16(PACKET_STATE_XOR, &packet_temp->data[0]);
#endif
		for (int i = 1; i < network_delay; i++)
        {
#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
            for (int j = 4; j < packet_temp->buflen; j++)
            {
                packet_temp->buf[j] ^= packet_state_out[i]->buf[j];
            }
        }

        if (packet_temp->buf)
        {
            packet_temp->addr = SDLNet_RefAddress(ip);
            packet_temp->port = network_opponent_port;
            if (SDLNet_SendDatagram(socket, packet_temp->addr, packet_temp->port, packet_temp->buf, packet_temp->buflen) == false)
            {
                fprintf(stderr, "SDLNet_SendDatagram: %s, host: %s, port: %d, length: %d (2)\n", SDL_GetError(), SDLNet_GetAddressString(ip), network_opponent_port, packet_temp->buflen);
#else
            for (int j = 4; j < packet_temp->len; j++)
            {
                packet_temp->data[j] ^= packet_state_out[i]->data[j];
            }
        }

            if (!SDLNet_UDP_Send(socket, 0, packet_temp))
            {
                fprintf(stderr, "SDLNet_UDP_Send: %s (2)\n", SDL_GetError());
#endif
                return -1;
            }
#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
        } else {
            fprintf(stderr, "network_state_send: Packet buffer is NULL (2)\n");
            return -1;
        }
#endif
	}

	packets_shift_down(packet_state_out, NET_PACKET_QUEUE);

	last_state_out_sync++;

	return 0;
}

// receive state packet, wait until received
bool network_state_update(void)
{
	if (network_state_is_reset())
	{
		return 0;
	}
	else
	{
		packets_shift_up(packet_state_in, NET_PACKET_QUEUE);

		packets_shift_up(packet_state_in_xor, NET_PACKET_QUEUE);

		last_state_in_sync++;

		// current xor packet index
		int x = network_delay - (last_state_in_sync - 1) % network_delay - 1;

		// loop until needed packet is available
		while (!packet_state_in[0])
		{
			// xor the packet from thin air, if possible
#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
            if (packet_state_in_xor[x] && SDLNet_Read16(&packet_state_in_xor[x]->buf[0]) == PACKET_STATE_XOR)
#else
			if (packet_state_in_xor[x] && SDLNet_Read16(&packet_state_in_xor[x]->data[0]) == PACKET_STATE_XOR)
#endif
			{
				// check for all other required packets
				bool okay = true;
				for (int i = 1; i <= x; i++)
				{
					if (packet_state_in[i] == NULL)
					{
						okay = false;
						break;
					}
				}
				if (okay)
				{
#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
                    packet_state_in[0] = malloc(sizeof(*packet_state_in[0]));

                    if (packet_state_in[0] != NULL)
                    {
                        packet_state_in[0]->buf = malloc(NET_PACKET_SIZE);
                        packet_state_in[0]->buflen = NET_PACKET_SIZE;
                    }
#else
					packet_state_in[0] = SDLNet_AllocPacket(NET_PACKET_SIZE);
#endif
					packet_copy(packet_state_in[0], packet_state_in_xor[x]);
					for (int i = 1; i <= x; i++)
                    {
#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
                        for (int j = 4; j < packet_state_in[0]->buflen; j++)
                        {
                            packet_state_in[0]->buf[j] ^= packet_state_in[i]->buf[j];
                        }
#else
                        for (int j = 4; j < packet_state_in[0]->len; j++)
                        {
                            packet_state_in[0]->data[j] ^= packet_state_in[i]->data[j];
                        }
#endif
                    }
					break;
				}
			}

			static Uint32 resend_tick = 0;
			if (SDL_GetTicks() - last_state_in_tick > NET_RESEND && SDL_GetTicks() - resend_tick > NET_RESEND)
			{
#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
                SDLNet_Write16(PACKET_STATE_RESEND,    &packet_out_temp->buf[0]);
                SDLNet_Write16(last_state_in_sync - 1, &packet_out_temp->buf[2]);
#else
				SDLNet_Write16(PACKET_STATE_RESEND,    &packet_out_temp->data[0]);
				SDLNet_Write16(last_state_in_sync - 1, &packet_out_temp->data[2]);
#endif

				network_send_no_ack(4);  // PACKET_RESEND

				resend_tick = SDL_GetTicks();
			}

			if (network_check() == 0)
				SDL_Delay(1);
		}

		if (network_delay > 1)
		{
			// process the current in packet against the xor queue
			if (packet_state_in_xor[x] == NULL)
			{
#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
                packet_state_in_xor[x] = malloc(sizeof(*packet_state_in_xor[x]));

                if (packet_state_in_xor[x] != NULL)
                {
                    packet_state_in_xor[x]->buf = malloc(NET_PACKET_SIZE);
                    packet_state_in_xor[x]->buflen = NET_PACKET_SIZE;
                }
#else
				packet_state_in_xor[x] = SDLNet_AllocPacket(NET_PACKET_SIZE);
#endif

				packet_copy(packet_state_in_xor[x], packet_state_in[0]);

#if !defined(WITH_SDL3) || defined(WITH_SDL2NET)
				packet_state_in_xor[x]->status = 0;
#endif
			}
			else
			{
#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
                for (int j = 4; j < packet_state_in_xor[x]->buflen; j++)
                {
                    packet_state_in_xor[x]->buf[j] ^= packet_state_in[0]->buf[j];
                }
#else
				for (int j = 4; j < packet_state_in_xor[x]->len; j++)
                {
                    packet_state_in_xor[x]->data[j] ^= packet_state_in[0]->data[j];
                }
#endif
			}
		}

		last_state_in_tick = SDL_GetTicks();
	}

	return 1;
}

// ignore first network_delay states of level
bool network_state_is_reset(void)
{
	return (last_state_out_sync < network_delay);
}

// reset queues for new level
void network_state_reset(void)
{
	last_state_in_sync = last_state_out_sync = 0;

	for (int i = 0; i < NET_PACKET_QUEUE; i++)
	{
		if (packet_state_in[i])
		{
#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
            SDLNet_DestroyDatagram(packet_state_in[i]);
#else
			SDLNet_FreePacket(packet_state_in[i]);
#endif

			packet_state_in[i] = NULL;
		}
	}
	for (int i = 0; i < NET_PACKET_QUEUE; i++)
	{
		if (packet_state_in_xor[i])
		{
#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
            SDLNet_DestroyDatagram(packet_state_in_xor[i]);
#else
			SDLNet_FreePacket(packet_state_in_xor[i]);
#endif

            packet_state_in_xor[i] = NULL;
		}
	}
	for (int i = 0; i < NET_PACKET_QUEUE; i++)
	{
		if (packet_state_out[i])
		{
#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
            SDLNet_DestroyDatagram(packet_state_out[i]);
#else
			SDLNet_FreePacket(packet_state_out[i]);
#endif

			packet_state_out[i] = NULL;
		}
	}

	last_state_in_tick = SDL_GetTicks();
}

// attempt to punch through firewall by firing off UDP packets at the opponent
// exchange game information
int network_connect(void)
{
#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
    ip = SDLNet_ResolveHostname(network_opponent_host);

    if (ip) {
        if (SDLNet_WaitUntilResolved(ip, -1) < 0) {
            SDLNet_UnrefAddress(ip);
            ip = NULL;
        }
    }

    if (!ip) {
        fprintf(stderr, "network_connect: Resolve host name failed! %s", SDL_GetError());
        return -1;
    }
#else
	SDLNet_ResolveHost(&ip, network_opponent_host, network_opponent_port);

	SDLNet_UDP_Bind(socket, 0, &ip);
#endif

	Uint16 episodes = 0, episodes_local = 0;
	assert(EPISODE_MAX <= 16);
	for (int i = EPISODE_MAX - 1; i >= 0; i--)
	{
		episodes <<= 1;
		episodes |= (episodeAvail[i] != 0);
	}
	episodes_local = episodes;

	assert(NET_PACKET_SIZE - 12 >= 20 + 1);
	if (strlen(network_player_name) > 20)
		network_player_name[20] = '\0';

connect_reset:
	network_prepare(PACKET_CONNECT);
#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
	SDLNet_Write16(NET_VERSION, &packet_out_temp->buf[4]);
	SDLNet_Write16(network_delay,   &packet_out_temp->buf[6]);
	SDLNet_Write16(episodes_local,  &packet_out_temp->buf[8]);
	SDLNet_Write16(thisPlayerNum,   &packet_out_temp->buf[10]);
	strlcpy((char *)&packet_out_temp->buf[12], network_player_name, packet_out_temp->buflen);
#else
	SDLNet_Write16(NET_VERSION, &packet_out_temp->data[4]);
	SDLNet_Write16(network_delay,   &packet_out_temp->data[6]);
	SDLNet_Write16(episodes_local,  &packet_out_temp->data[8]);
	SDLNet_Write16(thisPlayerNum,   &packet_out_temp->data[10]);
	strlcpy((char *)&packet_out_temp->data[12], network_player_name, packet_out_temp->len);
#endif
	network_send(12 + strlen(network_player_name) + 1); // PACKET_CONNECT

	// until opponent sends connect packet
	while (true)
	{
		push_joysticks_as_keyboard();
		service_SDL_events(false);

		if (newkey && lastkey_scan == SDL_SCANCODE_ESCAPE)
			network_tyrian_halt(0, false);

		// never timeout
		last_in_tick = SDL_GetTicks();

#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
		if (packet_in[0] && SDLNet_Read16(&packet_in[0]->buf[0]) == PACKET_CONNECT)
#else
		if (packet_in[0] && SDLNet_Read16(&packet_in[0]->data[0]) == PACKET_CONNECT)
#endif
			break;

		network_update();
		network_check();

		SDL_Delay(16);
	}

connect_again:
#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
    if (SDLNet_Read16(&packet_in[0]->buf[4]) != NET_VERSION)
    {
        fprintf(stderr, "error: network version did not match opponent's\n");
        network_tyrian_halt(4, true);
    }
    if (SDLNet_Read16(&packet_in[0]->buf[6]) != network_delay)
    {
        fprintf(stderr, "error: network delay did not match opponent's\n");
        network_tyrian_halt(5, true);
    }
    if (SDLNet_Read16(&packet_in[0]->buf[10]) == thisPlayerNum)
    {
        fprintf(stderr, "error: player number conflicts with opponent's\n");
        network_tyrian_halt(6, true);
    }

    episodes = SDLNet_Read16(&packet_in[0]->buf[8]);
    for (int i = 0; i < EPISODE_MAX; i++) {
        episodeAvail[i] &= (episodes & 1);
        episodes >>= 1;
    }

    network_opponent_name = malloc(packet_in[0]->buflen - 12 + 1);
    strlcpy(network_opponent_name, (char *)&packet_in[0]->buf[12], packet_in[0]->buflen);
#else
	if (SDLNet_Read16(&packet_in[0]->data[4]) != NET_VERSION)
	{
		fprintf(stderr, "error: network version did not match opponent's\n");
		network_tyrian_halt(4, true);
	}
	if (SDLNet_Read16(&packet_in[0]->data[6]) != network_delay)
	{
		fprintf(stderr, "error: network delay did not match opponent's\n");
		network_tyrian_halt(5, true);
	}
	if (SDLNet_Read16(&packet_in[0]->data[10]) == thisPlayerNum)
	{
		fprintf(stderr, "error: player number conflicts with opponent's\n");
		network_tyrian_halt(6, true);
	}

	episodes = SDLNet_Read16(&packet_in[0]->data[8]);
	for (int i = 0; i < EPISODE_MAX; i++) {
		episodeAvail[i] &= (episodes & 1);
		episodes >>= 1;
	}

	network_opponent_name = malloc(packet_in[0]->len - 12 + 1);
	strlcpy(network_opponent_name, (char *)&packet_in[0]->data[12], packet_in[0]->len);
#endif

	network_update();

	// until opponent has acknowledged
	while (!network_is_sync())
	{
		service_SDL_events(false);

#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
        // got a duplicate packet; process it again (but why?)
        if (packet_in[0] && SDLNet_Read16(&packet_in[0]->buf[0]) == PACKET_CONNECT)
#else
		// got a duplicate packet; process it again (but why?)
		if (packet_in[0] && SDLNet_Read16(&packet_in[0]->data[0]) == PACKET_CONNECT)
#endif
			goto connect_again;

		network_check();

		// maybe opponent didn't get our packet
		if (SDL_GetTicks() - last_out_tick > NET_RETRY)
			goto connect_reset;

		SDL_Delay(16);
	}

	// send another packet since sometimes the network syncs without both connect packets exchanged
	// there should be a better way to handle this
	network_prepare(PACKET_CONNECT);
    
#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
    if (packet_out_temp == NULL)
    {
        return -1;
    }

    SDLNet_Write16(NET_VERSION, &packet_out_temp->buf[4]);
    SDLNet_Write16(network_delay,   &packet_out_temp->buf[6]);
    SDLNet_Write16(episodes_local,  &packet_out_temp->buf[8]);
    SDLNet_Write16(thisPlayerNum,   &packet_out_temp->buf[10]);
    strlcpy((char *)&packet_out_temp->buf[12], network_player_name, packet_out_temp->buflen);
#else
	SDLNet_Write16(NET_VERSION, &packet_out_temp->data[4]);
	SDLNet_Write16(network_delay,   &packet_out_temp->data[6]);
	SDLNet_Write16(episodes_local,  &packet_out_temp->data[8]);
	SDLNet_Write16(thisPlayerNum,   &packet_out_temp->data[10]);
	strlcpy((char *)&packet_out_temp->data[12], network_player_name, packet_out_temp->len);
#endif

	network_send(12 + strlen(network_player_name) + 1); // PACKET_CONNECT

	connected = true;

	return 0;
}

// something has gone wrong :(
void network_tyrian_halt(unsigned int err, bool attempt_sync)
{
	const char *const err_msg[] = {
		"Quitting...",
		"Other player quit the game.",
		"Network connection was lost.",
		"Network connection failed.",
		"Network version mismatch.",
		"Network delay mismatch.",
		"Network player number conflict.",
	};

	quit = true;

	if (err >= COUNTOF(err_msg))
		err = 0;

	fade_black(10);

	VGAScreen = VGAScreenSeg;

	JE_loadPic(VGAScreen, 2, false);
	JE_dString(VGAScreen, JE_fontCenter(err_msg[err], SMALL_FONT_SHAPES), 140, err_msg[err], SMALL_FONT_SHAPES);

	JE_showVGA();
	fade_palette(colors, 10, 0, 255);

	if (attempt_sync)
	{
		while (!network_is_sync() && network_is_alive())
		{
			service_SDL_events(false);

			network_check();
			SDL_Delay(16);
		}
	}

	if (err)
	{
		while (!JE_anyButton())
			SDL_Delay(16);
	}

	fade_black(10);

#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
    SDLNet_DestroyDatagramSocket(socket);
#endif

	SDLNet_Quit();

	JE_tyrianHalt(5);
}

int network_init(void)
{
	printf("Initializing network...\n");

	if (network_delay * 2 > NET_PACKET_QUEUE - 2)
	{
		fprintf(stderr, "error: network delay would overflow packet queue\n");
		return -4;
	}

#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
    if (SDLNet_Init() == false)
#else
	if (SDLNet_Init() == -1)
#endif
	{
		fprintf(stderr, "error: SDLNet_Init: %s\n", SDLNet_GetError());
		return -1;
	}

#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
    socket = SDLNet_CreateDatagramSocket(NULL, network_player_port);
#else
	socket = SDLNet_UDP_Open(network_player_port);
#endif

	if (!socket)
	{
		fprintf(stderr, "error: SDLNet_UDP_Open: %s\n", SDLNet_GetError());
		return -2;
	}

#if defined(WITH_SDL3) && !defined(WITH_SDL2NET)
    packet_temp = malloc(sizeof(*packet_temp));

    if (packet_temp != NULL)
    {
        packet_temp->buf = malloc(NET_PACKET_SIZE);
        packet_temp->buflen = NET_PACKET_SIZE;
    }

    packet_out_temp = malloc(sizeof(*packet_out_temp));

    if (packet_out_temp != NULL)
    {
        packet_out_temp->buf = malloc(NET_PACKET_SIZE);
        packet_out_temp->buflen = NET_PACKET_SIZE;
    }
#else
	packet_temp = SDLNet_AllocPacket(NET_PACKET_SIZE);
	packet_out_temp = SDLNet_AllocPacket(NET_PACKET_SIZE);
#endif

	if (!packet_temp || !packet_out_temp)
	{
		fprintf(stderr, "SDLNet_AllocPacket: %s\n", SDLNet_GetError());
		return -3;
	}

	net_initialized = true;

	return 0;
}

#endif

void JE_clearSpecialRequests(void)
{
	pauseRequest = false;
	inGameMenuRequest = false;
	skipLevelRequest = false;
	helpRequest = false;
	nortShipRequest = false;
}
