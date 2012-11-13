// testec.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>

#include <windows.h>
#include <process.h>

struct WaiterParams {
  PROCESS_INFORMATION process_info;
  HANDLE event;
  int* pRC;

  WaiterParams(PROCESS_INFORMATION pi, HANDLE e, int* _pRC): process_info(pi), event(e), pRC(_pRC) {}
};

VOID CALLBACK callback(PTP_CALLBACK_INSTANCE instance, PVOID context, PTP_WAIT wait, TP_WAIT_RESULT waitResult) {
    WaiterParams* wp = (WaiterParams*)context;
    SetEvent(wp->event);
    delete wp;
}

int _tmain(int argc, _TCHAR* argv[])
{
  int depth;
  if (argc < 2) {
    printf("testec <depth> [<multiply>]\n");
	  return -1;
  }
  depth = _ttoi(argv[1]);
  printf("%2d", depth);
  if (depth == 0) { return 1; }

  int multiply = 1;
  if (argc > 2) {
    multiply = _ttoi(argv[2]);
    if (multiply < 1) {
      printf("multiply should be greater than 1\n");
      return -2;
    }
  }

  TP_CALLBACK_ENVIRON CallBackEnviron;
  InitializeThreadpoolEnvironment(&CallBackEnviron);
  PTP_POOL pool = CreateThreadpool(NULL);
  if (NULL == pool) {
    _tprintf(_T("CreateThreadpool failed. LastError: %u\n"), GetLastError());
  }
  SetThreadpoolThreadMaximum(pool, 100);
  BOOL bRet = SetThreadpoolThreadMinimum(pool, 20);
  if (!bRet) {
    _tprintf(_T("SetThreadpoolThreadMinimum failed. LastError: %u\n"), GetLastError());
  }
  PTP_CLEANUP_GROUP cleanupgroup = CreateThreadpoolCleanupGroup();
  if (NULL == cleanupgroup) {
    _tprintf(_T("CreateThreadpoolCleanupGroup failed. LastError: %u\n"), GetLastError());
  }

  SetThreadpoolCallbackPool(&CallBackEnviron, pool);
  SetThreadpoolCallbackCleanupGroup(&CallBackEnviron,
    cleanupgroup,
    NULL);
  
  struct ProcSetup {
    PROCESS_INFORMATION pi;
    HANDLE hEvent;
    HANDLE hWait;
    int rc;
    ProcSetup(PROCESS_INFORMATION _pi, HANDLE _hEvent, HANDLE _hWait): pi(_pi), hEvent(_hEvent), hWait(_hWait) {}
  };

  ProcSetup* pss = (ProcSetup*)calloc(multiply, sizeof(ProcSetup));

  for (int i = 0; i < multiply; i++) {
    STARTUPINFO startup_info;
    ZeroMemory(&startup_info, sizeof(startup_info));
    startup_info.cb = sizeof(startup_info);
    startup_info.hStdInput = GetStdHandle(STD_INPUT_HANDLE); //stdin_handles[kReadHandle];
    startup_info.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE); //stdout_handles[kWriteHandle];
    startup_info.hStdError = GetStdHandle(STD_ERROR_HANDLE); //stderr_handles[kWriteHandle];
    startup_info.dwFlags = STARTF_USESTDHANDLES;

    PROCESS_INFORMATION process_info;
    ZeroMemory(&process_info, sizeof(process_info));

    TCHAR cmd[1024];
    _stprintf(cmd, _T("%s %d"), argv[0], depth-1);
    BOOL result = CreateProcess(NULL,   // ApplicationName
                                cmd,
                                NULL,   // ProcessAttributes
                                NULL,   // ThreadAttributes
                                TRUE,   // InheritHandles
                                CREATE_SUSPENDED, // | CREATE_BREAKAWAY_FROM_JOB,      // CreationFlags
                                NULL,
                                NULL,
                                &startup_info,
                                &process_info);
    pss[i].hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    pss[i].pi = process_info;

    PTP_WAIT myWait = CreateThreadpoolWait(callback, new WaiterParams(process_info, pss[i].hEvent, &pss[i].rc), &CallBackEnviron);
    SetThreadpoolWait(myWait, process_info.hProcess, NULL);

    ResumeThread(process_info.hThread);
  }

  HANDLE* events = (HANDLE*)calloc(multiply, sizeof(HANDLE));
  for (int i = 0; i < multiply; i++) {
    events[i] = pss[i].hEvent;
  }
  WaitForMultipleObjects(multiply, events, true, INFINITE);

  int exit_code;
  for (int i = 0; i < multiply; i++) {
    PROCESS_INFORMATION* process_info = &pss[i].pi;

    //DWORD rc = WaitForSingleObject(process_info->hThread, INFINITE);
    //if (rc != WAIT_OBJECT_0) {
    //  printf("callback - WaitForMultipleObjects failed. RC=%x. GetLastError=%x\n", rc, GetLastError());
    //}

    int texit_code;
    BOOL ok = GetExitCodeThread(process_info->hThread,
                            reinterpret_cast<DWORD*>(&texit_code));
    if (!ok) {
      printf("*** GetExitCodeThread failed %d\n", GetLastError());
    }
    ok = GetExitCodeProcess(process_info->hProcess,
                            reinterpret_cast<DWORD*>(&exit_code));
    if (!ok) {
      printf("*** GetExitCodeProcess failed %d\n", GetLastError());
    }
    if (texit_code != exit_code) {
      printf("*** Thread %d exit code %x didn't match process %d exit code %x\n", process_info->dwThreadId, texit_code, process_info->dwProcessId, exit_code);
    }
//    CloseHandle(hJob);
    CloseHandle(process_info->hProcess);
    CloseHandle(process_info->hThread);

//    *wp->pRC = exit_code;
//    exit_code = pss[i].rc;
    if (exit_code != 1) {
      printf("Failed to get correct exit code. Got %d instead\n", exit_code);
    }
  }
  for (int i = 0; i < multiply; i++) {
    CloseHandle(pss[i].hEvent);
  }
  free(events);

  CloseThreadpoolCleanupGroupMembers(cleanupgroup, FALSE, NULL);
  CloseThreadpoolCleanupGroup(cleanupgroup);
  CloseThreadpool(pool);
  DestroyThreadpoolEnvironment(&CallBackEnviron);

  return exit_code;
}

