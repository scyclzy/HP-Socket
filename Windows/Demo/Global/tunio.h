#pragma once

#include <Windows.h>

#include "tap-windows.h"

HANDLE OpenTun(LPCSTR szDev);
void CloseTun(HANDLE hTun);
DWORD ReadTun(HANDLE hTun, LPVOID lpBuffer, DWORD nLen);
DWORD WriteTun(HANDLE hTun, LPCSTR lpBuffer, DWORD nLen);