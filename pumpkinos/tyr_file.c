/*
 * OpenTyrian 2000 PumpkinOS port: file access.
 *
 * Replaces src/file.c. The stdio calls below are redirected to the
 * tyr_* functions by pumpkin_compat.h; those read the bundled data
 * resources ("res:/<name>") or PalmOS VFS files (everything else).
 */
#include "file.h"

#include "opentyr.h"
#include "varz.h"

#include "pumpkin_glue.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *custom_data_dir = NULL;

/* The data files are PRC resources; "res:" is the pseudo directory that
   tyr_fopen() resolves to them (see pumpkin_stdio.h). */
const char *data_dir(void)
{
	return "res:";
}

/* prepend directory and fopen */
FILE *dir_fopen(const char *dir, const char *file, const char *mode)
{
	size_t len = strlen(dir) + 1 + strlen(file) + 1;
	char *path = malloc(len);
	FILE *f;

	if (path == NULL)
		return NULL;

	snprintf(path, len, "%s/%s", dir, file);
	f = fopen(path, mode);
	free(path);

	return f;
}

/* warn when dir_fopen fails */
FILE *dir_fopen_warn(const char *dir, const char *file, const char *mode)
{
	FILE *f = dir_fopen(dir, file, mode);

	if (f == NULL)
		_fprintf(stderr, "warning: failed to open '%s' in '%s'\n", file, dir);

	return f;
}

/* die when dir_fopen fails */
FILE *dir_fopen_die(const char *dir, const char *file, const char *mode)
{
	FILE *f = dir_fopen(dir, file, mode);

	if (f == NULL)
	{
		char msg[256];

		snprintf(msg, sizeof(msg), "The Tyrian " TYRIAN_VERSION " data file '%s' is missing from the application.", file);
		_fprintf(stderr, "error: failed to open '%s' in '%s'\n", file, dir);
		_fprintf(stderr, "error: One or more of the required Tyrian " TYRIAN_VERSION " data files could not be found.\n");
		pk_error(msg);
		JE_tyrianHalt(1);
	}

	return f;
}

/* check if file can be opened for reading */
bool dir_file_exists(const char *dir, const char *file)
{
	FILE *f = dir_fopen(dir, file, "rb");
	if (f != NULL)
		efclose(f);
	return (f != NULL);
}

/* returns end-of-file position */
long ftell_eof(FILE *f)
{
	long pos = ftell(f);

	fseek(f, 0, SEEK_END);
	long size = ftell(f);

	fseek(f, pos, SEEK_SET);

	return size;
}

long eftell(FILE *f)
{
	return ftell(f);
}

int efseek(FILE *f, long pos, int flag)
{
	return fseek(f, pos, flag);
}

int efclose(FILE *f)
{
	return fclose(f);
}

void fread_die(void *buffer, size_t size, size_t count, FILE *stream)
{
	size_t result = fread(buffer, size, count, stream);

	if (result != count)
	{
		_fprintf(stderr, "error: An unexpected problem occurred while reading from a file.\n");
		pk_error("An unexpected problem occurred while reading a data file.");
		SDL_Quit();
		exit(EXIT_FAILURE);
	}
}

void fwrite_die(const void *buffer, size_t size, size_t count, FILE *stream)
{
	size_t result = fwrite(buffer, size, count, stream);

	if (result != count)
	{
		_fprintf(stderr, "error: An unexpected problem occurred while writing to a file.\n");
		pk_error("An unexpected problem occurred while writing a file.");
		SDL_Quit();
		exit(EXIT_FAILURE);
	}
}
