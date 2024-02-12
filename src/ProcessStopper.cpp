/*
+-----------------------+
|  by AmeliePick. 2024  |
| github.com/AmeliePick |
+-----------------------+
*/


#include <Windows.h>
#pragma comment(lib, "Synchronization.lib")

typedef LONG(NTAPI* NtProcess)(HANDLE ProcessHandle);

NtProcess NtSuspendProcess = (NtProcess)GetProcAddress(GetModuleHandle(L"ntdll"), "NtSuspendProcess");
NtProcess NtResumeProcess  = (NtProcess)GetProcAddress(GetModuleHandle(L"ntdll"), "NtResumeProcess");

static HANDLE suspendedProcess = NULL;
static bool runThread = false;


void CALLBACK WorkCallback(PTP_CALLBACK_INSTANCE Instance, PVOID Parameter, PTP_WORK Work)
{
    if (suspendedProcess != NULL)
    {
        NtResumeProcess(suspendedProcess);
        CloseHandle(suspendedProcess);
        suspendedProcess = NULL;

        return;
    }

    bool cmp = runThread;
    WaitOnAddress(&runThread, &cmp, sizeof(bool), 2000); // 2 sec delay for key pressing.
    if (runThread == false) return; // The key was released.

    DWORD processID = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &processID);

    suspendedProcess = OpenProcess(THREAD_ALL_ACCESS, TRUE, processID);
    NtSuspendProcess(suspendedProcess);
}


LRESULT keyboardProcessing(int code, WPARAM w, LPARAM l)
{
    if (((PKBDLLHOOKSTRUCT)l)->vkCode != VK_ESCAPE) return CallNextHookEx(NULL, code, w, l);

    if (w == WM_KEYUP && runThread)
    {
        runThread = false;
        WakeByAddressAll(&runThread);
    }
    else if (!runThread) // Don't create the thread while the key is pressed.
    {
        runThread = true;
        SubmitThreadpoolWork(CreateThreadpoolWork((PTP_WORK_CALLBACK)WorkCallback, NULL, NULL));
    }

    return CallNextHookEx(NULL, code, w, l);
}

int main()
{
    ShowWindow(GetConsoleWindow(), SW_HIDE);

    PTP_POOL threadPool = CreateThreadpool(NULL);
    HHOOK kHook = SetWindowsHookEx(WH_KEYBOARD_LL, (HOOKPROC)&keyboardProcessing, GetModuleHandle(NULL), 0);

    int retVal;
    MSG msg;
    while ((retVal = GetMessage(&msg, NULL, 0, 0)) != 0)
    {
        if (retVal == -1) break;

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    };

    UnhookWindowsHookEx(kHook);
    CloseThreadpool(threadPool);
}
