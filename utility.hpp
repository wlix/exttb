#pragma once

#define _CRT_SECURE_DEPRECATE_MEMORY
#ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN 
#endif

#include <windows.h>

#include <stdlib.h> // malloc
#include <tchar.h>  // string, memory, wchar, tchar 

#include "plugin.hpp"

#if defined(UNICODE) || defined(_UNICODE)
#define tmemcpy			wmemcpy
#define tmemmove		wmemmove
#define tmemset			wmemset
#define tmemcmp			wmemcmp
#define tmalloc(c)		((LPTSTR) malloc((c) << 1))
#define trealloc(p, c)	((LPTSTR) realloc((p), (c) << 1))
#define talloca(c)		((LPTSTR) _alloca((c) << 1))
#else
#define tmemcpy			(char*)memcpy
#define tmemmove		memmove
#define tmemset			memset
#define tmemcmp			memcmp
#define tmalloc(c)		((LPTSTR) malloc(c))
#define trealloc(p, c)	((LPTSTR) realloc((p), (c)))
#define talloca(c)		((LPTSTR) _alloca(c))
#endif

LPTSTR       malloc_tstring       (LPCTSTR src);
void         free_tstring         (LPTSTR str);
PLUGIN_INFO* CopyPluginInfo       (const PLUGIN_INFO* Src);
void         FreePluginInfo       (PLUGIN_INFO* PLUGIN_INFO);
void         GetVersion           (LPTSTR Filename, DWORD* VersionMS, DWORD* VersionLS);
BOOL         ExecutePluginCommand (LPCTSTR pluginName, INT32 CmdID);

#if NO_WRITELOG
  #define write_log(logLevel, format, ...)
#else
  void write_log(ERROR_LEVEL logLevel, LPCTSTR format, ...);
#endif
