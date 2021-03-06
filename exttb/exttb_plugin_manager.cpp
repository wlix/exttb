#include "exttb_plugin_manager.h"

#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib") // PathRemoveFileSpec, PathRelativePathTo

#include <array>

#include "../utility.hpp"

#if SYS_DEBUG
template<typename C, typename... Args>
void SystemLog(const C* const format, Args... args) {
  WriteLog(ERROR_LEVEL(5), format, args...);
}
#else
  #define SystemLog(format, ...)
#endif

extern HINSTANCE g_hInst;

TTBasePlugin::~TTBasePlugin() {
  if (nullptr == m_handle && nullptr == m_info){ return; }

  Free();
  FreeInfo();
}

void TTBasePlugin::swap(TTBasePlugin&& rhs) noexcept {
    if (this == &rhs) { return; }

    std::swap(m_path,   rhs.m_path);
    std::swap(m_handle, rhs.m_handle);
    std::swap(m_info,   rhs.m_info);

    std::swap(TTBEvent_InitPluginInfo, rhs.TTBEvent_InitPluginInfo);
    std::swap(TTBEvent_FreePluginInfo, rhs.TTBEvent_FreePluginInfo);
    std::swap(TTBEvent_Init,           rhs.TTBEvent_Init);
    std::swap(TTBEvent_Unload,         rhs.TTBEvent_Unload);
    std::swap(TTBEvent_Execute,        rhs.TTBEvent_Execute);
    std::swap(TTBEvent_WindowsHook,    rhs.TTBEvent_WindowsHook);
}

void TTBasePlugin::info(PLUGIN_INFO* info) {
  FreePluginInfo(m_info);
  m_info = CopyPluginInfo(info);
}

