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
  SystemLog(TEXT("%s"), TEXT("�v���O�C���̓ǂݍ���"));
  SystemLog(TEXT("  %s"), plugin_path);

  if (m_handle) {
    SystemLog(TEXT("  %s"), TEXT("�ǂݍ��ݍς�"));
    return true;
  }

  // DLL�̓ǂݍ���
  m_handle = ::LoadLibraryEx(plugin_path, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
  if (nullptr == m_handle) {
    SystemLog(TEXT("  %s"), TEXT("NG"));
    return false;
  }

  // DLL�̃t���p�X���擾
  ::GetModuleFileName(m_handle, m_path, MAX_PATH);

  // �֐��|�C���^�̎擾 (�K�{)
  TTBEvent_InitPluginInfo = (TTBEVENT_INITPLUGININFO)::GetProcAddress(m_handle, "TTBEvent_InitPluginInfo");
  TTBEvent_FreePluginInfo = (TTBEVENT_FREEPLUGININFO)::GetProcAddress(m_handle, "TTBEvent_FreePluginInfo");

  // �K�{API���������Ă��邩
  if (nullptr == TTBEvent_InitPluginInfo || nullptr == TTBEvent_FreePluginInfo) {
    SystemLog(TEXT("  %s"), TEXT("�L���� TTBase �v���O�C���ł͂���܂���"));

    ::FreeLibrary(m_handle);
    m_handle = nullptr;

    return false;
  }

  // �֐��|�C���^�̎擾
  TTBEvent_Init        = (TTBEVENT_INIT)       ::GetProcAddress(m_handle, "TTBEvent_Init");
  TTBEvent_Unload      = (TTBEVENT_UNLOAD)     ::GetProcAddress(m_handle, "TTBEvent_Unload");
  TTBEvent_Execute     = (TTBEVENT_EXECUTE)    ::GetProcAddress(m_handle, "TTBEvent_Execute");
  TTBEvent_WindowsHook = (TTBEVENT_WINDOWSHOOK)::GetProcAddress(m_handle, "TTBEvent_WindowsHook");

  SystemLog(TEXT("  %s"), TEXT("OK"));
  return true;
}

void TTBasePlugin::Free() {
  Unload();

  SystemLog(TEXT("%s"), TEXT("�v���O�C�������"));
  SystemLog(TEXT("  %s"), m_path);

  if (nullptr == m_handle) {
    SystemLog(TEXT("  %s"), TEXT("����ς�"));
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

  // Reload() ���鎞�̂��� m_path �� m_info �͋L�����Ă���

  SystemLog(TEXT("  %s"), TEXT("OK"));
}

bool TTBasePlugin::Reload() {
  bool result;

  SystemLog(TEXT("%s"), TEXT("�v���O�C���̍ēǂݍ���"));

  // �v���O�C���̓ǂݍ���
  result = Load(m_path);
  if (!result) { return false; }

  // ���΃p�X�̎擾
  std::array<TCHAR, MAX_PATH> exe_path;
  std::array<TCHAR, MAX_PATH> relative_path;

  ::GetModuleFileName(g_hInst, exe_path.data(), (DWORD)exe_path.size());
  ::PathRelativePathTo
  (
    relative_path.data(),
    exe_path.data(), FILE_ATTRIBUTE_ARCHIVE,
    m_path, FILE_ATTRIBUTE_ARCHIVE
  );

  // relative_path �� �擪2���� (".\") �͗v��Ȃ��̂� ���炷
  const auto PluginFilename = relative_path.data() + 2;

  // �v���O�C�����̍Ď擾
  //InitInfo(PluginFilename);

  // �v���O�C���̏�����
  result = Init(PluginFilename, (DWORD_PTR)this);
  if (!result) {
    Unload();
    return false;
  }

  SystemLog(TEXT("  %s"), TEXT("OK"));
  return true;
}

bool TTBasePlugin::InitInfo(LPTSTR PluginFilename) {
  SystemLog(TEXT("%s"), TEXT("�v���O�C�������R�s�["));
  SystemLog(TEXT("  %s"), m_path);

  if (nullptr == TTBEvent_InitPluginInfo) {
    SystemLog(TEXT("  %s"), TEXT("NG"));
    return false;
  }

  // �v���O�C�����̎擾
  const auto tmp = TTBEvent_InitPluginInfo(PluginFilename);
  if (nullptr == tmp) {
    SystemLog(TEXT("  %s"), TEXT("���擾���s"));
    return false;
  }

  // �v���O�C�������R�s�[
  if (m_info) { FreeInfo(); }
  m_info = CopyPluginInfo(tmp);

  // �R�s�[���I������̂� ���f�[�^�����
  TTBEvent_FreePluginInfo(tmp);

  if (nullptr == m_info) {
    SystemLog(TEXT("  %s"), TEXT("���R�s�[���s"));
    return false;
  }

  // ���[�h�ɂ����������Ԃ͈ꗥ 0 �ɂ���
  m_info->LoadTime = 0;

  SystemLog(TEXT("  ���O:       %s"), m_info->Name);
  SystemLog(TEXT("  ���΃p�X:   %s"), m_info->Filename);
  SystemLog(TEXT("  �^�C�v:     %s"), m_info->PluginType == ptAlwaysLoad ? TEXT("�풓") : TEXT("�s�x"));
  SystemLog(TEXT("  �R�}���h��: %u"), m_info->CommandCount);

  SystemLog(TEXT("  %s"), TEXT("OK"));
  return true;
}

//---------------------------------------------------------------------------//

void TTBasePlugin::FreeInfo() {
  SystemLog(TEXT("%s"), TEXT("�v���O�C���������"));

  if (m_info) {
    FreePluginInfo(m_info);
    m_info = nullptr;
  }

  SystemLog(TEXT("  %s"), TEXT("OK"));
}

//---------------------------------------------------------------------------//

bool TTBasePlugin::Init(LPTSTR PluginFilename, DWORD_PTR hPlugin) {
  SystemLog(TEXT("%s"), TEXT("�v���O�C���̏�����"));
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
    write_log(ERROR_LEVEL::elWarning, TEXT("  %s"), TEXT("Init: ��O���������܂���"));
  }

  return result ? true : false;
}

