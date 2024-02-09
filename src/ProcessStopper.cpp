/*
+-----------------------+
|  by AmeliePick. 2024  |
| github.com/AmeliePick |
+-----------------------+
*/


#include <Windows.h>

typedef LONG(NTAPI* NtProcess)(HANDLE ProcessHandle);

NtProcess NtSuspendProcess = (NtProcess)GetProcAddress(GetModuleHandle(L"ntdll"), "NtSuspendProcess");
NtProcess NtResumeProcess  = (NtProcess)GetProcAddress(GetModuleHandle(L"ntdll"), "NtResumeProcess");

static HANDLE suspendedProcess = NULL;

static int keyState = 0;

static PTP_TIMER timer = NULL;
static HANDLE tick = CreateEvent(NULL, true, FALSE, NULL);
static bool isTimerRestarted = false;

VOID CALLBACK TimerTickCallback(PTP_CALLBACK_INSTANCE Instance, PVOID unusage, PTP_TIMER timer)
{
    ResetEvent(tick);

    if (!keyState) return;

    DWORD processID = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &processID);

    suspendedProcess = OpenProcess(THREAD_ALL_ACCESS, TRUE, processID);

    NtSuspendProcess(suspendedProcess);
}


void RestartTimer()
{
    isTimerRestarted = true;
    if (timer != NULL)
    {
        ResetEvent(tick);
        CloseThreadpoolTimer(timer);
        timer = NULL;
    }
    ULARGE_INTEGER ulDueTime = { 0 };
    ulDueTime.QuadPart = 2000 * (-10000); // 2 sec delay.
    FILETIME FileDueTime = { 0 };
    FileDueTime.dwHighDateTime = ulDueTime.HighPart;
    FileDueTime.dwLowDateTime = ulDueTime.LowPart;

    timer = CreateThreadpoolTimer(TimerTickCallback, NULL, NULL);
    ResetEvent(tick);
    SetThreadpoolTimer(timer, &FileDueTime, 0, 0);
}


LRESULT keyboardProcessing(int code, WPARAM w, LPARAM l)
{
    keyState = w == WM_KEYDOWN;
    if (!keyState) isTimerRestarted = false;  // Allow to restart the timer only when the key is released.

    if (((PKBDLLHOOKSTRUCT)l)->vkCode == VK_ESCAPE && keyState && !isTimerRestarted)
    {
        if (suspendedProcess != NULL)
        {
            NtResumeProcess(suspendedProcess);
            CloseHandle(suspendedProcess);
            suspendedProcess = NULL;
        }
        else if (!isTimerRestarted) RestartTimer(); // Don't call the function while the key is pressed.
    }

    return CallNextHookEx(NULL, code, w, l);
}

int main()
{
    ShowWindow(GetConsoleWindow(), SW_HIDE);

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
}
