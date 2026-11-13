/*
 * OpenTyrian2000 - classic Mac OS (Retro68) start-up helper.
 *
 * Classic Mac OS gives an application a tiny stack (8-32 KB on 68k) that
 * grows down towards the application heap. Enlarge it and grow the heap to
 * its full size before anything allocates, then pre-allocate master
 * pointer blocks so that the heap does not get fragmented by them later.
 *
 * With -DMACCLASSIC_LOG (make MACCLASSIC_LOG=1), stdout and stderr, which
 * Retro68 otherwise discards, go to "OpenTyrian2000 log" in the
 * application's folder.
 */

#include <Memory.h>
#include <LowMem.h>

#ifdef MACCLASSIC_LOG
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#endif

#ifndef MACCLASSIC_STACK_SIZE
#define MACCLASSIC_STACK_SIZE (256L * 1024L)
#endif

__attribute__((constructor(101)))
static void macclassic_init(void)
{
#ifdef __m68k__
	Ptr limit = LMGetApplLimit();   // no GetApplLimit() glue on 68k
#else
	Ptr limit = GetApplLimit();     // InterfaceLib
#endif
	SetApplLimit(limit - MACCLASSIC_STACK_SIZE);
	MaxApplZone();

	for (int i = 0; i < 8; ++i)
		MoreMasters();

#ifdef MACCLASSIC_LOG
	// unbuffered, so that the log is complete even if the Mac crashes
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);
#endif
}

#ifdef MACCLASSIC_LOG
// Replace Retro68's no-op hooks that read()/write() use for fds 0-2
// (both, since they live in the same object file of libretrocrt).
ssize_t _consoleread(int fd, void *buf, size_t count);
ssize_t _consoleread(int fd, void *buf, size_t count)
{
	(void)fd; (void)buf; (void)count;
	return 0;
}

ssize_t _consolewrite(int fd, const void *buf, size_t count);
ssize_t _consolewrite(int fd, const void *buf, size_t count)
{
	static int log_fd = -2;

	(void)fd;
	if (log_fd == -2)
		log_fd = open(":OpenTyrian2000 log", O_WRONLY | O_CREAT | O_TRUNC);  // app folder
	if (log_fd < 0)
		return count;
	return write(log_fd, buf, count);
}
#endif
