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
#include "loudness.h"

#include "file.h"
#include "lds_play.h"
#include "nortsong.h"
#include "opentyr.h"
#include "params.h"

#ifdef WITH_SDL3
#include <SDL3/SDL.h>
#else
#include <SDL2/SDL.h>
#endif

#ifdef WITH_MIDI
#ifdef WITH_SDL3
#include <SDL3_mixer/SDL_mixer.h>

#define Mix_GetError SDL_GetError
#else
#include <SDL2/SDL_mixer_ext.h>
#endif

#include <midiproc.h>
#endif
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define OUTPUT_QUALITY 4  // 44.1 kHz

int audioSampleRate = 0;

bool music_stopped = true;
unsigned int song_playing = 0;

bool audio_disabled = false, music_disabled = false, samples_disabled = false;
bool fading_out = false;
bool unwated_loop = false;

MusicDevice music_device = OPL;
char soundfont[4096] = {0};

#ifdef WITH_SDL3
SDL_Mutex *AudioDeviceLock = NULL;
#endif

const char *const music_device_names[MUSIC_DEVICE_MAX] = {
	"OPL3",
	"FluidSynth",
	"Native MIDI"
};

const uint8_t IS_MIDI_DEVICE = FLUIDSYNTH | NATIVE_MIDI;

#ifdef WITH_MIDI
#ifndef WITH_SDL3
static Mix_CommonMixer_t music_mixer = NULL;
#endif

typedef struct _MidiData {
	Uint8 *data;
	Uint32 size;
	Uint32 duration;
	Uint32 loop_start;
	Uint32 loop_end;
	Uint32 track_count;
	Uint32 subsong_count;
    Uint32 duration_ms;
} MidiData;
static MidiData * midi_data;
static Mix_Music ** midi_tracks = NULL;
#endif

static SDL_AudioDeviceID audioDevice = 0;

#ifdef WITH_SDL3
static SDL_AudioStream *auStream = NULL;
#endif

static Uint8 musicVolume = 255;
static Uint8 sampleVolume = 255;

static const float volumeRange = 30.0f;  // dB

// Fixed point Q20.12; needs to be able to store (10 * INT16_MIN/MAX)
static Sint32 volumeFactorTable[256];
#define TO_FIXED(x) ((Sint32)((x) * (1 << 12)))
#define FIXED_TO_INT(x) ((Sint32)((x) >> 12))

// Twice the Loudness update rate (in updates/second).  In Tyrian, Loudness
// updates were performed at the same rate as the game timer, which varied
// depending on the game speed (~69.57 Hz at most game speeds).  We don't have
// the same limitations, so we'll keep the update rate constant, but we do want
// to stick to integer math, so we'll update at 69.5 Hz.
static const int ldsUpdate2Rate = 139;  // 69.5 * 2

static int samplesPerLdsUpdate;
static int samplesPerLdsUpdateFrac;

static int samplesUntilLdsUpdate = 0;
static int samplesUntilLdsUpdateFrac = 0;

static FILE *music_file = NULL;

static Uint32 *song_offset;
static Uint16 song_count = 0;

#define NO_SONG_PLAYING 0xFFFFFFFF
static double time_playing = 0;
#define CHANNEL_COUNT 8
static const Sint16 *channelSamples[CHANNEL_COUNT];
static size_t channelSampleCount[CHANNEL_COUNT] = { 0 };
static Uint8 channelVolume[CHANNEL_COUNT];
#define CHANNEL_VOLUME_LEVELS 8

#ifdef WITH_SDL3
#ifdef WITH_MIDI
static void audioMixCallback(void *userdata, Uint8 *stream, int size);
#endif

static void audioCallback(void *userdata, SDL_AudioStream *SDLstream, int add_size, int size);
#else
static void audioCallback(void *userdata, Uint8 *stream, int size);
#endif

static void load_song(unsigned int song_num);

