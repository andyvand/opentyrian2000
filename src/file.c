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
#include <SDL2/SDL.h>
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

#if defined(ANDROID) || defined(__ANDROID__)
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#endif

const char *custom_data_dir = NULL;

#ifndef TYRIAN_DIR
#define TYRIAN_DIR "."
#endif

// finds the Tyrian data directory
const char *data_dir(void)
{
#if defined(__APPLE__) & defined(__MACH__)
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
        TYRIAN_DIR,
        ".",
    };
#elif defined(__linux__)
    const char *const dirs[] =
    {
        custom_data_dir,
        TYRIAN_DIR,
        "/usr/share/tyriandata",
        ".",
    };
#else
	const char *const dirs[] =
	{
		custom_data_dir,
		TYRIAN_DIR,
		"data",
		".",
	};
#endif

	static const char *dir = NULL;

	if (dir != NULL)
		return dir;

	for (uint i = 0; i < COUNTOF(dirs); ++i)
	{
		if (dirs[i] == NULL)
			continue;

		FILE *f = dir_fopen(dirs[i], "tyrian1.lvl", "rb");
		if (f)
		{
			fclose(f);

			dir = dirs[i];
			break;
		}
	}

	if (dir == NULL) // data not found
		dir = "";

	return dir;
}

// prepend directory and fopen
FILE *dir_fopen(const char *dir, const char *file, const char *mode)
{
	char *path = malloc(strlen(dir) + 1 + strlen(file) + 1);
	snprintf(path, (strlen(dir) + 1 + strlen(file) + 1), "%s/%s", dir, file);

#if defined(_MSC_VER) && __STDC_WANT_SECURE_LIB__
	FILE *f = NULL;
	fopen_s(&f, path, mode);
#else
	FILE *f = fopen(path, mode);
#endif

	free(path);

	return f;
}

// warn when dir_fopen fails
FILE *dir_fopen_warn(const char *dir, const char *file, const char *mode)
{
	FILE *f = dir_fopen(dir, file, mode);
#if defined(_MSC_VER) && __STDC_WANT_SECURE_LIB__
	char err[256];
#endif

	if (f == NULL)
	{
#if defined(_MSC_VER) && __STDC_WANT_SECURE_LIB__
		strerror_s(err, sizeof(err), errno);
		fprintf(stderr, "warning: faile to open '%s': %s\n", file, err);
#else
		fprintf(stderr, "warning: failed to open '%s': %s\n", file, strerror(errno));
#endif
	}

	return f;
}

// die when dir_fopen fails
FILE *dir_fopen_die(const char *dir, const char *file, const char *mode)
{
	FILE *f = dir_fopen(dir, file, mode);
#if defined(_MSC_VER) && __STDC_WANT_SECURE_LIB__
	char err[256];
#endif

	if (f == NULL)
	{
#if defined(_MSC_VER) && __STDC_WANT_SECURE_LIB__
		strerror_s(err, sizeof(err), errno);
		fprintf(stderr, "error: failed to open '%s': %s\n", file, err);
#else
		fprintf(stderr, "error: failed to open '%s': %s\n", file, strerror(errno));
#endif
		fprintf(stderr, "error: One or more of the required Tyrian " TYRIAN_VERSION " data files could not be found.\n"
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
		fclose(f);
	return (f != NULL);
}

// returns end-of-file position
long ftell_eof(FILE *f)
{
	long pos = ftell(f);

	fseek(f, 0, SEEK_END);
	long size = ftell(f);

	fseek(f, pos, SEEK_SET);

	return size;
}

#ifndef HANDLE_RESULT
#define HANDLE_RESULT 1
#endif

void fread_die(void *buffer, size_t size, size_t count, FILE *stream)
{
	size_t result = fread(buffer, size, count, stream);

#ifdef HANDLE_RESULT
	if (result != count)
	{
		fprintf(stderr, "error: An unexpected problem occurred while reading from a file.\n");
		SDL_Quit();
		exit(EXIT_FAILURE);
	}
#else
    fprintf(stderr, "fread_die - size=%llu.\n", (unsigned long long)result);
#endif
}

void fwrite_die(const void *buffer, size_t size, size_t count, FILE *stream)
{
	size_t result = fwrite(buffer, size, count, stream);

#ifdef HANDLE_RESULT
	if (result != count)
	{
		fprintf(stderr, "error: An unexpected problem occurred while writing to a file.\n");
		SDL_Quit();
		exit(EXIT_FAILURE);
	}
#else
    fprintf(stderr, "fwrite_die - size=%llu.\n", (unsigned long long)result);
#endif
}