bool TTBasePlugin::Load(LPCTSTR plugin_path) {
  SystemLog(TEXT("%s"), TEXT("プラグインの読み込み"));
  SystemLog(TEXT("  %s"), plugin_path);

  if (m_handle) {
    SystemLog(TEXT("  %s"), TEXT("読み込み済み"));
    return true;
  }

  // DLLの読み込み
  m_handle = ::LoadLibraryEx(plugin_path, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
  if (nullptr == m_handle) {
    SystemLog(TEXT("  %s"), TEXT("NG"));
    return false;
  }

  // DLLのフルパスを取得
  ::GetModuleFileName(m_handle, m_path, MAX_PATH);

  // 関数ポインタの取得 (必須)
  TTBEvent_InitPluginInfo = (TTBEVENT_INITPLUGININFO)::GetProcAddress(m_handle, "TTBEvent_InitPluginInfo");
  TTBEvent_FreePluginInfo = (TTBEVENT_FREEPLUGININFO)::GetProcAddress(m_handle, "TTBEvent_FreePluginInfo");

  // 必須APIを実装しているか
  if (nullptr == TTBEvent_InitPluginInfo || nullptr == TTBEvent_FreePluginInfo) {
    SystemLog(TEXT("  %s"), TEXT("有効な TTBase プラグインではありません"));

    ::FreeLibrary(m_handle);
    m_handle = nullptr;

    return false;
  }

  // 関数ポインタの取得
  TTBEvent_Init        = (TTBEVENT_INIT)       ::GetProcAddress(m_handle, "TTBEvent_Init");
  TTBEvent_Unload      = (TTBEVENT_UNLOAD)     ::GetProcAddress(m_handle, "TTBEvent_Unload");
  TTBEvent_Execute     = (TTBEVENT_EXECUTE)    ::GetProcAddress(m_handle, "TTBEvent_Execute");
  TTBEvent_WindowsHook = (TTBEVENT_WINDOWSHOOK)::GetProcAddress(m_handle, "TTBEvent_WindowsHook");

  SystemLog(TEXT("  %s"), TEXT("OK"));
  return true;
}

void TTBasePlugin::Free() {
  Unload();

  SystemLog(TEXT("%s"), TEXT("プラグインを解放"));
  SystemLog(TEXT("  %s"), m_path);

  if (nullptr == m_handle) {
    SystemLog(TEXT("  %s"), TEXT("解放済み"));
    return;
  }

  TTBEvent_InitPluginInfo = nullptr;
  TTBEvent_FreePluginInfo = nullptr;
  TTBEvent_Init = nullptr;
  TTBEvent_Unload = nullptr;
  TTBEvent_Execute = nullptr;
  TTBEvent_WindowsHook = nullptr;

  ::FreeLibrary(m_handle);
  m_handle = nullptr;

  // Reload() する時のため m_path と m_info は記憶しておく

  SystemLog(TEXT("  %s"), TEXT("OK"));
}

bool TTBasePlugin::Reload() {
  bool result;

  SystemLog(TEXT("%s"), TEXT("プラグインの再読み込み"));

  // プラグインの読み込み
  result = Load(m_path);
  if (!result) { return false; }

  // 相対パスの取得
  std::array<TCHAR, MAX_PATH> exe_path;
  std::array<TCHAR, MAX_PATH> relative_path;

  ::GetModuleFileName(g_hInst, exe_path.data(), (DWORD)exe_path.size());
  ::PathRelativePathTo
  (
    relative_path.data(),
    exe_path.data(), FILE_ATTRIBUTE_ARCHIVE,
    m_path, FILE_ATTRIBUTE_ARCHIVE
  );

  // relative_path の 先頭2文字 (".\") は要らないので ずらす
  const auto PluginFilename = relative_path.data() + 2;

  // プラグイン情報の再取得
  //InitInfo(PluginFilename);

  // プラグインの初期化
  result = Init(PluginFilename, (DWORD_PTR)this);
  if (!result) {
    Unload();
    return false;
  }

  SystemLog(TEXT("  %s"), TEXT("OK"));
  return true;
}

bool TTBasePlugin::InitInfo(LPTSTR PluginFilename) {
  SystemLog(TEXT("%s"), TEXT("プラグイン情報をコピー"));
  SystemLog(TEXT("  %s"), m_path);

  if (nullptr == TTBEvent_InitPluginInfo) {
    SystemLog(TEXT("  %s"), TEXT("NG"));
    return false;
  }

  // プラグイン情報の取得
  const auto tmp = TTBEvent_InitPluginInfo(PluginFilename);
  if (nullptr == tmp) {
    SystemLog(TEXT("  %s"), TEXT("情報取得失敗"));
    return false;
  }

  // プラグイン情報をコピー
  if (m_info) { FreeInfo(); }
  m_info = CopyPluginInfo(tmp);

  // コピーし終わったので 元データを解放
  TTBEvent_FreePluginInfo(tmp);

  if (nullptr == m_info) {
    SystemLog(TEXT("  %s"), TEXT("情報コピー失敗"));
    return false;
  }

  // ロードにかかった時間は一律 0 にする
  m_info->LoadTime = 0;

  SystemLog(TEXT("  名前:       %s"), m_info->Name);
  SystemLog(TEXT("  相対パス:   %s"), m_info->Filename);
  SystemLog(TEXT("  タイプ:     %s"), m_info->PluginType == ptAlwaysLoad ? TEXT("常駐") : TEXT("都度"));
  SystemLog(TEXT("  コマンド数: %u"), m_info->CommandCount);

  SystemLog(TEXT("  %s"), TEXT("OK"));
  return true;
}

//---------------------------------------------------------------------------//

void TTBasePlugin::FreeInfo() {
  SystemLog(TEXT("%s"), TEXT("プラグイン情報を解放"));

  if (m_info) {
    FreePluginInfo(m_info);
    m_info = nullptr;
  }

  SystemLog(TEXT("  %s"), TEXT("OK"));
}

//---------------------------------------------------------------------------//

bool TTBasePlugin::Init(LPTSTR PluginFilename, DWORD_PTR hPlugin) {
  SystemLog(TEXT("%s"), TEXT("プラグインの初期化"));
  SystemLog(TEXT("  %s"), m_path);

  if (nullptr == TTBEvent_Init) {
    SystemLog(TEXT("  %s"), TEXT("nullptr"));
    return true;
  }

  BOOL result{ FALSE };
  try {
    result = TTBEvent_Init(PluginFilename, hPlugin);
    SystemLog(TEXT("  %s"), result ? TEXT("OK") : TEXT("NG"));
  } catch (...) {
    write_log(ERROR_LEVEL::elWarning, TEXT("  %s"), TEXT("Init: 例外が発生しました"));
  }

  return result ? true : false;
}

//---------------------------------------------------------------------------//

void TTBasePlugin::Unload() {
  SystemLog(TEXT("%s"), TEXT("プラグインの終了処理"));
  SystemLog(TEXT("  %s"), m_path);

  if (nullptr == TTBEvent_Unload) {
    SystemLog(TEXT("  %s"), TEXT("nullptr"));
    return;
  }

  try {
    TTBEvent_Unload();
    SystemLog(TEXT("  %s"), TEXT("OK"));
  } catch (...) {
    write_log(ERROR_LEVEL::elWarning, TEXT("  %s"), TEXT("Unload: 例外が発生しました"));
  }
}

//---------------------------------------------------------------------------//

bool TTBasePlugin::Execute(INT32 CmdID, HWND hwnd) {
  SystemLog(TEXT("%s"), TEXT("コマンドの実行"));
  SystemLog(TEXT("  %s|%i"), info()->Filename, CmdID);

  if (nullptr == TTBEvent_Execute) {
    SystemLog(TEXT("  %s"), TEXT("nullptr"));
    return true;
  }

  BOOL result{ FALSE };
  try {
    result = TTBEvent_Execute(CmdID, hwnd);
    SystemLog(TEXT("  %s"), result ? TEXT("OK") : TEXT("NG"));
  } catch (...) {
    write_log(ERROR_LEVEL::elWarning, TEXT("  %s"), TEXT("Execute: 例外が発生しました"));
  }

  return result ? true : false;
}

//---------------------------------------------------------------------------//

void TTBasePlugin::Hook(UINT Msg, WPARAM wParam, LPARAM lParam) {
  //SystemLog(TEXT("%s"), TEXT("フックプロシージャの実行"));
  //SystemLog(TEXT("  %s"), m_path);
  //SystemLog(TEXT("  Msg = 0x%X"), Msg);

  if (nullptr == TTBEvent_WindowsHook) {
    //SystemLog(TEXT("  %s"), TEXT("nullptr"));
    return;
  }

  try {
    TTBEvent_WindowsHook(Msg, wParam, lParam);
    //SystemLog(TEXT("  %s"), TEXT("OK"));
  }
  catch (...) {
    write_log(ERROR_LEVEL::elWarning, TEXT("  %s"), TEXT("Hook: 例外が発生しました"));
  }
}

//---------------------------------------------------------------------------//
// SystemPlugin
//---------------------------------------------------------------------------//

SystemPlugin::SystemPlugin() {
  SystemLog(TEXT("%s"), TEXT("システムプラグインを生成"));

  m_handle = g_hInst;
  ::GetModuleFileName(m_handle, m_path, MAX_PATH);

  TTBEvent_InitPluginInfo = ::TTBEvent_InitPluginInfo;
  TTBEvent_FreePluginInfo = ::TTBEvent_FreePluginInfo;
  TTBEvent_Init           = ::TTBEvent_Init;
  TTBEvent_Unload         = ::TTBEvent_Unload;
  TTBEvent_Execute        = ::TTBEvent_Execute;
  TTBEvent_WindowsHook    = ::TTBEvent_WindowsHook;

  m_info = &g_info;
  TTBEvent_Init((LPTSTR)TEXT(":system"), (DWORD_PTR)this);

  SystemLog(TEXT("  %s"), TEXT("OK"));
}

//---------------------------------------------------------------------------//

SystemPlugin::~SystemPlugin() {
  SystemLog(TEXT("%s"), TEXT("システムプラグインを解放"));

  TTBEvent_Unload();
  m_info = nullptr;

  TTBEvent_InitPluginInfo = nullptr;
  TTBEvent_FreePluginInfo = nullptr;
  TTBEvent_Init           = nullptr;
  TTBEvent_Unload         = nullptr;
  TTBEvent_Execute        = nullptr;
  TTBEvent_WindowsHook    = nullptr;

  m_handle = nullptr;

  SystemLog(TEXT("  %s"), TEXT("OK"));
}

//---------------------------------------------------------------------------//
// PluginMgr
//---------------------------------------------------------------------------//

PluginMgr::PluginMgr() {
  SystemLog(TEXT("%s"), TEXT("プラグインマネージャを生成"));

  // プラグインリストの一番最初にシステムプラグインを登録
  plugins.emplace_back(new SystemPlugin);

  SystemLog(TEXT("  %s"), TEXT("OK"));
}

//---------------------------------------------------------------------------//

PluginMgr::~PluginMgr() {
  SystemLog(TEXT("%s"), TEXT("プラグインマネージャを解放"));

  // ロードとは逆順に解放する
  FreeAll();

  // システムプラグインも解放
  plugins.clear();

  SystemLog(TEXT("  %s"), TEXT("OK"));
}

//---------------------------------------------------------------------------//

void PluginMgr::LoadAll() {
  SystemLog(TEXT("%s"), TEXT("すべてのプラグインを読み込み"));

  // インストールフォルダのパスを取得
  std::array<TCHAR, MAX_PATH> dir_path;
  ::GetModuleFileName(g_hInst, dir_path.data(), (DWORD)dir_path.size());
  ::PathRemoveFileSpec(dir_path.data());

  // インストールフォルダ以下の DLL ファイルを収集
  CollectFile(dir_path.data(), TEXT(".dll"));

  // すべてのプラグインの情報を取得
  InitInfoAll();

  // すべてのプラグインを初期化
  InitAll();

  write_log(ERROR_LEVEL::elInfo, TEXT("%u plugin(s)"), plugins.size() - 1);
  SystemLog(TEXT("  %s"), TEXT("OK"));
}

//---------------------------------------------------------------------------//

void PluginMgr::FreeAll() {
  SystemLog(TEXT("%s"), TEXT("すべてのプラグインを解放"));

  // ロードとは逆順に解放 ... システムプラグインは解放しない
  while (plugins.size() > 1) {
    plugins.pop_back();
  }

  SystemLog(TEXT("  %s"), TEXT("OK"));
}

//---------------------------------------------------------------------------//

const ITTBPlugin* PluginMgr::Find(LPCTSTR PluginFilename) const noexcept {
  for (auto&& plugin : plugins) {
    if (0 == lstrcmp(PluginFilename, plugin->info()->Filename)) {
      return plugin.get();
    }
  }
  return nullptr;
}

//---------------------------------------------------------------------------//

void PluginMgr::CollectFile(LPCTSTR dir_path, LPCTSTR ext) {
  TCHAR path[MAX_PATH];
  _sntprintf(path, MAX_PATH, TEXT(R"(%s\*)"), dir_path);

  WIN32_FIND_DATA fd{ };
  const auto hFindFile = ::FindFirstFile(path, &fd);
  if (INVALID_HANDLE_VALUE == hFindFile) {
    // 指定されたフォルダが見つからなかった
    return;
  }

  // フォルダ内にあるものを列挙する
  do {
    // 隠しファイルは飛ばす
    if (fd.cFileName[0] == '.') {
      continue;
    }
    if (fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) {
      continue;
    }

    // Hook.dllは飛ばす
    if (0 == lstrcmp(TEXT("Hook.dll"), fd.cFileName)) {
      continue;
    }

    // フルパスを合成
    _sntprintf(path, MAX_PATH, TEXT(R"(%s\%s)"), dir_path, fd.cFileName);

    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      // サブフォルダを検索
      CollectFile(path, ext);
    }
    else {
      // 拡張子を取得
      TCHAR* s;
      for (s = path + lstrlen(path) - 1; *s != '.'; --s);

      // 目的の拡張子だったら
      if (0 == lstrcmp(s, ext)) {
        // コンテナに収録
        ITTBPlugin* plugin;

        plugin = new TTBasePlugin(path); // 通常のプラグイン
        if (plugin->is_loaded()) {
          plugins.emplace_back(plugin);
          continue;
        }
        else {
          if (plugin) { delete plugin; }
        }
      }
    }
  } while (::FindNextFile(hFindFile, &fd));

  ::FindClose(hFindFile);

  return;
}