#ifdef WITH_MIDI
bool init_midi(SDL_AudioSpec * got){
	if (!Mix_Init(MIX_INIT_MID)){
		fprintf(stderr, "error: SDL2_mixer_ext failed to init: %s\n", Mix_GetError());
		return false;
#ifndef WITH_SDL3
    } else if (Mix_InitMixer(got, SDL_FALSE) != 0) {
        fprintf(stderr, "error: SDL2_mixer_ext failed to open audio device: %s\n", Mix_GetError());
        return false;
#endif
	} else if ((strlen(soundfont) != 0) && Mix_SetSoundFonts(soundfont) == 0) {
#ifndef WITH_SDL3
		Mix_FreeMixer();
#endif
		fprintf(stderr, "error: SDL2_mixer_ext failed to set soundfont: %s\n", Mix_GetError());
		return false;
	} else {
#ifdef WITH_SDL3
        if (Mix_OpenAudio(audioDevice, got) == false)
        {
            fprintf(stderr, "error: SDL2_mixer_ext: failed to open audio device: %s\n", Mix_GetError());
            return false;
        }
#else
		music_mixer = Mix_GetGeneralMixer();

		if (music_mixer == NULL){
			Mix_FreeMixer();

			fprintf(stderr, "error: SDL2_mixer_ext: failed to get music_mixer: %s\n", Mix_GetError());
			return false;
		}
#endif
#ifdef WITH_SDL3
        Mix_SetPostMix(audioMixCallback, NULL);
        Mix_AllocateChannels(2);
        Mix_ReserveChannels(2);
#endif
	}
	return true;
}


// only use this within a lock
void _stop_midi(void){
	if (playing)
	{
		Mix_HaltMusic();
	}
	playing = false;
	songlooped = false;
}

void deinit_midi(void){
	_stop_midi();
	if (midi_tracks != NULL) {
		for (unsigned int i = 0; i < song_count; ++i)
		{
			if (midi_tracks[i] != NULL)
			{
				Mix_FreeMusic(midi_tracks[i]);
				midi_tracks[i] = NULL;
			}
		}
	}

#ifndef WITH_SDL3
    Mix_FreeMixer();
    music_mixer = NULL;
#else
    Mix_CloseAudio();
#endif
}

void convert_midi_data(void){
	// initialize the midi_data array
	midi_data = malloc(song_count * sizeof(*midi_data));
	midi_tracks = malloc(song_count * sizeof(*midi_tracks));
	memset(midi_tracks, 0, song_count * sizeof(*midi_tracks));
	for (unsigned int i = 0; i < song_count; ++i)
	{
		memset(&midi_data[i], 0, sizeof(MidiData));
		Uint32 start = song_offset[i];
		Uint32 end = song_offset[i + 1];
		Uint32 size = end - start;
		Uint8 *buf = malloc(size);
		fread_die(buf, size, 1, music_file);
		HMIDIContainer midi_container = MIDPROC_Container_Create();
		if (!MIDPROC_Process(buf, size, "lds", midi_container))
		{
			fprintf(stderr, "warning: failed to process song %d\n", i + 1);
			MIDPROC_Container_Delete(midi_container);
			free(buf);
			continue;
		}
		size_t midi_data_size = 0;
		MIDPROC_Container_SerializeAsSMF(midi_container, &(midi_data[i].data), &midi_data_size);
		midi_data[i].size = (Uint32)midi_data_size;
		if (midi_data[i].size == 0)
		{
			fprintf(stderr, "warning: failed to process song %d\n", i + 1);
			continue;
		}

		midi_data[i].duration = MIDPROC_Container_GetDuration(midi_container, 0, false);
		midi_data[i].duration_ms = MIDPROC_Container_GetDuration(midi_container, 0, true);
		MIDPROC_Container_DetectLoops(midi_container, false, true, false, false);
		midi_data[i].loop_start = MIDPROC_Container_GetLoopBeginTimestamp(midi_container, 0, false);
		midi_data[i].loop_end = MIDPROC_Container_GetLoopEndTimestamp(midi_container, 0, false);
		midi_data[i].track_count = MIDPROC_Container_GetTrackCount(midi_container);
		midi_data[i].subsong_count = (Uint32)MIDPROC_Container_GetSubSongCount(midi_container);
		MIDPROC_Container_Delete(midi_container);

		free(buf);
	}
}


