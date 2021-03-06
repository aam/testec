// testec.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>

#include <windows.h>
#include <process.h>

int depth;

void DisplayError(char *pszAPI)
{
	LPVOID lpvMessageBuffer;
	TCHAR szPrintBuffer[512];
	DWORD nCharsWritten;

	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, GetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpvMessageBuffer, 0, NULL);

  const size_t size = 1024;
  WCHAR wszAPI[size];
  mbstowcs(wszAPI, pszAPI, strlen(pszAPI));

  _stprintf(szPrintBuffer,
		_T("ERROR: API    = %s.\n   error code = %d.\n   message    = %s.\n"),
		wszAPI, GetLastError(), (char *)lpvMessageBuffer);

  WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE),szPrintBuffer,
		lstrlen(szPrintBuffer),&nCharsWritten,NULL);

	LocalFree(lpvMessageBuffer);
	ExitProcess(GetLastError());
}

void Display(char* fmt,...) {
  const size_t size = 1024;
  char buf[size];
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(buf, size, fmt, ap);
  va_end(ap);

	DWORD nCharsWritten;
  char fullbuf[size];
  sprintf(fullbuf, "%*s", depth, "");
  strcat(fullbuf, buf);
	FILE* f = fopen("c:\\dart_bleeding\\trace.log", "a+");
  fprintf(f, "%s", fullbuf);
  fclose(f);
  va_end(ap);
//  WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), fullbuf, strlen(fullbuf), &nCharsWritten, NULL);
  return;

  WCHAR wfullbuf[size];
  _swprintf(wfullbuf, _T("%*s"), depth, _T(""));

  WCHAR wbuf[size];
  mbstowcs(wbuf, buf, strlen(buf));
  wbuf[strlen(buf)] = L'\0';
  wcscat(wfullbuf, wbuf);

  //WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), wfullbuf, wcslen(wfullbuf), &nCharsWritten, NULL);
  //wprintf(wfullbuf);
	//WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), wfullbuf, wcslen(wfullbuf),&nCharsWritten,NULL);
}


struct PairOfHandles {
  HANDLE hFrom;
  HANDLE hTo;
  HANDLE hToClose;
  
  PairOfHandles(HANDLE _hFrom, HANDLE _hTo, HANDLE _hToClose) : hFrom(_hFrom), hTo(_hTo), hToClose(_hToClose) {}
};

VOID CALLBACK redirectCallback(PTP_CALLBACK_INSTANCE Instance, PVOID Parameter, PTP_WORK Work)
{
  PairOfHandles* ppoh = (PairOfHandles*)Parameter;
  CHAR lpBuffer[256];
  DWORD nBytesRead;
  DWORD nBytesWritten;
  Display("%d(%d) redirectCallback %d -> %d, planning on closing %d\n", GetCurrentProcessId(), GetCurrentThreadId(), ppoh->hFrom, ppoh->hTo, ppoh->hToClose);

  while(TRUE)
  {
    if (!ReadFile(ppoh->hFrom, lpBuffer, sizeof(lpBuffer), &nBytesRead, NULL) || !nBytesRead)
    {
      DWORD dwErr = GetLastError();
      if (dwErr == ERROR_BROKEN_PIPE || dwErr == ERROR_INVALID_HANDLE)
        break; // pipe done - normal exit path.
      else
        DisplayError("ReadFile"); // Something bad happened.
    }
    printf("%d(%d)\t\tread %d bytes from %d\n", GetCurrentProcessId(), GetCurrentThreadId(), nBytesRead, ppoh->hFrom);
    if (!WriteFile(ppoh->hTo, lpBuffer, nBytesRead, &nBytesWritten, NULL))
    {
      if (GetLastError() == ERROR_BROKEN_PIPE)
        break; // pipe done - normal exit path.
      else
        DisplayError("WriteFile"); // Something bad happened.
    }
    printf("%d(%d)\t\twrote %d bytes to %d\n", GetCurrentProcessId(), GetCurrentThreadId(), nBytesWritten, ppoh->hTo);
  }
  if (ppoh->hToClose != INVALID_HANDLE_VALUE) {
    printf("%d(%d)\t\tend of redirect - closing %d\n", GetCurrentProcessId(), GetCurrentThreadId(), ppoh->hToClose);
    CloseHandle(ppoh->hToClose);
  }
  delete ppoh;
}

