/*
 * OpenTyrian 2000 PumpkinOS port: C library functions for the DJGPP
 * (MS-DOS) build.
 *
 * On MS-DOS the application is a DXE3 module loaded by pumpkin.exe, and a
 * module can only import the symbols the host program exports to it
 * (every PumpkinOS API function plus a list of C library functions). The
 * functions below are the ones the game uses that pumpkin.exe does not
 * export; defining them here keeps them inside the module, so loading does
 * not fail with "unresolved symbol".
 *
 * The float math functions use the x87 FPU instructions directly (every
 * CPU the DOS build runs on has one), the rest is plain C.
 */
#if defined(DOS)

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

/* ---- math -------------------------------------------------------------- */

float sinf(float x)
{
	double r;

	__asm__ ("fsin" : "=t" (r) : "0" ((double)x));
	return (float)r;
}

float cosf(float x)
{
	double r;

	__asm__ ("fcos" : "=t" (r) : "0" ((double)x));
	return (float)r;
}

double sin(double x)
{
	double r;

	__asm__ ("fsin" : "=t" (r) : "0" (x));
	return r;
}

float sqrtf(float x)
{
	double r;

	__asm__ ("fsqrt" : "=t" (r) : "0" ((double)x));
	return (float)r;
}

float atanf(float x)
{
	double r;

	/* fpatan computes atan(st(1) / st(0)) and pops: atan(x / 1) */
	__asm__ ("fpatan" : "=t" (r) : "0" (1.0), "u" ((double)x) : "st(1)");
	return (float)r;
}

/* round half away from zero, like C99 roundf() */
float roundf(float x)
{
	union { float f; uint32_t u; } b;
	int e;

	b.f = x;
	e = (int)((b.u >> 23) & 0xFF) - 127;

	if (e >= 23)
		return x;          /* already an integer (or inf/nan) */
	if (e < -1)
		return b.u & 0x80000000u ? -0.0f : 0.0f;   /* |x| < 0.5 */

	{
		uint32_t mask = 0x007FFFFFu >> (e + 1);
		uint32_t half = 0x00400000u >> (e + 1);

		if (e == -1)
		{
			/* 0.5 <= |x| < 1: rounds to +-1 */
			return b.u & 0x80000000u ? -1.0f : 1.0f;
		}
		b.u += half;
		b.u &= ~mask;
	}

	return b.f;
}

/* ---- stdio ------------------------------------------------------------- */

int snprintf(char *str, size_t size, const char *fmt, ...)
{
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = vsnprintf(str, size, fmt, ap);
	va_end(ap);

	return n;
}

/* ---- strings ----------------------------------------------------------- */

size_t strlcpy(char *dst, const char *src, size_t size)
{
	size_t srclen = strlen(src);

	if (size > 0)
	{
		size_t n = srclen < size - 1 ? srclen : size - 1;
		memcpy(dst, src, n);
		dst[n] = '\0';
	}

	return srclen;
}

size_t strlcat(char *dst, const char *src, size_t size)
{
	size_t dstlen = strlen(dst);
	size_t srclen = strlen(src);

	if (dstlen < size)
		strlcpy(dst + dstlen, src, size - dstlen);

	return dstlen + srclen;
}

char *strncat(char *dst, const char *src, size_t n)
{
	char *d = dst + strlen(dst);

	while (n-- > 0 && *src != '\0')
		*d++ = *src++;
	*d = '\0';

	return dst;
}

#endif /* DOS */