//---------------------------------------------------------------------------//

void TTBasePlugin::Unload() {
  SystemLog(TEXT("%s"), TEXT("�v���O�C���̏I������"));
  SystemLog(TEXT("  %s"), m_path);

  if (nullptr == TTBEvent_Unload) {
    SystemLog(TEXT("  %s"), TEXT("nullptr"));
    return;
  }

  try {
    TTBEvent_Unload();
    SystemLog(TEXT("  %s"), TEXT("OK"));
  } catch (...) {
    write_log(ERROR_LEVEL::elWarning, TEXT("  %s"), TEXT("Unload: ��O���������܂���"));
  }
}

//---------------------------------------------------------------------------//

bool TTBasePlugin::Execute(INT32 CmdID, HWND hwnd) {
  SystemLog(TEXT("%s"), TEXT("�R�}���h�̎��s"));
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
    write_log(ERROR_LEVEL::elWarning, TEXT("  %s"), TEXT("Execute: ��O���������܂���"));
  }

  return result ? true : false;
}

//---------------------------------------------------------------------------//

void TTBasePlugin::Hook(UINT Msg, WPARAM wParam, LPARAM lParam) {
  //SystemLog(TEXT("%s"), TEXT("�t�b�N�v���V�[�W���̎��s"));
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
    write_log(ERROR_LEVEL::elWarning, TEXT("  %s"), TEXT("Hook: ��O���������܂���"));
  }
}

//---------------------------------------------------------------------------//
// SystemPlugin
//---------------------------------------------------------------------------//

SystemPlugin::SystemPlugin() {
  SystemLog(TEXT("%s"), TEXT("�V�X�e���v���O�C���𐶐�"));

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
  SystemLog(TEXT("%s"), TEXT("�V�X�e���v���O�C�������"));

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
  SystemLog(TEXT("%s"), TEXT("�v���O�C���}�l�[�W���𐶐�"));

  // �v���O�C�����X�g�̈�ԍŏ��ɃV�X�e���v���O�C����o�^
  plugins.emplace_back(new SystemPlugin);

  SystemLog(TEXT("  %s"), TEXT("OK"));
}

//---------------------------------------------------------------------------//

PluginMgr::~PluginMgr() {
  SystemLog(TEXT("%s"), TEXT("�v���O�C���}�l�[�W�������"));

  // ���[�h�Ƃ͋t���ɉ������
  FreeAll();

  // �V�X�e���v���O�C�������
  plugins.clear();

  SystemLog(TEXT("  %s"), TEXT("OK"));
}

//---------------------------------------------------------------------------//

void PluginMgr::LoadAll() {
  SystemLog(TEXT("%s"), TEXT("���ׂẴv���O�C����ǂݍ���"));

  // �C���X�g�[���t�H���_�̃p�X���擾
  std::array<TCHAR, MAX_PATH> dir_path;
  ::GetModuleFileName(g_hInst, dir_path.data(), (DWORD)dir_path.size());
  ::PathRemoveFileSpec(dir_path.data());

  // �C���X�g�[���t�H���_�ȉ��� DLL �t�@�C�������W
  CollectFile(dir_path.data(), TEXT(".dll"));

  // ���ׂẴv���O�C���̏����擾
  InitInfoAll();

  // ���ׂẴv���O�C����������
  InitAll();

  write_log(ERROR_LEVEL::elInfo, TEXT("%u plugin(s)"), plugins.size() - 1);
  SystemLog(TEXT("  %s"), TEXT("OK"));
}

