/*
 * UNSUPPORTED EXPERIMENT: NT clone called from the Zend VM.
 * Do not enable on production systems. Never alias this to pcntl_fork.
 */
#ifndef PHP_PCNTL_CLONE_EXPERIMENT_H
#define PHP_PCNTL_CLONE_EXPERIMENT_H

#include <dbghelp.h>

/* Only the gated, single-request research binary uses these inherited events. */
static HANDLE pcntl_clone_checkpoints[5];

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_pcntl_windows_clone_checkpoint_experimental, 0, 1, _IS_BOOL, 0)
    ZEND_ARG_TYPE_INFO(0, stage, IS_LONG, 0)
ZEND_END_ARG_INFO()

PHP_FUNCTION(pcntl_windows_clone_checkpoint_experimental)
{
    zend_long stage;
    BOOL ok;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(stage)
    ZEND_PARSE_PARAMETERS_END();
    if (stage < 0 || stage >= 5 || !pcntl_clone_checkpoints[stage]) RETURN_FALSE;
    /* No Zend allocation, stdio, or CRT logging on this marker path. */
    ok = SetEvent(pcntl_clone_checkpoints[stage]);
    CloseHandle(pcntl_clone_checkpoints[stage]);
    pcntl_clone_checkpoints[stage] = NULL;
    RETURN_BOOL(ok);
}

static void pcntl_clone_dump(HANDLE process, DWORD pid)
{
    typedef BOOL (WINAPI *dump_proc)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE,
        PMINIDUMP_EXCEPTION_INFORMATION, PMINIDUMP_USER_STREAM_INFORMATION,
        PMINIDUMP_CALLBACK_INFORMATION);
    HMODULE library = LoadLibraryW(L"dbghelp.dll");
    dump_proc dump = library ? (dump_proc) GetProcAddress(library, "MiniDumpWriteDump") : NULL;
    HANDLE file;
    BOOL ok = FALSE;
    if (dump) {
        file = CreateFileW(L"pcntl-clone-child.dmp", GENERIC_WRITE, 0, NULL,
            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (file != INVALID_HANDLE_VALUE) {
            ok = dump(process, pid, file,
                MiniDumpWithThreadInfo | MiniDumpWithFullMemoryInfo, NULL, NULL, NULL);
            CloseHandle(file);
        }
    }
    php_printf("nt-clone-dump-written=%d\n", ok ? 1 : 0);
    if (library) FreeLibrary(library);
}

static void pcntl_clone_debug(HANDLE process, DWORD pid, bool ignore_invalid_handles)
{
    DEBUG_EVENT event;
    ULONGLONG deadline = GetTickCount64() + 15000;
    while (GetTickCount64() < deadline) {
        DWORD disposition = DBG_CONTINUE;
        if (!WaitForDebugEvent(&event, 100)) continue;
        if (event.dwDebugEventCode == EXCEPTION_DEBUG_EVENT) {
            DWORD code = event.u.Exception.ExceptionRecord.ExceptionCode;
            if (code != EXCEPTION_BREAKPOINT) disposition = DBG_EXCEPTION_NOT_HANDLED;
            if (ignore_invalid_handles && code == 0xc0000008
                && event.u.Exception.dwFirstChance) {
                /* Diagnostic only: emulate non-debugged CloseHandle failure
                 * to observe the later uninstrumented access violation. */
                php_printf("nt-clone-invalid-handle-observed=1\n");
                disposition = DBG_CONTINUE;
            }
            if (!event.u.Exception.dwFirstChance) {
                php_printf("nt-clone-exception=0x%08lx address=%p\n",
                    (unsigned long) code, event.u.Exception.ExceptionRecord.ExceptionAddress);
                if (code == EXCEPTION_ACCESS_VIOLATION
                    && event.u.Exception.ExceptionRecord.NumberParameters >= 2) {
                    MEMORY_BASIC_INFORMATION parent_region = {0}, child_region = {0};
                    LPCVOID fault = (LPCVOID) event.u.Exception.ExceptionRecord.ExceptionInformation[1];
                    VirtualQuery(fault, &parent_region, sizeof(parent_region));
                    VirtualQueryEx(process, fault, &child_region, sizeof(child_region));
                    php_printf("nt-clone-fault=%p parent-state=0x%lx parent-type=0x%lx parent-protect=0x%lx child-state=0x%lx child-type=0x%lx child-protect=0x%lx\n",
                        fault, (unsigned long) parent_region.State, (unsigned long) parent_region.Type,
                        (unsigned long) parent_region.Protect, (unsigned long) child_region.State,
                        (unsigned long) child_region.Type, (unsigned long) child_region.Protect);
                }
                pcntl_clone_dump(process, pid);
            }
        } else if (event.dwDebugEventCode == LOAD_DLL_DEBUG_EVENT) {
            if (event.u.LoadDll.hFile) CloseHandle(event.u.LoadDll.hFile);
        } else if (event.dwDebugEventCode == CREATE_PROCESS_DEBUG_EVENT) {
            if (event.u.CreateProcessInfo.hFile) CloseHandle(event.u.CreateProcessInfo.hFile);
        }
        ContinueDebugEvent(event.dwProcessId, event.dwThreadId, disposition);
        if (event.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT) return;
    }
    php_printf("nt-clone-debug-timeout=1\n");
    pcntl_clone_dump(process, pid);
    DebugActiveProcessStop(pid);
}

