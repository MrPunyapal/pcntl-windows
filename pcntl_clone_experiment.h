/*
 * UNSUPPORTED EXPERIMENT: NT clone called from the Zend VM.
 * Do not enable on production systems. Never alias this to pcntl_fork.
 */
#ifndef PHP_PCNTL_CLONE_EXPERIMENT_H
#define PHP_PCNTL_CLONE_EXPERIMENT_H

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

    ZEND_PARSE_PARAMETERS_NONE();

    /* This opt-in is in addition to the compile-time flag. */
    if (GetEnvironmentVariableA("PCNTL_UNSAFE_CLONE_TEST", enabled, sizeof(enabled)) != 1
        || enabled[0] != '1') {
        zend_throw_error(NULL, "Experimental NT clone is disabled; set PCNTL_UNSAFE_CLONE_TEST=1 only in an isolated test process");
        RETURN_THROWS();
    }
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
    memset(&info, 0, sizeof(info));
    info.Length = sizeof(info);
    result = clone(PCNTL_NT_CLONE_INHERIT_HANDLES, NULL, NULL, NULL, &info);

    if (result == PCNTL_NT_CLONED_CHILD) {
        /* Pure Win32 event sets a cross-process marker before PHP VM resumes. */
        SetEvent(probe);
        CloseHandle(probe);
        /* Clone continues from this exact C call; the PHP VM may have unsafe
         * inherited locks and extension handles. The test must detect that. */
        PCNTL_G(last_error) = 0;
        RETURN_LONG(0);
    }
    if (result < 0 || result != 0 || !info.ProcessHandle) {
        CloseHandle(probe);
        PCNTL_G(last_error) = EIO;
        RETURN_LONG(-1);
    }
    probe_result = WaitForSingleObject(probe, 2500);
    CloseHandle(probe);
    php_printf("nt-clone-child-reached=%d\n", probe_result == WAIT_OBJECT_0 ? 1 : 0);
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
    child->sigchld_sent = 0;
    child->next = PCNTL_G(children);
    PCNTL_G(children) = child;
    PCNTL_G(last_error) = 0;
    RETURN_LONG((zend_long) pid);
}

#endif
