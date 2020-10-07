#include "plugin.hpp"
#include "utility.hpp"

#pragma comment(lib, "version.lib") // VerQueryValue

// 文字列の格納領域を確保し、文字列をコピーして返す
LPTSTR malloc_tstring(LPCTSTR src) {
  const auto len = 1 + _tcslen(src);
  auto dst = tmalloc(len);
  if (dst != nullptr) { tmemcpy(dst, src, len); }
  return dst;
}

// 文字列を削除する
void free_tstring(LPTSTR str) {
  if (str != nullptr) { free(str); str = nullptr; }
}

// プラグイン情報構造体をディープコピーして返す
PLUGIN_INFO* CopyPluginInfo(const PLUGIN_INFO *Src) {
  if (Src == nullptr) { return nullptr; }

  const auto info = (PLUGIN_INFO *)malloc(sizeof(PLUGIN_INFO));

  if (info != nullptr) {
    ZeroMemory(info, sizeof(PLUGIN_INFO));
    *info = *Src;

    // プラグイン情報のコピー
    info->Name     = malloc_tstring(Src->Name);
    info->Filename = malloc_tstring(Src->Filename);
    if (Src->CommandCount == 0) {
      info->Commands = nullptr;
    }
    else {
      info->Commands = (PLUGIN_COMMAND_INFO *)malloc(Src->CommandCount * sizeof(PLUGIN_COMMAND_INFO));
      if (info->Commands != nullptr) { ZeroMemory(info->Commands, Src->CommandCount * sizeof(PLUGIN_COMMAND_INFO)); }
    }
    // コマンド情報のコピー
    if (info->Commands != nullptr && Src->Commands != nullptr) {
      for (size_t i = 0; i < Src->CommandCount; ++i) {
        info->Commands[i] = Src->Commands[i];

        info->Commands[i].Name    = malloc_tstring(Src->Commands[i].Name);
        info->Commands[i].Caption = malloc_tstring(Src->Commands[i].Caption);
      }
    }
  }

  return info;
}

// プラグイン側で作成されたプラグイン情報構造体を破棄する
void FreePluginInfo(PLUGIN_INFO* PluginInfo) {
    if (PluginInfo == nullptr) { return; }

    // コマンド情報構造体配列の破棄
    if (PluginInfo->Commands != nullptr)
    {
        for (size_t i = 0; i < PluginInfo->CommandCount; ++i) {
            const auto pCI = &PluginInfo->Commands[i];

            free_tstring(pCI->Name);
            free_tstring(pCI->Caption);
        }
        free(PluginInfo->Commands);
    }

    free_tstring(PluginInfo->Filename);
    free_tstring(PluginInfo->Name);

    free(PluginInfo);
}

//---------------------------------------------------------------------------//

// バージョン情報を返す
void GetVersion(LPTSTR Filename, DWORD* VersionMS, DWORD* VersionLS) {
  if (VersionMS == nullptr || VersionLS == nullptr) { return; }

  // DLL ファイルに埋め込まれたバージョンリソースのサイズを取得
  DWORD VersionHandle;
  const auto VersionSize = GetFileVersionInfoSize(Filename, &VersionHandle);
  if (VersionSize == 0) { return; }

  // バージョンリソースを読み込む
  const auto pVersionInfo = (BYTE *)malloc(VersionSize * sizeof(BYTE));
  if (pVersionInfo != nullptr) {
    ZeroMemory(pVersionInfo, VersionSize * sizeof(BYTE));
    if (VersionHandle == 0 && GetFileVersionInfo(Filename, VersionHandle, VersionSize, pVersionInfo)) {
      VS_FIXEDFILEINFO* FixedFileInfo;
      UINT itemLen;

      // バージョンリソースからファイルバージョンを取得
      if (VerQueryValue(pVersionInfo, (LPTSTR)TEXT("\\"), (void **)&FixedFileInfo, &itemLen)) {
        *VersionMS = FixedFileInfo->dwFileVersionMS;
        *VersionLS = FixedFileInfo->dwFileVersionLS;
      }
    }
    free(pVersionInfo);
  }
}

//---------------------------------------------------------------------------//

// ほかのプラグインのコマンドを実行する
BOOL ExecutePluginCommand(LPCTSTR pluginName, INT32 CmdID) {
  #if defined(_USRDLL)
    // 本体が TTBPlugin_ExecuteCommand をエクスポートしていない場合は何もしない
    if (TTBPlugin_ExecuteCommand == nullptr) { return TRUE; }
  #endif

    return TTBPlugin_ExecuteCommand(pluginName, CmdID);
}

//---------------------------------------------------------------------------//

// ログを出力する
#if NO_WRITELOG
  #define write_log(loglevel, format, ...)
#else
void write_log(ERROR_LEVEL loglevel, LPCTSTR format, ...) {
  #if defined(_USRDLL)
    // 本体が TTBPlugin_WriteLog をエクスポートしていない場合は何もしない
    if ( TTBPlugin_WriteLog == nullptr ) { return; }
  #endif

    constexpr size_t BUF_SIZE { 1024 + 1 };
    static TCHAR msg[BUF_SIZE];
    tmemset(msg, TEXT('\0'), BUF_SIZE);
    // TODO: 排他制御

    // 文字列の結合
    va_list args;
    va_start(args, format);
    _vsntprintf(msg, BUF_SIZE - 1, format, args);
    va_end(args);
    msg[BUF_SIZE - 1] = TEXT('\0');

    // ログの出力
    TTBPlugin_WriteLog(g_hPlugin, loglevel, msg);
}
#endif

//---------------------------------------------------------------------------//

// Utility.cpp
