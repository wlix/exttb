#include "framework.h"

#include "include/File.hpp"

#include "../utility.hpp"

#include "exttb.h"


// global variables
HINSTANCE g_hInst;
HWND      g_hWnd;
HANDLE    g_hMap;

const auto SHARED_MEMORY_NAME = L"ExTTBSharedMemory";

int APIENTRY _tWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPTSTR, _In_ int) {
  g_hInst = hInstance;
  
  // Duplicate Check
  win_file shared;
  if (shared.open(SHARED_MEMORY_NAME, ACCESS::READ)) {
    HWND hwnd;
    // Activate already 
    shared.read(&hwnd, sizeof(HWND));
    ::PostMessage(hwnd, WM_SHOWWINDOW, SW_SHOWNORMAL, 0);
    return 0;
  }

  TTBPlugin_WriteLog(0, ERROR_LEVEL::elInfo, TEXT("起動しました"));

  if (!shared.map(sizeof(HWND), SHARED_MEMORY_NAME, ACCESS::WRITE)) {

  }
}
