/*
 * OpenTyrian 2000 PumpkinOS port: stdio replacement.
 *
 * The game reads its data files and reads/writes its configuration and
 * save games through the C stdio API (FILE *, fread, fwrite, ...).
 * On PumpkinOS the data files are bundled as PRC resources and the user
 * files live in the PalmOS VFS, neither of which is reachable through
 * the host C library. pumpkin_compat.h (force-included into every game
 * source file) maps the stdio calls onto these functions, which are
 * implemented in pumpkin_app.c.
 *
 * A path of the form "res:/<name>" opens the bundled data resource of
 * that name (read only). Any other path is a PalmOS VFS path.
 */
#ifndef PUMPKIN_STDIO_H
#define PUMPKIN_STDIO_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tyr_FILE tyr_FILE;

#define TYR_RES_PREFIX "res:/"

tyr_FILE *tyr_fopen(const char *path, const char *mode);
int tyr_fclose(tyr_FILE *f);
size_t tyr_fread(void *ptr, size_t size, size_t nmemb, tyr_FILE *f);
size_t tyr_fwrite(const void *ptr, size_t size, size_t nmemb, tyr_FILE *f);
int tyr_fseek(tyr_FILE *f, long offset, int whence);
long tyr_ftell(tyr_FILE *f);
void tyr_rewind(tyr_FILE *f);
int tyr_feof(tyr_FILE *f);
int tyr_ferror(tyr_FILE *f);
int tyr_fflush(tyr_FILE *f);
int tyr_fgetc(tyr_FILE *f);
int tyr_fputc(int c, tyr_FILE *f);
int tyr_fputs(const char *s, tyr_FILE *f);
char *tyr_fgets(char *s, int size, tyr_FILE *f);

#ifdef __cplusplus
}
#endif

#endif /* PUMPKIN_STDIO_H */
