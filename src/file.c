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
#include "file.h"

#include "opentyr.h"
#include "varz.h"

#ifdef WITH_SDL3
#include <SDL3/SDL.h>
#else
#ifdef WITH_SDL
#include <SDL.h>
#else
#include <SDL2/SDL.h>
#endif
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__APPLE__) & defined(__MACH__)
#include "macos-bundle.h"

#define fseek fseeko
#define ftell ftello
#elif (defined(_WIN32) || defined(WIN32)) && !defined(_MSC_VER)
#define fseek fseeko64
#define ftell ftello64
#define fopen fopen64
#endif

#ifdef PSP
#include <pspiofilemgr.h>
#endif

#ifdef VITA
#include <psp2/io/fcntl.h>
#endif

#if defined(ANDROID) || defined(__ANDROID__)
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#endif

#ifdef WITH_SDL3_ESP
#include "bsp/esp-bsp.h"
#endif

const char *custom_data_dir = NULL;

#ifndef TYRIAN_DIR
#define TYRIAN_DIR "."
#endif

// finds the Tyrian data directory
const char * data_dir(void)
{
    static const char *dir = NULL;

#if defined(VITA)
    const char *const dirs[] =
    {
        custom_data_dir,
        "app0:data/",
        ".",
    };
#elif (defined(WITH_SDL) && !defined(WITH_SDL1)) || defined(WITH_SDL3_ESP)
    const char *const dirs[] =
    {
        "/sd/data",
        "/sd",
        "data",
        ".",
    };
#elif defined(__DREAMCAST__)
    const char *const dirs[] =
    {
        custom_data_dir,
        "/rd/data/",
        "data",
        ".",
    };
#elif defined(__NDS__)
    const char *const dirs[] =
    {
        custom_data_dir,
        "nitro:/",
        ".",
    };
#elif defined(__3DS__)
    const char *const dirs[] =
    {
        custom_data_dir,
        "data",
        ".",
    };
#elif defined(__OGC__)
    const char *const dirs[] =
    {
        custom_data_dir,
#ifdef HW_RVL
        "sd:/data",
        "usb:/data",
#endif
        "carda:/data",
        "dvd:",
        ".",
    };
#elif defined(PSP)
    const char *const dirs[] =
    {
        custom_data_dir,
        "ms0:/PSP/GAME/opentyrian2000/data",
        "umd0:/data",
        "/data",
        ".",
    };
#elif defined(__APPLE__) & defined(__MACH__)
    const char *const dirs[] =
    {
        custom_data_dir,
        getBundlePath(),
        "data",
        ".",
    };
#elif defined(ANDROID) || defined(__ANDROID__)
    const char *const dirs[] =
    {
        custom_data_dir,
        "/sdcard/Android/tyriandata",
        ".",
    };
#elif defined(__linux__)
    const char *const dirs[] =
    {
        custom_data_dir,
        "/usr/share/tyriandata",
        ".",
    };
#else
	const char *const dirs[] =
	{
		custom_data_dir,
		"data",
		".",
	};
#endif

	if (dir != NULL)
		return dir;

	for (uint i = 0; i < COUNTOF(dirs); ++i)
	{
		if (dirs[i] == NULL)
			continue;

		FILE *f = dir_fopen(dirs[i], "tyrian1.lvl", "rb");
		if (f)
		{
			efclose(f);
			dir = dirs[i];
			break;
		}
	}

	if (dir == NULL) // data not found
		dir = "";

	return dir;
}

#if (defined(WITH_SDL) && !defined(WITH_SDL1)) || defined(WITH_SDL3_ESP)
bool init_SD = false;
#endif

// prepend directory and fopen
FILE * dir_fopen(const char *dir, const char *file, const char *mode)
{
	char *path = malloc(strlen(dir) + 1 + strlen(file) + 1);
	snprintf(path, (strlen(dir) + 1 + strlen(file) + 1), "%s/%s", dir, file);

#if defined(WITH_SDL) && !defined(WITH_SDL1)
    if(init_SD == false)
    {
        SDL_InitSD();
        init_SD = true;
    }

    SDL_LockDisplay();
#elif defined(WITH_SDL3_ESP)
    if(init_SD == false)
    {
#if CONFIG_MOUNT_SD_CARD
        if (bsp_sdcard_mount() == ESP_OK)
#else /* CONFIG_MOUNT_LITTLEFS */
        if (bsp_littlefs_mount() == ESP_OK)
#endif
        {
            init_SD = true;
        }
    }
#endif
#if defined(_MSC_VER) && __STDC_WANT_SECURE_LIB__
	FILE *f = NULL;
	fopen_s(&f, path, mode);
#else
	FILE *f = fopen(path, mode);
#endif
#if defined(WITH_SDL) && !defined(WITH_SDL1)
    SDL_UnlockDisplay();
#endif

	free(path);

	return f;
}