struct WaiterParams {
  PROCESS_INFORMATION process_info;
  HANDLE event;
  HANDLE hChildReadInput;
  int* pRC;

  WaiterParams(PROCESS_INFORMATION pi, HANDLE e, HANDLE _hChildReadInput, int* _pRC): process_info(pi), event(e), hChildReadInput(_hChildReadInput), pRC(_pRC) {}
};

VOID CALLBACK callback(PTP_CALLBACK_INSTANCE instance, PVOID context, PTP_WAIT wait, TP_WAIT_RESULT waitResult) {
  WaiterParams* wp = (WaiterParams*)context;
  Display("%d(%d) callback: closing %d\n", GetCurrentProcessId(), GetCurrentThreadId(), wp->hChildReadInput);
  CloseHandle(wp->hChildReadInput);
  SetEvent(wp->event);
  delete wp;
}

int _tmain1(int argc, _TCHAR* argv[])
{
  if (depth == 0) {
    Display("%d(%d)************\n", GetCurrentProcessId(), GetCurrentThreadId());
    //printf("***********************************\n%d(%d) Closing stdin=%d stdout=%d stderr=%d\n", GetCurrentProcessId(), GetCurrentThreadId(), GetStdHandle(STD_INPUT_HANDLE), GetStdHandle(STD_OUTPUT_HANDLE), GetStdHandle(STD_ERROR_HANDLE));
    //if (!CloseHandle(GetStdHandle(STD_INPUT_HANDLE))) { DisplayError("CloseHandle"); }
    //if (!CloseHandle(GetStdHandle(STD_OUTPUT_HANDLE))) { DisplayError("CloseHandle"); }
    //if (!CloseHandle(GetStdHandle(STD_ERROR_HANDLE))) { DisplayError("CloseHandle"); }
    //printf("***********************************\n%d(%d) Closed stdin=%d stdout=%d stderr=%d\n", GetCurrentProcessId(), GetCurrentThreadId(), GetStdHandle(STD_INPUT_HANDLE), GetStdHandle(STD_OUTPUT_HANDLE), GetStdHandle(STD_ERROR_HANDLE));
    return 1;
  }

  int multiply = 1;
  if (argc > 2) {
    multiply = _ttoi(argv[2]);
    if (multiply < 1) {
      Display("multiply should be greater than 1\n");
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
    HANDLE hOutputRead;
    HANDLE hErrorRead;
    HANDLE hInputWrite;

    int rc;
    ProcSetup(PROCESS_INFORMATION _pi, HANDLE _hEvent, HANDLE _hWait,
        HANDLE _hOutputRead, HANDLE _hErrorRead, HANDLE _hInputWrite)
        : pi(_pi), hEvent(_hEvent), hWait(_hWait), hOutputRead(_hOutputRead), hErrorRead(_hErrorRead), hInputWrite(_hInputWrite) {}
  };

  ProcSetup* pss = (ProcSetup*)calloc(multiply, sizeof(ProcSetup));

  for (int i = 0; i < multiply; i++) {
    Display("%d(%d) Multiply %d out of %d\n", GetCurrentProcessId(), GetCurrentThreadId(), i, multiply);
	  HANDLE hOutputRead, hOutputWrite;
	  HANDLE hErrorRead, hErrorWrite;
	  HANDLE hInputRead, hInputWrite;
	  SECURITY_ATTRIBUTES sa;

	  // Set up the security attributes struct.
	  sa.nLength= sizeof(SECURITY_ATTRIBUTES);
	  sa.lpSecurityDescriptor = NULL;
	  sa.bInheritHandle = TRUE;

	  if (!CreatePipe(&hOutputRead, &hOutputWrite, &sa, 0)) DisplayError("CreatePipe");
    Display("%d(%d) Created output pipe: r:%d* <-> w:%d\n", GetCurrentProcessId(), GetCurrentThreadId(), hOutputRead, hOutputWrite);
    if (!SetHandleInformation(hOutputRead, HANDLE_FLAG_INHERIT, 0)) DisplayError("SetHandleInformation");

    if (!CreatePipe(&hErrorRead, &hErrorWrite, &sa, 0)) DisplayError("CreatePipe");
    Display("%d(%d) Created error pipe: r:%d* <-> w:%d\n", GetCurrentProcessId(), GetCurrentThreadId(), hErrorRead, hErrorWrite);
    if (!SetHandleInformation(hErrorRead, HANDLE_FLAG_INHERIT, 0)) DisplayError("SetHandleInformation");

	  if (!CreatePipe(&hInputRead, &hInputWrite, &sa, 0)) DisplayError("CreatePipe");
    Display("%d(%d) Created input pipe: r:%d <-> w*:%d\n", GetCurrentProcessId(), GetCurrentThreadId(), hInputRead, hInputWrite);
    if (!SetHandleInformation(hInputWrite, HANDLE_FLAG_INHERIT, 0)) DisplayError("SetHandleInformation");

    HANDLE hChildStdInput;

  	STARTUPINFO startup_info;
    ZeroMemory(&startup_info, sizeof(startup_info));
    startup_info.cb = sizeof(startup_info);
    //startup_info.hStdInput = GetStdHandle(STD_INPUT_HANDLE); //stdin_handles[kReadHandle];
    DuplicateHandle(GetCurrentProcess(), GetStdHandle(STD_INPUT_HANDLE), GetCurrentProcess(), &startup_info.hStdInput, NULL, TRUE, DUPLICATE_SAME_ACCESS);
    DuplicateHandle(GetCurrentProcess(), GetStdHandle(STD_INPUT_HANDLE), GetCurrentProcess(), &hChildStdInput, NULL, FALSE, DUPLICATE_SAME_ACCESS);
    //startup_info.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE); //stdout_handles[kWriteHandle];
    DuplicateHandle(GetCurrentProcess(), GetStdHandle(STD_OUTPUT_HANDLE), GetCurrentProcess(), &startup_info.hStdOutput, NULL, TRUE, DUPLICATE_SAME_ACCESS);
    //startup_info.hStdError = GetStdHandle(STD_ERROR_HANDLE); //stderr_handles[kWriteHandle];
    DuplicateHandle(GetCurrentProcess(), GetStdHandle(STD_ERROR_HANDLE), GetCurrentProcess(), &startup_info.hStdError, NULL, TRUE, DUPLICATE_SAME_ACCESS);
    Display("%d(%d) Duped stdinput: %d -> %d, stdoutput: %d -> %d, stderr: %d -> %d\n", GetCurrentProcessId(), GetCurrentThreadId(), GetStdHandle(STD_INPUT_HANDLE), startup_info.hStdInput, GetStdHandle(STD_OUTPUT_HANDLE), startup_info.hStdOutput, GetStdHandle(STD_ERROR_HANDLE), startup_info.hStdError);
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
    pss[i].hOutputRead = hOutputRead;
    pss[i].hErrorRead = hErrorRead;
    pss[i].hInputWrite = hInputWrite;
    
    PTP_WAIT myWait = CreateThreadpoolWait(callback, new WaiterParams(process_info, pss[i].hEvent, hChildStdInput, &pss[i].rc), &CallBackEnviron);
    SetThreadpoolWait(myWait, process_info.hProcess, NULL);

    //Display("%d(%d) Ready to submit threadpool work\n", GetCurrentProcessId(), GetCurrentThreadId());
    //SubmitThreadpoolWork(CreateThreadpoolWork(redirectCallback, new PairOfHandles(hChildStdInput, hInputWrite, hInputWrite), &CallBackEnviron));
    //Display("%d(%d) Submit threadpool work 1\n", GetCurrentProcessId(), GetCurrentThreadId());
    //SubmitThreadpoolWork(CreateThreadpoolWork(redirectCallback, new PairOfHandles(hErrorRead, GetStdHandle(STD_ERROR_HANDLE), hErrorRead), &CallBackEnviron));
    //Display("%d(%d) Submit threadpool work 2\n", GetCurrentProcessId(), GetCurrentThreadId());
    //SubmitThreadpoolWork(CreateThreadpoolWork(redirectCallback, new PairOfHandles(hOutputRead, GetStdHandle(STD_OUTPUT_HANDLE), hOutputRead), &CallBackEnviron));
    //Display("%d(%d) Submit threadpool work 3\n", GetCurrentProcessId(), GetCurrentThreadId());

    Display("%d(%d) Closing dups: stdin=%d stdout=%d stderr=%d\n", GetCurrentProcessId(), GetCurrentThreadId(), startup_info.hStdInput, startup_info.hStdOutput, startup_info.hStdError);
    if (!CloseHandle(startup_info.hStdInput)) DisplayError("CloseHandle");
    if (!CloseHandle(startup_info.hStdOutput)) DisplayError("CloseHandle");
    if (!CloseHandle(startup_info.hStdError)) DisplayError("CloseHandle");

    Display("%d(%d) Closing wrong ends: stdin=%d stdout=%d stderr=%d\n", GetCurrentProcessId(), GetCurrentThreadId(), hInputRead, hOutputWrite, hErrorWrite);
    if (!CloseHandle(hInputRead)) DisplayError("CloseHandle");
    if (!CloseHandle(hOutputWrite)) DisplayError("CloseHandle");
    if (!CloseHandle(hErrorWrite)) DisplayError("CloseHandle");

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
    //  Display("callback - WaitForMultipleObjects failed. RC=%x. GetLastError=%x\n", rc, GetLastError());
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

    //if (!CloseHandle(pss[i].hOutputRead)) DisplayError("CloseHandle");
    //if (!CloseHandle(pss[i].hInputWrite)) DisplayError("CloseHandle");

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


int _tmain(int argc, _TCHAR* argv[]) {
  LoadLibrary(_T("testecdll.dll"));
 if (argc < 2) {
    Display("testec <depth> [<multiply>]\n");
	  return -1;
  }
  depth = _ttoi(argv[1]);
//  Display("%2d", depth);
  SetConsoleOutputCP(CP_OEMCP);
  Display("%d(%d) Depth: %d Starting stdin=%d stdout=%d stderr=%d\n", GetCurrentProcessId(), GetCurrentThreadId(), depth, GetStdHandle(STD_INPUT_HANDLE), GetStdHandle(STD_OUTPUT_HANDLE), GetStdHandle(STD_ERROR_HANDLE));
  int rc = -1;
  try {
  //__try {
  //  __try {
      rc = _tmain1(argc, argv);
  //  } __except(EXCEPTION_EXECUTE_HANDLER)
  //  {
  //    Display("EXCEPTION: %d\n", GetExceptionCode());
  //    DebugBreak();
  //  }
  //} __finally {
  //  if (rc != 1) {
  //    DebugBreak();
  //  }
  //}
  } catch(...) {
    DebugBreak();
  }
  Display("%d(%d) Exiting and closing stdin=%d stdout=%d stderr=%d\n", GetCurrentProcessId(), GetCurrentThreadId(), GetStdHandle(STD_INPUT_HANDLE), GetStdHandle(STD_OUTPUT_HANDLE), GetStdHandle(STD_ERROR_HANDLE));
  if (!CloseHandle(GetStdHandle(STD_INPUT_HANDLE))) { DisplayError("CloseHandle"); }
  if (!CloseHandle(GetStdHandle(STD_OUTPUT_HANDLE))) { DisplayError("CloseHandle"); }
  if (!CloseHandle(GetStdHandle(STD_ERROR_HANDLE))) { DisplayError("CloseHandle"); }
  return rc;
}