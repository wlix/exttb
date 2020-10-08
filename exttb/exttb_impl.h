#pragma once

#include <array>

#include "framework.h"

#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib") // PathRenameExtension, PathRemoveFileSpec

#include "include/File.hpp"

extern HINSTANCE g_hInst;

// �R�}���hID
enum CMD : INT32 {
  CMD_EXIT,
  CMD_SETTINGS,
  CMD_SHOW_VER_INFO,
  CMD_RELOAD,
  CMD_OPEN_FOLDER,
  CMD_TRAYICON,
  CMD_COUNT
};

class settings {
public:
  static settings& get() { static settings s; return s; }

public:
  bool  TTBaseCompatible;
  bool  ShowTaskTrayIcon;
  DWORD logLevel;
  bool  logToWindow;
  bool  logToFile;

  win_file file;

public:
  void load();
  void save();

private:
  settings() { load(); }
  ~settings() = default;
};

inline void settings::load() {
  std::array<wchar_t, MAX_PATH> path;

  // ini�t�@�C�����擾
  ::GetModuleFileNameW(g_hInst, path.data(), (DWORD)path.size());
  ::PathRenameExtensionW(path.data(), L".ini");

  // �p�����[�^�̎擾
  TTBaseCompatible = ::GetPrivateProfileIntW
  (
    L"Setting", L"TTBaseCompatible", 0, path.data()
  )
    ? true : false;

  ShowTaskTrayIcon = ::GetPrivateProfileIntW
  (
    L"Setting", L"ShowTaskTrayIcon", 1, path.data()
  )
    ? true : false;

  logLevel = ::GetPrivateProfileIntW
  (
    L"Setting", L"logLevel", ERROR_LEVEL(5), path.data()
  );

  logToWindow = ::GetPrivateProfileIntW
  (
    L"Setting", L"logToWindow", 1, path.data()
  )
    ? true : false;

  logToFile = ::GetPrivateProfileIntW
  (
    L"Setting", L"logToFile", 1, path.data()
  )
    ? true : false;

  // ���O�̏o�͐悪�t�@�C���̏ꍇ
  if (logLevel && logToFile)
  {
    // ���O�t�H���_�̃p�X������
    ::PathRemoveFileSpecW(path.data());
    ::StringCchCatW(path.data(), path.size(), LR"(\log)");
    if (!::PathIsDirectoryW(path.data()))
    {
      // �t�H���_���쐬
      ::CreateDirectoryW(path.data(), nullptr);
    }

    // ���O�t�@�C���̃p�X������
    SYSTEMTIME st;
    ::GetLocalTime(&st);

    std::array<wchar_t, MAX_PATH> log;
    ::StringCchPrintfW
    (
      log.data(), log.size(), LR"(%s\%04u%02u%02u.log)",
      path.data(), st.wYear, st.wMonth, st.wDay
    );

    // ���O�t�@�C�����J��
    using namespace tapetums;
    const auto result = file.Open
    (
      log.data(),
      File::ACCESS::WRITE, File::SHARE::WRITE, File::OPEN::OR_CREATE
    );
    if (!result)
    {
      ::MessageBoxW(nullptr, log.data(), L"���O�t�@�C�����쐬�ł��܂���", MB_OK);
      file.Close();
    }
    else
    {
      // �����ɒǋL���Ă���
      file.Seek(0, File::ORIGIN::END);

#if defined(_UNICODE) || defined(UNICODE)
      if (file.position() == 0)
      {
        file.Write(UINT16(0xFEFF)); // UTF-16 BOM ���o��
      }
#endif
    }
  }
}