// warn when dir_fopen fails
FILE * dir_fopen_warn(const char *dir, const char *file, const char *mode)
{
	FILE *f = dir_fopen(dir, file, mode);
#if defined(_MSC_VER) && __STDC_WANT_SECURE_LIB__
	char err[256];
#endif

	if (f == NULL)
	{
#if defined(_MSC_VER) && __STDC_WANT_SECURE_LIB__
		strerror_s(err, sizeof(err), errno);
		_fprintf(stderr, "warning: failed to open '%s': %s\n", file, err);
#else
		_fprintf(stderr, "warning: failed to open '%s': %s\n", file, strerror(errno));
#endif
	}

	return f;
}

// die when dir_fopen fails
FILE * dir_fopen_die(const char *dir, const char *file, const char *mode)
{
	FILE *f = dir_fopen(dir, file, mode);
#if defined(_MSC_VER) && __STDC_WANT_SECURE_LIB__
	char err[256];
#endif

	if (f == NULL)
	{
#if defined(_MSC_VER) && __STDC_WANT_SECURE_LIB__
		strerror_s(err, sizeof(err), errno);
		_fprintf(stderr, "error: failed to open '%s': %s\n", file, err);
#else
		_fprintf(stderr, "error: failed to open '%s': %s\n", file, strerror(errno));
#endif
		_fprintf(stderr, "error: One or more of the required Tyrian " TYRIAN_VERSION " data files could not be found.\n"
		                "       Please read the README file.\n");
		JE_tyrianHalt(1);
	}

	return f;
}

// check if file can be opened for reading
bool dir_file_exists(const char *dir, const char *file)
{
	FILE *f = dir_fopen(dir, file, "rb");
	if (f != NULL)
		efclose(f);
	return (f != NULL);
}

// returns end-of-file position
long ftell_eof(FILE *f)
{
#if defined(WITH_SDL) && !defined(WITH_SDL1)
    SDL_LockDisplay();
#endif
	long pos = ftell(f);

	fseek(f, 0, SEEK_END);
	long size = ftell(f);

	fseek(f, pos, SEEK_SET);
#if defined(WITH_SDL) && !defined(WITH_SDL1)
    SDL_UnlockDisplay();
#endif
	return size;
}

long eftell(FILE *f)
{
#if defined(WITH_SDL) && !defined(WITH_SDL1)
    SDL_LockDisplay();
#endif
    long size = ftell(f);
#if defined(WITH_SDL) && !defined(WITH_SDL1)
    SDL_UnlockDisplay();
#endif

    return size;
}

int efseek(FILE *f, long pos, int flag)
{
#if defined(WITH_SDL) && !defined(WITH_SDL1)
    SDL_LockDisplay();
#endif
    int retval = fseek(f, pos, flag);
#if defined(WITH_SDL) && !defined(WITH_SDL1)
    SDL_UnlockDisplay();
#endif
    return retval;
}

int efclose(FILE *f)
{
#if defined(WITH_SDL) && !defined(WITH_SDL1)
    SDL_LockDisplay();
#endif
    int retval = fclose(f);
#if defined(WITH_SDL) && !defined(WITH_SDL1)
    SDL_UnlockDisplay();
#endif
    return retval;
}
#ifndef HANDLE_RESULT
#define HANDLE_RESULT 1
#endif

void fread_die(void *buffer, size_t size, size_t count, FILE *stream)
{
#if defined(WITH_SDL) && !defined(WITH_SDL1)
    SDL_LockDisplay();
#endif

	size_t result = fread(buffer, size, count, stream);

#ifdef HANDLE_RESULT
	if (result != count)
	{
		_fprintf(stderr, "error: An unexpected problem occurred while reading from a file.\n");
		SDL_Quit();
		exit(EXIT_FAILURE);
	}
#else
    _fprintf(stderr, "fread_die - size=%llu.\n", (unsigned long long)result);
#endif

#if defined(WITH_SDL) && !defined(WITH_SDL1)
    SDL_UnlockDisplay();
#endif
}

void fwrite_die(const void *buffer, size_t size, size_t count, FILE *stream)
{
#if defined(WITH_SDL) && !defined(WITH_SDL1)
    SDL_LockDisplay();
#endif

	size_t result = fwrite(buffer, size, count, stream);

#ifdef HANDLE_RESULT
	if (result != count)
	{
		_fprintf(stderr, "error: An unexpected problem occurred while writing to a file.\n");
		SDL_Quit();
		exit(EXIT_FAILURE);
	}
#else
    _fprintf(stderr, "fwrite_die - size=%llu.\n", (unsigned long long)result);
#endif

#if defined(WITH_SDL) && !defined(WITH_SDL1)
    SDL_UnlockDisplay();
#endif
}