typedef LONG pcntl_ntstatus;
typedef struct {
    ULONG Length;
    HANDLE ProcessHandle;
    HANDLE ThreadHandle;
    HANDLE ProcessId;
    HANDLE ThreadId;
    BYTE ImageInformation[256];
} pcntl_nt_clone_info;

typedef pcntl_ntstatus (NTAPI *pcntl_nt_clone_proc)(
    ULONG flags, PSECURITY_DESCRIPTOR process_security,
    PSECURITY_DESCRIPTOR thread_security, HANDLE debug_port,
    pcntl_nt_clone_info *info
);
#define PCNTL_NT_CLONE_INHERIT_HANDLES 2
#define PCNTL_NT_CLONED_CHILD 0x129

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_pcntl_windows_clone_experimental, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

PHP_FUNCTION(pcntl_windows_clone_experimental)
{
    HMODULE nt;
    pcntl_nt_clone_proc clone;
    pcntl_nt_clone_info info;
    pcntl_ntstatus result;
    DWORD pid;
    pcntl_win_child *child;
    char enabled[2];
    SECURITY_ATTRIBUTES attrs;
    HANDLE probe;
    DWORD probe_result;
    int stage;
    bool pin_module;
    bool debug_child;
    bool ignore_invalid_handles;

    ZEND_PARSE_PARAMETERS_NONE();

    /* This opt-in is in addition to the compile-time flag. */
    if (GetEnvironmentVariableA("PCNTL_UNSAFE_CLONE_TEST", enabled, sizeof(enabled)) != 1
        || enabled[0] != '1') {
        zend_throw_error(NULL, "Experimental NT clone is disabled; set PCNTL_UNSAFE_CLONE_TEST=1 only in an isolated test process");
        RETURN_THROWS();
    }
    pin_module = GetEnvironmentVariableA("PCNTL_UNSAFE_CLONE_PIN_MODULE", enabled, sizeof(enabled)) == 1
        && enabled[0] == '1';
    debug_child = GetEnvironmentVariableA("PCNTL_UNSAFE_CLONE_DEBUG", enabled, sizeof(enabled)) == 1
        && enabled[0] == '1';
    ignore_invalid_handles = GetEnvironmentVariableA("PCNTL_UNSAFE_CLONE_IGNORE_INVALID_HANDLES", enabled, sizeof(enabled)) == 1
        && enabled[0] == '1';
    nt = GetModuleHandleW(L"ntdll.dll");
    if (!nt) {
        PCNTL_G(last_error) = ENOSYS;
        RETURN_LONG(-1);
    }
    clone = (pcntl_nt_clone_proc) GetProcAddress(nt, "RtlCloneUserProcess");
    if (!clone) {
        PCNTL_G(last_error) = ENOSYS;
        RETURN_LONG(-1);
    }
    memset(&attrs, 0, sizeof(attrs));
    attrs.nLength = sizeof(attrs);
    attrs.bInheritHandle = TRUE;
    probe = CreateEventW(&attrs, TRUE, FALSE, NULL);
    if (!probe) {
        PCNTL_G(last_error) = EIO;
        RETURN_LONG(-1);
    }
    for (stage = 0; stage < 5; stage++) {
        pcntl_clone_checkpoints[stage] = CreateEventW(&attrs, TRUE, FALSE, NULL);
        if (!pcntl_clone_checkpoints[stage]) {
            while (stage > 0) CloseHandle(pcntl_clone_checkpoints[--stage]);
            memset(pcntl_clone_checkpoints, 0, sizeof(pcntl_clone_checkpoints));
            CloseHandle(probe);
            PCNTL_G(last_error) = EIO;
            RETURN_LONG(-1);
        }
    }
    memset(&info, 0, sizeof(info));
    info.Length = sizeof(info);
    result = clone(PCNTL_NT_CLONE_INHERIT_HANDLES | (debug_child ? 1 : 0), NULL, NULL, NULL, &info);

    if (result == PCNTL_NT_CLONED_CHILD) {
        /* Pure Win32 event sets a cross-process marker before PHP VM resumes. */
        SetEvent(probe);
        CloseHandle(probe);
        if (pin_module) {
            HMODULE pinned;
            /* Isolated hypothesis: module unmapping triggers the observed
             * inherited LdrProtectMrdata SRW lock wait. Never a release fix. */
            if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                    | GET_MODULE_HANDLE_EX_FLAG_PIN,
                    (LPCWSTR) &pcntl_clone_checkpoints[0], &pinned)) {
                PCNTL_G(last_error) = EIO;
                RETURN_LONG(-1);
            }
        }
        /* Clone continues from this exact C call; the PHP VM may have unsafe
         * inherited locks and extension handles. The test must detect that. */
        PCNTL_G(last_error) = 0;
        RETURN_LONG(0);
    }
    if (result < 0 || result != 0 || !info.ProcessHandle) {
        for (stage = 0; stage < 5; stage++) CloseHandle(pcntl_clone_checkpoints[stage]);
        memset(pcntl_clone_checkpoints, 0, sizeof(pcntl_clone_checkpoints));
        CloseHandle(probe);
        PCNTL_G(last_error) = EIO;
        RETURN_LONG(-1);
    }
    if (debug_child) {
        pid = GetProcessId(info.ProcessHandle);
        if (!info.ThreadHandle || !DebugActiveProcess(pid)) {
            TerminateProcess(info.ProcessHandle, 99);
        } else {
            ResumeThread(info.ThreadHandle);
            pcntl_clone_debug(info.ProcessHandle, pid, ignore_invalid_handles);
        }
    }
    probe_result = WaitForSingleObject(probe, 2500);
    CloseHandle(probe);
    php_printf("nt-clone-child-reached=%d\n", probe_result == WAIT_OBJECT_0 ? 1 : 0);
    /* Give PHP continuation and shutdown a bounded interval. Report every
     * checkpoint even on failure, then preserve the child's native stacks. */
    WaitForSingleObject(pcntl_clone_checkpoints[4], 2500);
    for (stage = 0; stage < 5; stage++) {
        php_printf("nt-clone-checkpoint-%d=%d\n", stage,
            WaitForSingleObject(pcntl_clone_checkpoints[stage], 0) == WAIT_OBJECT_0 ? 1 : 0);
        CloseHandle(pcntl_clone_checkpoints[stage]);
        pcntl_clone_checkpoints[stage] = NULL;
    }
    if (WaitForSingleObject(info.ProcessHandle, 500) == WAIT_TIMEOUT) {
        pcntl_clone_dump(info.ProcessHandle, GetProcessId(info.ProcessHandle));
    } else {
        DWORD native_exit;
        if (GetExitCodeProcess(info.ProcessHandle, &native_exit)) {
            php_printf("nt-clone-native-exit=%lu\n", (unsigned long) native_exit);
        }
    }
    pid = GetProcessId(info.ProcessHandle);
    if (!pid) {
        CloseHandle(info.ProcessHandle);
        if (info.ThreadHandle) CloseHandle(info.ThreadHandle);
        PCNTL_G(last_error) = EIO;
        RETURN_LONG(-1);
    }
    if (info.ThreadHandle) CloseHandle(info.ThreadHandle);

    child = emalloc(sizeof(pcntl_win_child));
    child->pid = pid;
    child->group_id = 0;
    child->handle = info.ProcessHandle;
    child->wait_registration = NULL;
    child->ready_event = PCNTL_G(child_ready_event);
    /* The experimental NT clone is not registered with a threadpool wait. */
    PCNTL_G(child_wait_notifications_complete) = false;
    child->sigchld_sent = 0;
    child->next = PCNTL_G(children);
    PCNTL_G(children) = child;
    PCNTL_G(last_error) = 0;
    RETURN_LONG((zend_long) pid);
}

#endif
