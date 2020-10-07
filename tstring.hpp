#pragma once

#define _CRT_SECURE_DEPRECATE_MEMORY
#include <stdlib.h>
#include <tchar.h>

#if defined(UNICODE) || defined(_UNICODE)
#define tmemcpy			wmemcpy
#define tmemmove		wmemmove
#define tmemset			wmemset
#define tmemcmp			wmemcmp
#define tmalloc(c)		 ((TCHAR *) malloc((c) << 1))
#define trealloc(p, c) ((TCHAR *) realloc((p), (c) << 1))
#define talloca(c)		 ((TCHAR *) _alloca((c) << 1))
#else
#define tmemcpy			(char*)memcpy
#define tmemmove		memmove
#define tmemset			memset
#define tmemcmp			memcmp
#define tmalloc(c)		((TCHAR *) malloc(c))
#define trealloc(p, c)	((TCHAR *) realloc((p), (c)))
#define talloca(c)		((TCHAR *) _alloca(c))
#endif
