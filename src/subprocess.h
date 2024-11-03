#ifndef SUBPROCESS_H_
#define SUBPROCESS_H_

#ifndef SUBPROCESS_NO_STDIO
#include <stdio.h>
#define PRINTF(...) fprintf(stderr, __VA_ARGS__)
#define EPRINTF(...) fprintf(stdout, __VA_ARGS__)
#endif

#ifdef _WIN32
#include <windows.h>
typedef struct HANDLE Subprocess;
#define INVALID_SUBPROCESS INVALID_HANDLE_VALUE
#else
typedef struct int Subprocess
#define INVALID_SUBPROCESS (-1)
#endif

static Subprocess *subprocess_start(const char *command)
{
#ifdef _WIN32
    STARTUPINFO siStartInfo;
    ZeroMemory(&siStartInfo, sizeof(siStartInfo));
    siStartInfo.cb = sizeof(STARTUPINFO);
    siStartInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    siStartInfo.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    siStartInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;
    PROCESS_INFORMATION piProcInfo;
    ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
    BOOL bSuccess = CreateProcessA(NULL, (LPSTR)command, NULL, NULL, TRUE, 0, NULL, NULL, &siStartInfo, &piProcInfo);
    if(!bSuccess) {
        return INVALID_SUBPROCESS;
    }
    CloseHandle(piProcInfo.hThread);
    return piProcInfo.hProcess;
#endif

    return INVALID_SUBPROCESS;
}

static int subprocess_wait(Subprocess *subprocess, int *exit_code)
{
#ifdef _WIN32
    DWORD result = WaitForSingleObject(
                       subprocess,    // HANDLE hHandle,
                       INFINITE // DWORD  dwMilliseconds
                   );

    if (result == WAIT_FAILED) {
        EPRINTF("Failed to wait for subprocess\n");
        return -1;
    }

    DWORD exit_status;
    if (!GetExitCodeProcess(subprocess, &exit_status)) {
        EPRINTF("Failed to get the exit code of subprocess\n");
        return -1;
    }

    if(exit_code) *exit_code = exit_status;


    CloseHandle(subprocess);
    return 0;
#endif
}

#endif // SUBPROCESS_H_
