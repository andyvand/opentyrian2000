#ifndef __OS_COMPAT_H__
#define __OS_COMPAT_H__

// if windows
#pragma once

#ifdef _WIN32
#include <windows.h>
#else
#ifdef __GNUC__
#include <unistd.h>
#endif

#include <stdarg.h>
#include <stdint.h>
#include <wchar.h>

#ifndef MAX_PATH
#define MAX_PATH PATH_MAX
#endif

#ifndef WCHAR
#define WCHAR wchar_t
#endif

#ifndef FOURCC
#define FOURCC uint32_t
#endif

#ifndef mmioFOURCC
#define mmioFOURCC(ch0, ch1, ch2, ch3) \
    ((uint32_t)(uint8_t)(ch0) | ((uint32_t)(uint8_t)(ch1) << 8) | \
    ((uint32_t)(uint8_t)(ch2) << 16) | ((uint32_t)(uint8_t)(ch3) << 24))
#endif

#ifdef __cplusplus
extern "C" {
#endif
    void OutputDebugStringW(const WCHAR * fmt, ...);
#ifdef __cplusplus
}
# endif
#endif

#endif /* __OS_COMPAT_H__ */
