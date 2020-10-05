#pragma once

#include <windows.h>

#include "tstring.hpp"
#include "plugin.hpp"

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