// only use this within a lock
bool _play_midi(Uint32 songnum){
	if (fading_out)
	{
		_stop_midi();
	}
	assert((midi_tracks[songnum] != NULL));
	Sint32 loops = -1; // loop forever
	// Not setting loops to 0 for no loop because it will either loop anyway
	// or continue playing for like 4+ seconds after the end; we stop it manually below
#ifdef WITH_SDL3
    if (Mix_PlayMusic(midi_tracks[songnum], loops) == false)
#else
	Mix_RewindMusicStream(midi_tracks[songnum]);
    if (Mix_PlayMusic(midi_tracks[songnum], loops) != 0)
#endif
    {
        fprintf(stderr, "error: failed to play music: %s\n", Mix_GetError());
        return false;
#ifdef WITH_SDL3
    } else {
        Mix_RewindMusic();
        Mix_ResumeMusic();
#endif
    }

#ifdef WITH_SDL3
    Mix_VolumeMusic(MIX_MAX_VOLUME); // Fix with Windows
#endif
	song_playing = songnum;
	playing = true;
	songlooped = false;
	return true;
}

const char * get_midi_params(void){
	if (music_device == FLUIDSYNTH){
#if (defined(__APPLE__) && defined(__MACH__)) || defined(ANDROID) || defined(__ANDROID__) || defined(WIN32) || defined(_WIN32) || defined(__linux__)
        return "s1;p512;";
#else
		return "s4;p512;";
#endif
    } else if (music_device == NATIVE_MIDI){
        return "s1;";
	} else{
		return "";
	}
}

bool load_midi(unsigned int song_num){
	// This is outside of the audio lock because it can take a while
	if (midi_tracks[song_num] == NULL){
#ifdef WITH_SDL3
        midi_tracks[song_num] = Mix_LoadMUS_IO(SDL_IOFromConstMem(midi_data[song_num].data, midi_data[song_num].size), false);
#else
        const char * params = get_midi_params();
		midi_tracks[song_num] = Mix_LoadMUSType_RW_ARG(SDL_RWFromConstMem(midi_data[song_num].data, midi_data[song_num].size), MUS_MID, 1, params);
#endif

		if (midi_tracks[song_num] == NULL)
		{
			fprintf(stderr, "error: failed to load music: %s\n", Mix_GetError());
			return false;
		}
	}
	return true;
}
#endif

bool init_audio(void)
{
#ifdef WITH_SDL3
    AudioDeviceLock = SDL_CreateMutex();
#endif

#ifndef WITH_MIDI
	// Force OPL if compiled without MIDI support
	music_device = OPL;
#else
	#ifdef NO_NATIVE_MIDI
	if (music_device == NATIVE_MIDI){
		music_device = FLUIDSYNTH;
	}
	#endif
#endif
	if (audio_disabled)
		return false;

	SDL_AudioSpec ask, got;

	ask.freq = 11025 * OUTPUT_QUALITY;
#ifdef WITH_SDL3
    ask.format = SDL_AUDIO_S16;
#else
	ask.format = AUDIO_S16SYS;
#endif

	ask.channels = 1;

#ifndef WITH_SDL3
    ask.samples = 256 * OUTPUT_QUALITY; // ~23 ms
	ask.callback = audioCallback;
#endif

#ifdef WITH_SDL3
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) == false)
#else
	if (SDL_InitSubSystem(SDL_INIT_AUDIO))
#endif
	{
		fprintf(stderr, "error: failed to initialize SDL audio: %s\n", SDL_GetError());
		audio_disabled = true;
		return false;
	}

#ifndef WITH_SDL3
	int allowedChanges = SDL_AUDIO_ALLOW_FREQUENCY_CHANGE;
#if SDL_VERSION_ATLEAST(2, 0, 9)
	allowedChanges |= SDL_AUDIO_ALLOW_SAMPLES_CHANGE;
#endif
#endif

