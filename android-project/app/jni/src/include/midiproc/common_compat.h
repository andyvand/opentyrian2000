#ifndef __COMMON_COMPAT_H__
#define __COMMON_COMPAT_H__

#pragma once

#ifndef _countof
#define _countof(x) (sizeof(x) / sizeof((x)[0]))
#endif

#endif /* __COMMON_COMPAT_H__ */