//---------------------------------------------------------------------------//

void PluginMgr::FreeAll() {
  SystemLog(TEXT("%s"), TEXT("���ׂẴv���O�C�������"));

  // ���[�h�Ƃ͋t���ɉ�� ... �V�X�e���v���O�C���͉�����Ȃ�
  while (plugins.size() > 1)
  {
    plugins.pop_back();
  }

  SystemLog(TEXT("  %s"), TEXT("OK"));
}

//---------------------------------------------------------------------------//

const ITTBPlugin* PluginMgr::Find
(
  LPCTSTR PluginFilename
)
const noexcept
{
  for (auto&& plugin : plugins)
  {
    if (0 == lstrcmp(PluginFilename, plugin->info()->Filename))
    {
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
    // �w�肳�ꂽ�t�H���_��������Ȃ�����
    return;
  }

  // �t�H���_���ɂ�����̂�񋓂���
  do {
    // �B���t�@�C���͔�΂�
    if (fd.cFileName[0] == '.') {
      continue;
    }
    if (fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) {
      continue;
    }

    // Hook.dll�͔�΂�
    if (0 == lstrcmp(TEXT("Hook.dll"), fd.cFileName)) {
      continue;
    }

    // �t���p�X������
    _sntprintf(path, MAX_PATH, TEXT(R"(%s\%s)"), dir_path, fd.cFileName);

    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      // �T�u�t�H���_������
      CollectFile(path, ext);
    }
    else {
      // �g���q���擾
      TCHAR* s;
      for (s = path + lstrlen(path) - 1; *s != '.'; --s);

      // �ړI�̊g���q��������
      if (0 == lstrcmp(s, ext))
      {
        // �R���e�i�Ɏ��^
        ITTBPlugin* plugin;

        plugin = new TTBasePlugin(path); // �ʏ�̃v���O�C��
        if (plugin->is_loaded())
        {
          plugins.emplace_back(plugin);
          continue;
        }
        else
        {
          if (plugin) { delete plugin; }
        }

#if INTPTR_MAX == INT64_MAX
        plugin = new TTBBridgePlugin(path); // 32�r�b�g�̃v���O�C��
        if (plugin->is_loaded())
        {
          plugins.emplace_back(plugin);
        }
        else
        {
          if (plugin) { delete plugin; }
        }
#endif
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

  // �{�̂̐�΃p�X���擾
  ::GetModuleFileName(g_hInst, exe_path.data(), (DWORD)exe_path.size());

  // �v���O�C�����̎擾
  // * �V�X�e���v���O�C���͊��ɏ������ς݂̂��߁A���X�g��2�Ԗڂ���J�n
  auto it = ++plugins.begin();
  while (it != plugins.end()) {
    auto&& plugin = *it;
    ::PathRelativePathTo
    (
      relative_path.data(),
      exe_path.data(), FILE_ATTRIBUTE_ARCHIVE,
      plugin->path(), FILE_ATTRIBUTE_ARCHIVE
    );

    // relative_path �� �擪2���� (".\") �͗v��Ȃ��̂� ���炷
    const auto PluginFilename = relative_path.data() + 2;

    const auto result = plugin->InitInfo(PluginFilename);
    if (!result) {
      it = plugins.erase(it);
      continue;
    }

    // �풓�^�łȂ���Ή��
    if (plugin->info()->PluginType != ptAlwaysLoad)
    {
      plugin->Free();
    }

    ++it;
  }
}

//---------------------------------------------------------------------------//

void PluginMgr::InitAll()
{
  std::array<TCHAR, MAX_PATH> exe_path;
  std::array<TCHAR, MAX_PATH> relative_path;

  // �{�̂̐�΃p�X���擾
  ::GetModuleFileName(g_hInst, exe_path.data(), (DWORD)exe_path.size());

  // �v���O�C���̏�����
  // * �V�X�e���v���O�C���͊��ɏ������ς݂̂��߁A���X�g��2�Ԗڂ���J�n
  auto it = ++plugins.begin();
  const auto end = plugins.end();
  while (it != end)
  {
    auto&& plugin = *it;
    if (plugin->is_loaded())
    {
      ::PathRelativePathTo
      (
        relative_path.data(),
        exe_path.data(), FILE_ATTRIBUTE_ARCHIVE,
        plugin->path(), FILE_ATTRIBUTE_ARCHIVE
      );

      // relative_path �� �擪2���� (".\") �͗v��Ȃ��̂� ���炷
      const auto PluginFilename = relative_path.data() + 2;

      plugin->Init(PluginFilename, (DWORD_PTR)plugin.get());
    }

    ++it;
  }
}