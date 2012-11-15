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
  __try {
    WaiterParams* wp = (WaiterParams*)context;
    PROCESS_INFORMATION *process_info = &wp->process_info;
    DWORD texit_code;
    BOOL ok = GetExitCodeThread(process_info->hThread,
                            reinterpret_cast<DWORD*>(&texit_code));
    if (!ok) {
      printf("*** GetExitCodeThread failed %d\n", GetLastError());
    }
    DWORD exit_code;
    ok = GetExitCodeProcess(process_info->hProcess,
                            reinterpret_cast<DWORD*>(&exit_code));
    if (!ok) {
      printf("*** GetExitCodeProcess failed %d\n", GetLastError());
    }
    *wp->pRC = exit_code;
    if (texit_code != exit_code) {
      printf("*** cb *** Thread %d exit code %x didn't match process %d exit code %x\n", process_info->dwThreadId, texit_code, process_info->dwProcessId, exit_code);
    }
    SetEvent(wp->event);
    delete wp;
  } __except(true? EXCEPTION_EXECUTE_HANDLER: EXCEPTION_CONTINUE_SEARCH) { //GetExceptionCode() == 0xc000004b
    printf("callback caught exception: %x\n", GetExceptionCode());
    DebugBreak();
  }
}

int _tmain1(int argc, _TCHAR* argv[])
{
  LoadLibrary(_T("testecdll.dll"));
  int depth;
  if (argc < 2) {
    printf("testec <depth> [<multiply>]\n");
	  return -1;
  }
  depth = _ttoi(argv[1]);
  printf("%2d", depth);
  if (depth == 0) { printf("."); return 1; }

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
  BOOL bRet = SetThreadpoolThreadMinimum(pool, 1);
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

  HANDLE* procs = (HANDLE*)calloc(multiply, sizeof(HANDLE));

  for (int i = 0; i < multiply; i++) {
    STARTUPINFO startup_info;
    ZeroMemory(&startup_info, sizeof(startup_info));
    startup_info.cb = sizeof(startup_info);
    //HANDLE hStdin;
    //HANDLE hStdout;
    //HANDLE hStderr;
    //if (!DuplicateHandle(GetCurrentProcess(), GetStdHandle(STD_INPUT_HANDLE), GetCurrentProcess(), &hStdin, NULL, TRUE, DUPLICATE_SAME_ACCESS)) { _tprintf(_T("Failed to duplicate handle: %d\n"), GetLastError()); }
    //if (!DuplicateHandle(GetCurrentProcess(), GetStdHandle(STD_ERROR_HANDLE), GetCurrentProcess(), &hStderr, NULL, TRUE, DUPLICATE_SAME_ACCESS)) { _tprintf(_T("Failed to duplicate handle: %d\n"), GetLastError()); }
    //if (!DuplicateHandle(GetCurrentProcess(), GetStdHandle(STD_OUTPUT_HANDLE), GetCurrentProcess(), &hStdout, NULL, TRUE, DUPLICATE_SAME_ACCESS)) { _tprintf(_T("Failed to duplicate handle: %d\n"), GetLastError()); }
    startup_info.hStdInput = NULL;//GetStdHandle(STD_INPUT_HANDLE); //stdin_handles[kReadHandle];
    startup_info.hStdOutput = NULL; //hStdout; //GetStdHandle(STD_OUTPUT_HANDLE); //stdout_handles[kWriteHandle];
    startup_info.hStdError = NULL; //hStderr; //GetStdHandle(STD_ERROR_HANDLE); //stderr_handles[kWriteHandle];
    //startup_info.dwFlags = STARTF_USESTDHANDLES;

    PROCESS_INFORMATION process_info;
    ZeroMemory(&process_info, sizeof(process_info));

    TCHAR cmd[1024];
    _stprintf(cmd, _T("%s %d"), argv[0], depth-1);
    BOOL result = CreateProcess(NULL,   // ApplicationName
                                cmd,
                                NULL,   // ProcessAttributes
                                NULL,   // ThreadAttributes
                                TRUE,   // InheritHandles
                                CREATE_SUSPENDED | DETACHED_PROCESS, // | CREATE_BREAKAWAY_FROM_JOB,      // CreationFlags
                                NULL,
                                NULL,
                                &startup_info,
                                &process_info);
    //CloseHandle(stdin);
    //CloseHandle(stdout);
    //CloseHandle(stderr);
    pss[i].hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    pss[i].pi = process_info;
    procs[i] = process_info.hProcess;

    //PTP_WAIT myWait = CreateThreadpoolWait(callback, new WaiterParams(process_info, pss[i].hEvent, &pss[i].rc), &CallBackEnviron);
    //SetThreadpoolWait(myWait, process_info.hProcess, NULL);

    ResumeThread(process_info.hThread);
  }
  DWORD wres = WaitForMultipleObjects(multiply, procs, true, INFINITE);
  if (wres != WAIT_OBJECT_0) {
    _tprintf(_T("WaitForMultipleObjects failed. LastError: %u\n"), GetLastError());
  }

  //HANDLE* events = (HANDLE*)calloc(multiply, sizeof(HANDLE));
  //for (int i = 0; i < multiply; i++) {
  //  events[i] = pss[i].hEvent;
  //}
  //DWORD wrese = WaitForMultipleObjects(multiply, events, true, INFINITE);
  //if (wrese != WAIT_OBJECT_0) {
  //  _tprintf(_T("WaitForMultipleObjects failed. LastError: %u\n"), GetLastError());
  //}

  int texit_code;
  int exit_code;
  for (int i = 0; i < multiply; i++) {
    PROCESS_INFORMATION* process_info = &pss[i].pi;

    //DWORD rc = WaitForSingleObject(process_info->hThread, INFINITE);
    //if (rc != WAIT_OBJECT_0) {
    //  printf("callback - WaitForMultipleObjects failed. RC=%x. GetLastError=%x\n", rc, GetLastError());
    //}

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
  //for (int i = 0; i < multiply; i++) {
  //  CloseHandle(pss[i].hEvent);
  //}
  //free(events);
  free(procs);

  CloseThreadpoolCleanupGroupMembers(cleanupgroup, FALSE, NULL);
  CloseThreadpoolCleanupGroup(cleanupgroup);
  CloseThreadpool(pool);
  DestroyThreadpoolEnvironment(&CallBackEnviron);

//  _cexit();
//  ExitThread(texit_code);
  return exit_code;
}


int _tmain(int argc, _TCHAR* argv[])
{
  int rc;
  __try {
    rc = _tmain1(argc, argv);
  } __except(true? EXCEPTION_EXECUTE_HANDLER: EXCEPTION_CONTINUE_SEARCH) { //GetExceptionCode() == 0xc000004b
    printf("tmain caught exception: %x\n", GetExceptionCode());
    DebugBreak();
  }
  return rc;
}

