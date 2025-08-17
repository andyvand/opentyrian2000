#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "libwav.h"

#define SFX_COUNT 31
#define VOICE_COUNT 9
#define SOUND_COUNT (SFX_COUNT + VOICE_COUNT)

enum
{
	S_NONE             =  0,
	S_WEAPON_1         =  1,
	S_WEAPON_2         =  2,
	S_ENEMY_HIT        =  3,
	S_EXPLOSION_4      =  4,
	S_WEAPON_5         =  5,
	S_WEAPON_6         =  6,
	S_WEAPON_7         =  7,
	S_SELECT           =  8,
	S_EXPLOSION_8      =  8,
	S_EXPLOSION_9      =  9,
	S_WEAPON_10        = 10,
	S_EXPLOSION_11     = 11,
	S_EXPLOSION_12     = 12,
	S_WEAPON_13        = 13,
	S_WEAPON_14        = 14,
	S_WEAPON_15        = 15,
	S_SPRING           = 16,
	S_WARNING          = 17,
	S_ITEM             = 18,
	S_HULL_HIT         = 19,
	S_MACHINE_GUN      = 20,
	S_SOUL_OF_ZINGLON  = 21,
	S_EXPLOSION_22     = 22,
	S_CLINK            = 23,
	S_CLICK            = 24,
	S_WEAPON_25        = 25,
	S_WEAPON_26        = 26,
	S_SHIELD_HIT       = 27,
	S_CURSOR           = 28,
	S_POWERUP          = 29,
	S_MARS3            = 30,
	S_NEEDLE2          = 31,
	V_CLEARED_PLATFORM = 32,  // "Cleared enemy platform."
	V_BOSS             = 33,  // "Large enemy approaching."
	V_ENEMIES          = 34,  // "Enemies ahead."
	V_GOOD_LUCK        = 35,  // "Good luck."
	V_LEVEL_END        = 36,  // "Level completed."
	V_DANGER           = 37,  // "Danger."
	V_SPIKES           = 38,  // "Warning: spikes ahead."
	V_DATA_CUBE        = 39,  // "Data acquired."
	V_ACCELERATE       = 40,  // "Unexplained speed increase."
};

typedef int16_t Sint16;
typedef uint8_t Uint8;
typedef uint16_t Uint16;
typedef uint32_t Uint32;

Sint16 *soundSamples[SOUND_COUNT];
size_t soundSampleCount[SOUND_COUNT];

static inline void fread_u32_die(Uint32 *buffer, size_t count, FILE *stream)
{
	fread(buffer, sizeof(Uint32), count, stream);

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	for (size_t i = 0; i < count; ++i)
		buffer[i] = SDL_Swap32(buffer[i]);
#endif
}

static inline void fread_u16_die(Uint16 *buffer, size_t count, FILE *stream)
{
	fread(buffer, sizeof(Uint16), count, stream);

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	for (size_t i = 0; i < count; ++i)
		buffer[i] = SDL_Swap16(buffer[i]);
#endif
}

static inline void fread_u8_die(Uint8 *buffer, size_t count, FILE *stream)
{
    fread(buffer, sizeof(Uint8), count, stream);
}

void loadSndFile(bool xmas)
{
	FILE *f;
	f = fopen("tyrian.snd", "rb");

	Uint16 sfxCount;
	Uint32 sfxPositions[SFX_COUNT + 1];

	// Read number of sounds.
	fread_u16_die(&sfxCount, 1, f);
	/*if (sfxCount != SFX_COUNT)
		goto die;*/

	// Read positions of sounds.
	fread_u32_die(sfxPositions, sfxCount, f);

	// Determine end of last sound.
	fseek(f, 0, SEEK_END);
	sfxPositions[sfxCount] = (Uint32)ftell(f);

    if (xmas == false) {
        // Read samples.
        for (size_t i = 0; i < sfxCount; ++i)
        {
            soundSampleCount[i] = sfxPositions[i + 1] - sfxPositions[i];

            free(soundSamples[i]);
            soundSamples[i] = malloc(soundSampleCount[i]);
            fseek(f, sfxPositions[i], SEEK_SET);
            fread(soundSamples[i], soundSampleCount[i], 1, f);
            
            char audio[255];
            snprintf(audio, sizeof(audio), "Audio%zu.pcm", i);
            FILE *out = fopen(audio, "wb");
            
            fwrite(soundSamples[i], soundSampleCount[i], 1, out);
            fclose(out);

        }
    }

    fclose(f);

	f = fopen(xmas ? "voicesc.snd" : "voices.snd", "rb");

	Uint16 voiceCount;
	Uint32 voicePositions[VOICE_COUNT + 1];

	// Read number of sounds.
	fread_u16_die(&voiceCount, 1, f);

	// Read positions of sounds.
	fread_u32_die(voicePositions, voiceCount, f);

	// Determine end of last sound.
	fseek(f, 0, SEEK_END);
    voicePositions[voiceCount] = (Uint32)ftell(f);

	for (size_t vi = 0; vi < voiceCount; ++vi)
	{
		size_t i = SFX_COUNT + vi;

		soundSampleCount[i] = voicePositions[vi + 1] - voicePositions[vi];

		// Voice sounds have some bad data at the end.
		soundSampleCount[i] = soundSampleCount[i] >= 100
			? soundSampleCount[i] - 100
			: 0;

		free(soundSamples[i]);
		soundSamples[i] = malloc(soundSampleCount[i]);
		fseek(f, voicePositions[vi], SEEK_SET);
		fread(soundSamples[i], soundSampleCount[i], 1, f);

        char audio[255];

        if (xmas == true) {
			snprintf(audio, sizeof(audio), "VoiceC%zu.pcm", vi);
		} else {
            snprintf(audio, sizeof(audio), "Voice%zu.pcm", vi);
		}

        FILE *out = fopen(audio, "wb");

        fwrite(soundSamples[i], soundSampleCount[i], 1, out);
        fclose(out);
	}

	fclose(f);
}

int main(void)
{
	loadSndFile(false);
	loadSndFile(true);

	return 0;
}

