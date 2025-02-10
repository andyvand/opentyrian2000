#ifndef __PRINT_COMPAT_H__
#define __PRINT_COMPAT_H__

#include <stdio.h>

#if !defined(_MSC_VER) && !__STDC_WANT_SECURE_LIB__
#ifndef sprintf_s
#define sprintf_s snprintf
#endif

#ifndef swprintf_s
#include <wchar.h>
#define swprintf_s swprintf
#endif
#endif

#endif /* __PRINT_COMPAT_H__ */
