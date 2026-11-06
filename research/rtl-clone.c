/*
 * Standalone NT process clone experiment. NOT a PHP extension.
 * Uses undocumented ntdll APIs. Never run inside PHP or a server.
 *
 * Tests clone continuation, private parent/child state, exit and wait.
 * No custom NT library or PHNT headers are required. The output structure
 * over-allocates the architecture-dependent IMAGE_INFORMATION tail.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

typedef LONG NTSTATUS;
typedef struct {
    ULONG Length;
    HANDLE ProcessHandle;
    HANDLE ThreadHandle;
    HANDLE ProcessId;
    HANDLE ThreadId;
    BYTE ImageInformation[256];
} CLONE_INFO;

typedef NTSTATUS (NTAPI *CLONE_PROC)(
    ULONG,
    PSECURITY_DESCRIPTOR,
    PSECURITY_DESCRIPTOR,
    HANDLE,
    CLONE_INFO *
);
typedef NTSTATUS (NTAPI *TERMINATE_PROC)(HANDLE, NTSTATUS);

#define CLONE_CHILD_STATUS ((NTSTATUS)0x00000129)
#define CLONE_INHERIT_HANDLES ((ULONG)0x00000002)

static volatile LONG clone_test_value = 31415;

int main(void)
{
    HMODULE ntdll;
    CLONE_PROC clone;
    TERMINATE_PROC nt_terminate;
    CLONE_INFO info;
    NTSTATUS result;
    DWORD wait_result, child_exit;
    int success = 0;

    ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) {
        fprintf(stderr, "ntdll.dll not loaded\n");
        return 10;
    }
    clone = (CLONE_PROC) GetProcAddress(ntdll, "RtlCloneUserProcess");
    nt_terminate = (TERMINATE_PROC) GetProcAddress(ntdll, "NtTerminateProcess");
    if (!clone || !nt_terminate) {
        fprintf(stderr, "Process clone exports unavailable\n");
        return 11;
    }
    ZeroMemory(&info, sizeof(info));
    info.Length = sizeof(info);
    result = clone(CLONE_INHERIT_HANDLES, NULL, NULL, NULL, &info);

    if (result == CLONE_CHILD_STATUS) {
        NTSTATUS child_result = clone_test_value == 31415 ? 39 : 90;
        clone_test_value = 12345;
        /* Avoid C runtime cleanup and DLL loader teardown in a clone. */
        nt_terminate(GetCurrentProcess(), child_result);
        return 91; /* Should never return. */
    }

    if (result < 0) {
        fprintf(stderr, "RtlCloneUserProcess failed with NTSTATUS 0x%08lx\n",
            (unsigned long)(ULONG)result);
        return 12;
    }
    if (result != 0 || !info.ProcessHandle) {
        fprintf(stderr, "Unexpected clone status 0x%08lx\n", (unsigned long)(ULONG)result);
        return 13;
    }
    wait_result = WaitForSingleObject(info.ProcessHandle, 15000);
    if (wait_result == WAIT_TIMEOUT) {
        fprintf(stderr, "Child did not exit within timeout\n");
        TerminateProcess(info.ProcessHandle, 99);
    } else if (wait_result == WAIT_OBJECT_0
        && GetExitCodeProcess(info.ProcessHandle, &child_exit)) {
        success = child_exit == 39 && clone_test_value == 31415;
        printf("Clone child exit: %lu; parent memory intact: %s\n",
            (unsigned long)child_exit, clone_test_value == 31415 ? "yes" : "no");
    } else {
        fprintf(stderr, "WaitForSingleObject/GetExitCodeProcess failed: %lu\n",
            (unsigned long)GetLastError());
    }

    if (info.ThreadHandle) CloseHandle(info.ThreadHandle);
    CloseHandle(info.ProcessHandle);
    return success ? 0 : 14;
}