//---------------------------------------------------------------------------//

void PluginMgr::InitInfoAll() {
  std::array<TCHAR, MAX_PATH> exe_path;
  std::array<TCHAR, MAX_PATH> relative_path;

  // 本体の絶対パスを取得
  ::GetModuleFileName(g_hInst, exe_path.data(), (DWORD)exe_path.size());

  // プラグイン情報の取得
  // * システムプラグインは既に初期化済みのため、リストの2番目から開始
  auto it = ++plugins.begin();
  while (it != plugins.end()) {
    auto&& plugin = *it;
    ::PathRelativePathTo
    (
      relative_path.data(),
      exe_path.data(), FILE_ATTRIBUTE_ARCHIVE,
      plugin->path(), FILE_ATTRIBUTE_ARCHIVE
    );

    // relative_path の 先頭2文字 (".\") は要らないので ずらす
    const auto PluginFilename = relative_path.data() + 2;

    const auto result = plugin->InitInfo(PluginFilename);
    if (!result) {
      it = plugins.erase(it);
      continue;
    }

    // 常駐型でなければ解放
    if (plugin->info()->PluginType != ptAlwaysLoad) {
      plugin->Free();
    }

    ++it;
  }
}

//---------------------------------------------------------------------------//

void PluginMgr::InitAll() {
  std::array<TCHAR, MAX_PATH> exe_path;
  std::array<TCHAR, MAX_PATH> relative_path;

  // 本体の絶対パスを取得
  ::GetModuleFileName(g_hInst, exe_path.data(), (DWORD)exe_path.size());

  // プラグインの初期化
  // * システムプラグインは既に初期化済みのため、リストの2番目から開始
  auto it = ++plugins.begin();
  const auto end = plugins.end();
  while (it != end) {
    auto&& plugin = *it;
    if (plugin->is_loaded()) {
      ::PathRelativePathTo
      (
        relative_path.data(),
        exe_path.data(), FILE_ATTRIBUTE_ARCHIVE,
        plugin->path(), FILE_ATTRIBUTE_ARCHIVE
      );

      // relative_path の 先頭2文字 (".\") は要らないので ずらす
      const auto PluginFilename = relative_path.data() + 2;

      plugin->Init(PluginFilename, (DWORD_PTR)plugin.get());
    }

    ++it;
  }
}