#ifdef WITH_SDL3
    auStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &ask, audioCallback, NULL);
    audioDevice = SDL_GetAudioStreamDevice(auStream);
    SDL_GetAudioStreamFormat(auStream, &ask, &got);

    if ((auStream == NULL) || (audioDevice == 0))
    {
#else
	audioDevice = SDL_OpenAudioDevice(/*device*/ NULL, /*iscapture*/ 0, &ask, &got, allowedChanges);

    if (audioDevice == 0)
    {
#endif

		fprintf(stderr, "error: SDL failed to open audio device: %s\n", SDL_GetError());
		audio_disabled = true;
		return false;
	}
#ifdef WITH_MIDI
	if (music_device & IS_MIDI_DEVICE){
		if (!init_midi(&got)) {
			fprintf(stderr, "error: failed to initialize midi, falling back to OPL...\n");
			music_device = OPL;
		}
	}
#endif

	audioSampleRate = got.freq;

	samplesPerLdsUpdate = 2 * (audioSampleRate / ldsUpdate2Rate);
	samplesPerLdsUpdateFrac = 2 * (audioSampleRate % ldsUpdate2Rate);

	volumeFactorTable[0] = 0;
	for (size_t i = 1; i < 256; ++i)
		volumeFactorTable[i] = TO_FIXED(powf(10, (255 - i) * (-volumeRange / (20.0f * 255))));

	opl_init();

#ifdef WITH_SDL3
    SDL_ResumeAudioStreamDevice(auStream);

    SDL_ResumeAudioDevice(audioDevice); // unpause
#else
    SDL_PauseAudioDevice(audioDevice, 0); // unpause
#endif

	return true;
}

bool restart_audio(void){
	if (audio_disabled)
		return false;
#ifdef WITH_SDL3
    SDL_LockMutex(AudioDeviceLock);
#else
	SDL_LockAudioDevice(audioDevice);
#endif

	unsigned int prev_song = song_playing;

#ifdef WITH_SDL3
    SDL_UnlockMutex(AudioDeviceLock);
#else
	SDL_UnlockAudioDevice(audioDevice);
#endif

	deinit_audio();
	if (!init_audio()){
		return false;
	}
	if (prev_song != NO_SONG_PLAYING){
		play_song(prev_song);
	}
	return true;
}

#ifdef WITH_SDL3
#ifdef WITH_MIDI
static void audioMixCallback(void *userdata, Uint8 *stream, int size)
{
    (void)userdata;
    Sint16 *const samples = (Sint16 *)stream;
    const int samplesCount = size / sizeof (Sint16);

    if ((music_device & IS_MIDI_DEVICE) && !music_disabled && !music_stopped){
        if (Mix_PlayingMusic() == 0){
            fading_out = false;
            time_playing = 0;
            playing = false;
            songlooped = false;
        } else {
            if (playing){
                // get samples from the mixer
                double factor = 1000.0;
                double cur_position = 0.0;

                cur_position = midi_tracks[song_playing] ? Mix_GetMusicPosition(midi_tracks[song_playing])  : 0;
                cur_position *= factor;

                // check the duration of the song and see if it looped
                bool has_loop = (bool)(midi_data[song_playing].loop_end <= midi_data[song_playing].duration);
                #ifdef _DEBUG
                fprintf(stderr, "cur_position: %f, time_playing: %f, duration: %d, loop_end: %d\n", cur_position, time_playing, midi_data[song_playing].duration, midi_data[song_playing].loop_end);
                #endif
                if (unwated_loop && !has_loop) {
                    // this is to get around a bug in fluidsynth where it plays songs twice even if no loops are set
                    _stop_midi();
                    for (int i = 0; i < samplesCount; ++i)
                        samples[i] = 0;
                    time_playing = 0;
                    unwated_loop = false;
                } else if (!has_loop &&
                    ((cur_position < time_playing) ||
                    (cur_position >= midi_data[song_playing].duration + 100))) {
                    unwated_loop = true; // stop it the next time
                } else { // has loop and did loop
                    // The reason for this is that fluidsynth doesn't recognize any form of SMF loops,
                    // and consequentially does not loop where the original songs looped;
                    // e.g. they start at the very beginning rather than a few positions up like most of the songs.
                    // So, we have to do it manually.
                    // We have to call music_mixer above first to get SDL_mixer to drive the synth and update the position,
                    // then clear the samples and call it again.
                    if (has_loop &&
                    ((cur_position < time_playing) || (cur_position >= midi_data[song_playing].loop_end))) {
                        double loop_start = ((double)midi_data[song_playing].loop_start) / factor;
                        Mix_SetMusicPosition(loop_start);
                        for (int i = 0; i < samplesCount; ++i)
                        {
                            samples[i] = 0;
                        }

                        songlooped = true;
                    }
                }
                time_playing = cur_position;
            }
            if (!playing) {
            }
        }
    }
}
#endif

