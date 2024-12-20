/*
+-----------------------+
|  by AmeliePick. 2024  |
| github.com/AmeliePick |
+-----------------------+
*/


#include <Windows.h>
#pragma comment(lib, "Synchronization.lib")
#pragma comment(lib, "Winmm.lib")

typedef LONG(NTAPI* NtProcess)(HANDLE ProcessHandle);

static NtProcess NtSuspendProcess = (NtProcess)GetProcAddress(GetModuleHandle(L"ntdll"), "NtSuspendProcess");
static NtProcess NtResumeProcess  = (NtProcess)GetProcAddress(GetModuleHandle(L"ntdll"), "NtResumeProcess");

static HANDLE suspendedProcess = NULL;
static bool   isKeyPressed     = false;
static MMRESULT _timer         = NULL;


VOID CALLBACK WorkCallback(UINT uTimerID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2)
{
    if (!isKeyPressed) return;

    DWORD processID = 0;
    GetWindowThreadProcessId((HWND)dwUser, &processID);
    NtSuspendProcess(suspendedProcess = OpenProcess(THREAD_ALL_ACCESS | PROCESS_SUSPEND_RESUME, TRUE, processID)); 
}


LRESULT keyboardProcessing(int code, WPARAM w, LPARAM l)
{
    if (((PKBDLLHOOKSTRUCT)l)->vkCode != VK_ESCAPE) return CallNextHookEx(NULL, code, w, l);

    if (w == WM_KEYUP)
    {
        isKeyPressed = false;
        if (_timer != NULL) // If the user simulates input and sending UP event without DOWN before.
        {
            timeKillEvent(_timer);
            _timer = NULL;
        }
    }
    else if (!isKeyPressed)
    {
        isKeyPressed = true;

        if (suspendedProcess != NULL)
        {
            NtResumeProcess(suspendedProcess);
            CloseHandle(suspendedProcess);
            suspendedProcess = NULL;
            return CallNextHookEx(NULL, code, w, l);
        }
        _timer = timeSetEvent(2000, 0, WorkCallback, (DWORD_PTR)GetForegroundWindow(), TIME_CALLBACK_FUNCTION | TIME_KILL_SYNCHRONOUS | TIME_ONESHOT);
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
