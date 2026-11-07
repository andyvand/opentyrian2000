/*
 * OpenTyrian 2000 PumpkinOS port: compatibility header.
 *
 * This header is force-included (-include) into every OpenTyrian source
 * file when building for PumpkinOS. It redirects the few host C library
 * facilities the game must not use directly:
 *
 *  - exit(): a PumpkinOS application is a shared library running inside
 *    the PumpkinOS process; calling exit() would terminate the whole
 *    system. tyr_exit() unwinds back to PilotMain instead.
 *  - stdio FILE functions: data files are PRC resources and user files
 *    live in the PalmOS VFS (see pumpkin_stdio.h).
 *  - mkdir(): the user directory is created by the PalmOS side.
 *
 * The system headers that declare these names are included first so
 * that the macros only affect the game's call sites.
 */
#ifndef PUMPKIN_COMPAT_H
#define PUMPKIN_COMPAT_H

#ifndef PUMPKINOS
#define PUMPKINOS 1
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <unistd.h>
#endif

#include "pumpkin_stdio.h"

#ifdef __cplusplus
extern "C" {
#endif

void tyr_exit(int code)
#if defined(__GNUC__) || defined(__clang__)
	__attribute__((noreturn))
#endif
	;

#ifdef __cplusplus
}
#endif

#define exit(code) tyr_exit(code)

#define mkdir(...) ((void)0)

#define FILE    tyr_FILE
#define fopen   tyr_fopen
#define fclose  tyr_fclose
#define fread   tyr_fread
#define fwrite  tyr_fwrite
#define fseek   tyr_fseek
#define ftell   tyr_ftell
#define rewind  tyr_rewind
#define feof    tyr_feof
#define ferror  tyr_ferror
#define fflush  tyr_fflush
#define fgetc   tyr_fgetc
#define fputc   tyr_fputc
#define fputs   tyr_fputs
#define fgets   tyr_fgets

#endif /* PUMPKIN_COMPAT_H */