static void audioCallback(void *userdata, SDL_AudioStream *SDLstream, int add_size, int size)
#else
static void audioCallback(void *userdata, Uint8 *stream, int size)
#endif
{
#ifndef WITH_SDL3
    (void)size;
#endif
    (void)userdata;

#ifdef WITH_SDL3
    Sint16 *const samples = SDL_malloc(add_size);

    if (samples == NULL)
    {
        return;
    }
#else
	Sint16 *const samples = (Sint16 *)stream;
#endif

	const int samplesCount = size / sizeof (Sint16);

#ifndef WITH_SDL3
#ifdef WITH_MIDI
	if ((music_device & IS_MIDI_DEVICE) && !music_disabled && !music_stopped){
		if (Mix_PlayingMusic() == 0){
			fading_out = false;
			time_playing = 0;
			playing = false;
			songlooped = false;
		} else {
			if (playing){
				// get samples from the mixer
				double factor = 1000.0;
                double cur_position = 0.0;

				music_mixer(NULL, stream, size);

                cur_position = midi_tracks[song_playing] ? Mix_GetMusicPosition(midi_tracks[song_playing])  : 0;
				cur_position *= factor;

				// check the duration of the song and see if it looped
                bool has_loop = (bool)(midi_data[song_playing].loop_end <= midi_data[song_playing].duration);
				#ifdef _DEBUG
				fprintf(stderr, "cur_position: %f, time_playing: %f, duration: %d, loop_end: %d\n", cur_position, time_playing, midi_data[song_playing].duration, midi_data[song_playing].loop_end);
                #endif
				if (unwated_loop && !has_loop) {
					// this is to get around a bug in fluidsynth where it plays songs twice even if no loops are set
					_stop_midi();
					for (int i = 0; i < samplesCount; ++i)
						samples[i] = 0;
					time_playing = 0;
					unwated_loop = false;
				} else if (!has_loop &&
					((cur_position < time_playing) ||
					(cur_position >= midi_data[song_playing].duration + 100))) {
					unwated_loop = true; // stop it the next time
				} else { // has loop and did loop
					// The reason for this is that fluidsynth doesn't recognize any form of SMF loops,
					// and consequentially does not loop where the original songs looped;
					// e.g. they start at the very beginning rather than a few positions up like most of the songs.
					// So, we have to do it manually.
					// We have to call music_mixer above first to get SDL_mixer to drive the synth and update the position,
					// then clear the samples and call it again.
					if (has_loop &&
					((cur_position < time_playing) || (cur_position >= midi_data[song_playing].loop_end))) {
						double loop_start = ((double)midi_data[song_playing].loop_start) / factor;
						Mix_SetMusicPosition(loop_start);
						for (int i = 0; i < samplesCount; ++i)
                        {
                            samples[i] = 0;
                        }

						music_mixer(NULL, stream, size);

						songlooped = true;
					}
				}
                time_playing = cur_position;
			}
			if (!playing) {
			}
		}
	}
	else
#endif
#endif
	if ((music_device == OPL) && !music_disabled && !music_stopped)
	{
		Sint16 *remaining = samples;
		int remainingCount = samplesCount;
		while (remainingCount > 0)
		{
			if (samplesUntilLdsUpdate == 0)
			{
				lds_update();

				// The number of samples that should be produced per Loudness
				// update is not an integer, but we can only produce an integer
				// number of samples, so we accumulate the fractional samples
				// until it amounts to a whole sample.
				samplesUntilLdsUpdate += samplesPerLdsUpdate;
				samplesUntilLdsUpdateFrac += samplesPerLdsUpdateFrac;
				if (samplesUntilLdsUpdateFrac >= ldsUpdate2Rate)
				{
					samplesUntilLdsUpdate += 1;
					samplesUntilLdsUpdateFrac -= ldsUpdate2Rate;
				}
			}

			int count = MIN(samplesUntilLdsUpdate, remainingCount);

			opl_update(remaining, count);

			remaining += count;
			remainingCount -= count;

			samplesUntilLdsUpdate -= count;
		}
	}
	else
	{
		for (int i = 0; i < samplesCount; ++i)
			samples[i] = 0;
	}

	Sint32 musicVolumeFactor = volumeFactorTable[musicVolume];
	musicVolumeFactor *= 2;  // OPL emulator is too quiet

	if (samples_disabled && !music_disabled)
	{
		// Mix music
		Sint16 *remaining = samples;
		int remainingCount = samplesCount;
		while (remainingCount > 0)
		{
			Sint32 sample = *remaining * musicVolumeFactor;

			sample = FIXED_TO_INT(sample);
			*remaining = MIN(MAX(INT16_MIN, sample), INT16_MAX);

			remaining += 1;
			remainingCount -= 1;
		}
	}
	else if (!samples_disabled)
	{
		Sint32 sampleVolumeFactor = volumeFactorTable[sampleVolume];
		Sint32 sampleVolumeFactors[CHANNEL_VOLUME_LEVELS];
		for (int i = 0; i < CHANNEL_VOLUME_LEVELS; ++i)
        {
            sampleVolumeFactors[i] = sampleVolumeFactor * (i + 1) / CHANNEL_VOLUME_LEVELS;
        }

		// Mix music and channels
		Sint16 *remaining = samples;
		int remainingCount = samplesCount;
		while (remainingCount > 0)
        {
            Sint32 sample = *remaining * musicVolumeFactor;

            for (size_t i = 0; i < CHANNEL_COUNT; ++i)
            {
                if (channelSampleCount[i] > 0)
                {
                    sample += *channelSamples[i] * sampleVolumeFactors[channelVolume[i]];

                    channelSamples[i] += 1;
                    channelSampleCount[i] -= 1;
                }
            }

            sample = FIXED_TO_INT(sample);
            *remaining = MIN(MAX(INT16_MIN, sample), INT16_MAX);

            remaining += 1;
            remainingCount -= 1;
        }
    }

#ifdef WITH_SDL3
    SDL_PutAudioStreamData(SDLstream, samples, add_size);

    if (samples != NULL)
    {
        SDL_free(samples);
    }
#endif
}

