// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"

#include <stdio.h>


void Display(char* fmt,...) {
  const size_t size = 1024;
  char buf[size];
  va_list ap;
  va_start(ap, fmt);
//  int n = vsnprintf(buf, size, fmt, ap);

	FILE* f = fopen("c:\\dart_bleeding\\trace-thr.log", "a+");
  vfprintf(f, fmt, ap);
  fclose(f);
  va_end(ap);

  
}


BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
    break;
	case DLL_THREAD_ATTACH:
    break;
	case DLL_THREAD_DETACH:
  {
    DWORD dw;
//    Display("\tTHR: %d done\n", GetCurrentThreadId());
    if (!GetExitCodeThread(GetCurrentThread(), &dw)) { printf("Failed to GetExitCodeThread - %d\n", GetLastError()); }
    if (dw != 0x103 && dw != 1) DebugBreak();
    break;
  }
	case DLL_PROCESS_DETACH:
  {
    DWORD dw;
//    Display("PROC: %d done\n", GetCurrentProcessId());
    if (!GetExitCodeProcess(GetCurrentProcess(), &dw)) { printf("Failed to GetExitCodeProcess - %d\n", GetLastError()); }
    if (dw != 0x103 && dw != 1) DebugBreak();
		break;
  }
	}
	return TRUE;
}