void deinit_audio(void)
{
	if (audio_disabled)
		return;

	if (audioDevice != 0)
	{
#ifdef WITH_SDL3
		SDL_PauseAudioDevice(audioDevice); // pause
#else
        SDL_PauseAudioDevice(audioDevice, 1); // pause
#endif

#ifdef WITH_MIDI
		deinit_midi();
#endif
		SDL_CloseAudioDevice(audioDevice);
		audioDevice = 0;
	}

	SDL_QuitSubSystem(SDL_INIT_AUDIO);
	song_playing = NO_SONG_PLAYING;
	playing = false;
	songlooped = false;
	music_stopped = true;

	memset(channelSampleCount, 0, sizeof(channelSampleCount));

	lds_free();

#ifdef WITH_SDL3
    SDL_free(AudioDeviceLock);
#endif
}


void load_music(void)  // FKA NortSong.loadSong
{
	if (music_file == NULL)
	{
		music_file = dir_fopen_die(data_dir(), "music.mus", "rb");

		fread_u16_die(&song_count, 1, music_file);

		song_offset = malloc((song_count + 1) * sizeof(*song_offset));

		fread_u32_die(song_offset, song_count, music_file);

		song_offset[song_count] = (Uint32)ftell_eof(music_file);
#ifdef WITH_MIDI
		convert_midi_data();
#endif
	}
}

static void load_song(unsigned int song_num)  // FKA NortSong.loadSong
{
	if (song_num < song_count)
	{
		unsigned int song_size = song_offset[song_num + 1] - song_offset[song_num];
		lds_load(music_file, song_offset[song_num], song_size);
	}
	else
	{
		fprintf(stderr, "warning: failed to load song %d\n", song_num + 1);
	}
}



void play_song(unsigned int song_num)  // FKA NortSong.playSong
{
	if (song_num >= song_count)
	{
		fprintf(stderr, "warning: song %d does not exist\n", song_num + 1);
		return;
	}
	if (audio_disabled)
		return;

#ifdef WITH_MIDI
	if ((song_num != song_playing) || !Mix_PlayingMusic())
#else
	if (song_num != song_playing)
#endif
	{
#ifdef WITH_MIDI
		// This is outside of the audio lock because it can take a while and it doesn't require it
		if (music_device & IS_MIDI_DEVICE && !load_midi(song_num)){
			return;
		}
#endif
#ifdef WITH_SDL3
        SDL_LockMutex(AudioDeviceLock);
#else
		SDL_LockAudioDevice(audioDevice);
#endif

		music_stopped = true;

#ifdef WITH_MIDI
		if (music_device & IS_MIDI_DEVICE){
			_stop_midi();
			_play_midi(song_num);
		}
#endif
		fading_out = false;
		time_playing = 0;
		song_playing = song_num;

#ifdef WITH_SDL3
        SDL_UnlockMutex(AudioDeviceLock);
#else
		SDL_UnlockAudioDevice(audioDevice);
#endif

		if (music_device == OPL)
		{
			load_song(song_num);
		}

	}

#ifdef WITH_SDL3
    SDL_LockMutex(AudioDeviceLock);
#else
	SDL_LockAudioDevice(audioDevice);
#endif

	music_stopped = false;

#ifdef WITH_SDL3
    SDL_UnlockMutex(AudioDeviceLock);
#else
	SDL_UnlockAudioDevice(audioDevice);
#endif
}

void restart_song(void)  // FKA Player.selectSong(1)
{
	if (audio_disabled)
		return;

#ifdef WITH_SDL3
    SDL_LockMutex(AudioDeviceLock);
#else
	SDL_LockAudioDevice(audioDevice);
#endif

	#ifdef WITH_MIDI
	if (music_device & IS_MIDI_DEVICE){
		// Rewind isn't implemented for fluidsynth or native midi, so we have to stop and start it again
		_stop_midi();
		_play_midi(song_playing);
	}
	else
	#endif
	{
		lds_rewind();
	}

	fading_out = false;
	time_playing = 0;
	music_stopped = false;

#ifdef WITH_SDL3
    SDL_UnlockMutex(AudioDeviceLock);
#else
	SDL_UnlockAudioDevice(audioDevice);
#endif
}

void stop_song(void)  // FKA Player.selectSong(0)
{
	if (audio_disabled)
		return;

#ifdef WITH_SDL3
    SDL_LockMutex(AudioDeviceLock);
#else
	SDL_LockAudioDevice(audioDevice);
#endif

#ifdef WITH_MIDI
	if (music_device & IS_MIDI_DEVICE){
		_stop_midi();
	}
#endif
	fading_out = false;
	time_playing = 0;

	music_stopped = true;

#ifdef WITH_SDL3
    SDL_UnlockMutex(AudioDeviceLock);
#else
	SDL_UnlockAudioDevice(audioDevice);
#endif
}

void fade_song(void)  // FKA Player.selectSong($C001)
{
	if (audio_disabled)
		return;

#ifdef WITH_SDL3
    SDL_LockMutex(AudioDeviceLock);
#else
	SDL_LockAudioDevice(audioDevice);
#endif

	fading_out = true;
#ifdef WITH_MIDI
	if (music_device & IS_MIDI_DEVICE){
		if (playing)
		{
			Mix_FadeOutMusic(6000);
		}
	}
	else
#endif
	{
		lds_fade(1);
	}

#ifdef WITH_SDL3
    SDL_UnlockMutex(AudioDeviceLock);
#else
	SDL_UnlockAudioDevice(audioDevice);
#endif
}

void set_volume(Uint8 musicVolume_, Uint8 sampleVolume_)  // FKA NortSong.setVol and Player.setVol
{
	if (audio_disabled)
		return;

#ifdef WITH_SDL3
    SDL_LockMutex(AudioDeviceLock);
#else
    SDL_LockAudioDevice(audioDevice);
#endif

	musicVolume = musicVolume_;
	sampleVolume = sampleVolume_;

#ifdef WITH_SDL3
    SDL_UnlockMutex(AudioDeviceLock);
#else
	SDL_UnlockAudioDevice(audioDevice);
#endif
}

void multiSamplePlay(const Sint16 *samples, size_t sampleCount, Uint8 chan, Uint8 vol)  // FKA Player.multiSamplePlay
{
	assert(chan < CHANNEL_COUNT);
	assert(vol < CHANNEL_VOLUME_LEVELS);

	if (audio_disabled || samples_disabled)
		return;

#ifdef WITH_SDL3
    SDL_LockMutex(AudioDeviceLock);
#else
	SDL_LockAudioDevice(audioDevice);
#endif

	channelSamples[chan] = samples;
	channelSampleCount[chan] = sampleCount;
	channelVolume[chan] = vol;

#ifdef WITH_SDL3
    SDL_UnlockMutex(AudioDeviceLock);
#else
	SDL_UnlockAudioDevice(audioDevice);
#endif
}
