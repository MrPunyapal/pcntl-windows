/* Experimental native Windows PCNTL extension. Not a Unix fork emulator. */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php.h"
#include "ext/standard/info.h"
#include "php_pcntl.h"

#ifndef PHP_WIN32
# error This proof of concept targets native Windows PHP only.
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <errno.h>
#include <limits.h>
#include <wchar.h>
#include <psapi.h>

#define PCNTL_WINDOWS_WNOHANG 1

typedef struct pcntl_win_child {
    DWORD pid;
    DWORD group_id; /* 0 = inherited/default; PID = new Windows console group */
    HANDLE handle;
    HANDLE wait_registration;
    HANDLE ready_event; /* Request-owned event, valid until callback unregister. */
    volatile LONG sigchld_sent;
    struct pcntl_win_child *next;
} pcntl_win_child;

ZEND_BEGIN_MODULE_GLOBALS(pcntl)
    pcntl_win_child *children;
    HANDLE child_ready_event;
    bool child_wait_notifications_complete;
    bool owns_active_request;
    int last_error;
    zval signal_handlers[32];
    DWORD signal_mask;
    bool async_signals;
    bool dispatching_signals;
    HANDLE alarm_timer;
    ULONGLONG alarm_deadline;
ZEND_END_MODULE_GLOBALS(pcntl)
ZEND_DECLARE_MODULE_GLOBALS(pcntl)
#define PCNTL_G(v) ZEND_MODULE_GLOBALS_ACCESSOR(pcntl, v)
/* The signal queue and VM interrupt target are process-wide. Refuse concurrent
 * PHP requests instead of delivering an event into another thread's VM. */
static volatile LONG pcntl_active_request = 0;

static void pcntl_globals_init(zend_pcntl_globals *g)
{
    g->children = NULL;
    g->last_error = 0;
}

/* Threadpool callbacks may run on a Windows worker thread. Only queue events;
 * never access PHP zvals or call PHP userland from these callbacks. */
static void pcntl_win_enqueue(int signum);
static int pcntl_win32_errno(DWORD code);

static void pcntl_win_emit_sigchld(pcntl_win_child *child)
{
    if (InterlockedCompareExchange(&child->sigchld_sent, 1, 0) == 0) {
        pcntl_win_enqueue(17); /* SIGCHLD in this extension's constants. */
    }
}

static VOID CALLBACK pcntl_win_child_exited(PVOID context, BOOLEAN timed_out)
{
    pcntl_win_child *child = (pcntl_win_child *) context;
    if (!timed_out) {
        /* Wake any-child and waitid waiters without polling all process handles.
         * The PHP request owns this event until every callback is unregistered. */
        if (child->ready_event != NULL) SetEvent(child->ready_event);
        pcntl_win_emit_sigchld(child);
    }
}

static bool pcntl_win_wait_for_child_event(void)
{
    DWORD result;
    if (!PCNTL_G(child_ready_event) || !PCNTL_G(child_wait_notifications_complete)) {
        /* A wait registration failed, so some children have no event source. */
        Sleep(10);
        return true;
    }
    result = WaitForSingleObject(PCNTL_G(child_ready_event), INFINITE);
    if (result == WAIT_OBJECT_0) return true;
    PCNTL_G(last_error) = pcntl_win32_errno(GetLastError());
    return false;
}

static void pcntl_win_release_child(pcntl_win_child *child)
{
    if (child->wait_registration != NULL) {
        /* Wait until pending callbacks are finished before releasing child. */
        UnregisterWaitEx(child->wait_registration, INVALID_HANDLE_VALUE);
        child->wait_registration = NULL;
    }
    CloseHandle(child->handle);
    efree(child);
}

static int pcntl_win32_errno(DWORD code)
{
    switch (code) {
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND: return ENOENT;
        case ERROR_ACCESS_DENIED: return EACCES;
        case ERROR_NOT_ENOUGH_MEMORY:
        case ERROR_OUTOFMEMORY: return ENOMEM;
        case ERROR_TOO_MANY_OPEN_FILES: return EMFILE;
        case ERROR_INVALID_PARAMETER: return EINVAL;
        case ERROR_INVALID_HANDLE: return ECHILD;
        default: return EIO;
    }
}

/* Convert a single PHP UTF-8 string to Windows UTF-16; embedded NUL is forbidden. */
static wchar_t *pcntl_utf16(const zend_string *s)
{
    int n;
    wchar_t *buffer;
    if (ZSTR_LEN(s) > INT_MAX || memchr(ZSTR_VAL(s), '\0', ZSTR_LEN(s))) {
        PCNTL_G(last_error) = EINVAL;
        return NULL;
    }
    if (ZSTR_LEN(s) == 0) {
        buffer = emalloc(sizeof(wchar_t));
        buffer[0] = L'\0';
        return buffer;
    }
    n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, ZSTR_VAL(s), (int) ZSTR_LEN(s), NULL, 0);
    if (!n) {
        PCNTL_G(last_error) = EINVAL;
        return NULL;
    }
    buffer = safe_emalloc((size_t) n + 1, sizeof(wchar_t), 0);
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, ZSTR_VAL(s), (int) ZSTR_LEN(s), buffer, n)) {
        efree(buffer);
        PCNTL_G(last_error) = EINVAL;
        return NULL;
    }
    buffer[n] = L'\0';
    return buffer;
}

/* Microsoft C runtime command-line quoting; no cmd.exe/shell evaluation. */
static wchar_t *pcntl_quote_argv(wchar_t **argv, size_t argc)
{
    size_t i, j, pos = 0, cap = 1;
    wchar_t *result;
    for (i = 0; i < argc; i++) {
        size_t len = wcslen(argv[i]);
        if (len > 16000 || cap + 2 * len + 3 > 32767) {
            PCNTL_G(last_error) = E2BIG;
            return NULL;
        }
        cap += 2 * len + 3;
    }
    result = safe_emalloc(cap, sizeof(wchar_t), 0);
    for (i = 0; i < argc; i++) {
        size_t backslashes = 0;
        if (i) result[pos++] = L' ';
        result[pos++] = L'"';
        for (j = 0; ; j++) {
            wchar_t ch = argv[i][j];
            if (ch == L'\\') {
                backslashes++;
                continue;
            }
            if (ch == L'"' || ch == L'\0') {
                size_t n = backslashes * 2 + (ch == L'"' ? 1 : 0);
                while (n--) result[pos++] = L'\\';
                backslashes = 0;
                if (ch == L'\0') break;
                result[pos++] = L'"';
            } else {
                while (backslashes--) result[pos++] = L'\\';
                backslashes = 0;
                result[pos++] = ch;
            }
        }
        result[pos++] = L'"';
    }
    result[pos] = L'\0';
    return result;
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_pcntl_windows_spawn, 0, 1, MAY_BE_LONG|MAY_BE_FALSE)
    ZEND_ARG_TYPE_INFO(0, executable, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, arguments, IS_ARRAY, 0, "[]")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, new_process_group, _IS_BOOL, 0, "false")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_pcntl_waitpid, 0, 2, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, process_id, IS_LONG, 0)
    ZEND_ARG_INFO(1, status)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, flags, IS_LONG, 0, "0")
    ZEND_ARG_INFO_WITH_DEFAULT_VALUE(1, resource_usage, "[]")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_pcntl_wait, 0, 1, IS_LONG, 0)
    ZEND_ARG_INFO(1, status)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, flags, IS_LONG, 0, "0")
    ZEND_ARG_INFO_WITH_DEFAULT_VALUE(1, resource_usage, "[]")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_pcntl_status_bool, 0, 1, _IS_BOOL, 0)
    ZEND_ARG_TYPE_INFO(0, status, IS_LONG, 0)
ZEND_END_ARG_INFO()
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_pcntl_status_int, 0, 1, MAY_BE_LONG|MAY_BE_FALSE)
    ZEND_ARG_TYPE_INFO(0, status, IS_LONG, 0)
ZEND_END_ARG_INFO()
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_pcntl_get_last_error, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_pcntl_strerror, 0, 1, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, error_code, IS_LONG, 0)
ZEND_END_ARG_INFO()

/* Stop only processes created in this request; not POSIX signaling. */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_pcntl_windows_terminate, 0, 1, _IS_BOOL, 0)
    ZEND_ARG_TYPE_INFO(0, process_id, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, exit_code, IS_LONG, 0, "1")
ZEND_END_ARG_INFO()

PHP_FUNCTION(pcntl_windows_terminate)
{
    zend_long pid, exit_code = 1;
    pcntl_win_child *child;
    DWORD wait_result;

    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_LONG(pid)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(exit_code)
    ZEND_PARSE_PARAMETERS_END();

    if (exit_code < 0 || exit_code > 255) {
        zend_argument_value_error(2, "must be between 0 and 255");
        RETURN_THROWS();
    }
    if (pid <= 0 || (uint64_t) pid > UINT32_MAX) {
        PCNTL_G(last_error) = EINVAL;
        RETURN_FALSE;
    }

    for (child = PCNTL_G(children); child; child = child->next) {
        if (child->pid == (DWORD) pid) {
            break;
        }
    }
    if (!child) {
        PCNTL_G(last_error) = ECHILD;
        RETURN_FALSE;
    }

    wait_result = WaitForSingleObject(child->handle, 0);
    if (wait_result == WAIT_OBJECT_0) {
        PCNTL_G(last_error) = ECHILD;
        RETURN_FALSE;
    }
    if (wait_result != WAIT_TIMEOUT) {
        PCNTL_G(last_error) = pcntl_win32_errno(GetLastError());
        RETURN_FALSE;
    }
    if (!TerminateProcess(child->handle, (UINT) exit_code)) {
        PCNTL_G(last_error) = pcntl_win32_errno(GetLastError());
        RETURN_FALSE;
    }
    PCNTL_G(last_error) = 0;
    RETURN_TRUE;
}

PHP_FUNCTION(pcntl_windows_spawn)
{
    zend_string *executable;
    HashTable *arguments = NULL;
    size_t argc, i = 0;
    wchar_t **argv, *command_line = NULL;
    zval *value;
    STARTUPINFOW startup;
    PROCESS_INFORMATION process;
    pcntl_win_child *child;
    bool invalid = false;
    bool new_process_group = false;

    ZEND_PARSE_PARAMETERS_START(1, 3)
        Z_PARAM_STR(executable)
        Z_PARAM_OPTIONAL
        Z_PARAM_ARRAY_HT(arguments)
        Z_PARAM_BOOL(new_process_group)
    ZEND_PARSE_PARAMETERS_END();

    if (ZSTR_LEN(executable) == 0) {
        zend_argument_value_error(1, "must not be empty");
        RETURN_THROWS();
    }
    argc = arguments ? zend_hash_num_elements(arguments) + 1 : 1;
    argv = safe_emalloc(argc, sizeof(wchar_t *), 0);
    memset(argv, 0, argc * sizeof(wchar_t *));
    argv[0] = pcntl_utf16(executable);
    if (!argv[0]) invalid = true;
    if (!invalid && arguments) {
        ZEND_HASH_FOREACH_VAL(arguments, value) {
            i++;
            if (Z_TYPE_P(value) != IS_STRING) {
                zend_type_error("pcntl_windows_spawn(): Argument #2 ($arguments) must contain only strings");
                invalid = true;
                break;
            }
            argv[i] = pcntl_utf16(Z_STR_P(value));
            if (!argv[i]) {
                invalid = true;
                break;
            }
        } ZEND_HASH_FOREACH_END();
    }
    if (!invalid) {
        command_line = pcntl_quote_argv(argv, argc);
        if (!command_line) invalid = true;
    }
    if (!invalid) {
        memset(&startup, 0, sizeof(startup));
        startup.cb = sizeof(startup);
        memset(&process, 0, sizeof(process));
        if (!CreateProcessW(argv[0], command_line, NULL, NULL, FALSE,
                new_process_group ? CREATE_NEW_PROCESS_GROUP : 0, NULL, NULL, &startup, &process)) {
            PCNTL_G(last_error) = pcntl_win32_errno(GetLastError());
            invalid = true;
        } else {
            CloseHandle(process.hThread);
            child = emalloc(sizeof(pcntl_win_child));
            child->pid = process.dwProcessId;
            child->group_id = new_process_group ? process.dwProcessId : 0;
            child->handle = process.hProcess;
            child->wait_registration = NULL;
            child->ready_event = PCNTL_G(child_ready_event);
            child->sigchld_sent = 0;
            /* If a wait registration fails, preserve the child but fall back
             * to polling so a blocking wait cannot hang on an unsignaled event. */
            if (!RegisterWaitForSingleObject(
                    &child->wait_registration, child->handle,
                    pcntl_win_child_exited, child, INFINITE, WT_EXECUTEONLYONCE)) {
                PCNTL_G(child_wait_notifications_complete) = false;
            }
            child->next = PCNTL_G(children);
            PCNTL_G(children) = child;
            PCNTL_G(last_error) = 0;
        }
    }
    if (command_line) efree(command_line);
    for (i = 0; i < argc; i++) {
        if (argv[i]) efree(argv[i]);
    }
    efree(argv);
    if (EG(exception)) RETURN_THROWS();
    if (invalid) RETURN_FALSE;
    RETURN_LONG((zend_long) process.dwProcessId);
}

/* Map only metrics whose Win32 equivalents are useful to PHP callers.
 * Windows PeakWorkingSetSize is an approximation of POSIX ru_maxrss. */
static void pcntl_win_rusage(HANDLE handle, zval *usage)
{
    FILETIME created, exited, kernel, user;
    PROCESS_MEMORY_COUNTERS memory;
    ULARGE_INTEGER ticks;
    ZVAL_DEREF(usage);
    zval_ptr_dtor(usage);
    array_init(usage);
    if (GetProcessTimes(handle, &created, &exited, &kernel, &user)) {
        ticks.LowPart = user.dwLowDateTime;
        ticks.HighPart = user.dwHighDateTime;
        add_assoc_long(usage, "ru_utime.tv_sec", (zend_long) (ticks.QuadPart / 10000000ULL));
        add_assoc_long(usage, "ru_utime.tv_usec", (zend_long) ((ticks.QuadPart % 10000000ULL) / 10));
        ticks.LowPart = kernel.dwLowDateTime;
        ticks.HighPart = kernel.dwHighDateTime;
        add_assoc_long(usage, "ru_stime.tv_sec", (zend_long) (ticks.QuadPart / 10000000ULL));
        add_assoc_long(usage, "ru_stime.tv_usec", (zend_long) ((ticks.QuadPart % 10000000ULL) / 10));
    }
    memset(&memory, 0, sizeof(memory));
    memory.cb = sizeof(memory);
    if (GetProcessMemoryInfo(handle, &memory, sizeof(memory))) {
        add_assoc_long(usage, "ru_maxrss", (zend_long) (memory.PeakWorkingSetSize / 1024));
    }
}

/* Wait on one child or use threadpool exit notifications for any-child waits.
 * Unlike WaitForMultipleObjects, this is not restricted to 64 handles. */
static zend_long pcntl_wait_impl(zend_long pid, zval *status, zend_long flags, zval *usage)
{
    pcntl_win_child *node, *selected = NULL, **link;
    DWORD result, exit_code;

    if ((flags & ~PCNTL_WINDOWS_WNOHANG) != 0
        || (pid < -1 && (pid == ZEND_LONG_MIN || (uint64_t)(-pid) > UINT32_MAX))) {
        PCNTL_G(last_error) = EINVAL;
        return -1;
    }
    if (!PCNTL_G(children)) {
        PCNTL_G(last_error) = ECHILD;
        return -1;
    }
    if (pid > 0) {
        for (node = PCNTL_G(children); node; node = node->next) {
            if (pid == (zend_long) node->pid) break;
        }
        if (!node) {
            PCNTL_G(last_error) = ECHILD;
            return -1;
        }
        result = WaitForSingleObject(node->handle, (flags & PCNTL_WINDOWS_WNOHANG) ? 0 : INFINITE);
        if (result == WAIT_TIMEOUT) {
            PCNTL_G(last_error) = 0;
            return 0;
        }
        if (result != WAIT_OBJECT_0) {
            PCNTL_G(last_error) = pcntl_win32_errno(GetLastError());
            return -1;
        }
        selected = node;
    } else {
        for (;;) {
            bool found = false;
            for (node = PCNTL_G(children); node; node = node->next) {
                /* Process-group filtering is scoped to explicitly registered
                 * Win32 child handles, not the global POSIX process table. */
                if (pid == 0 && node->group_id != 0) continue;
                if (pid < -1 && node->group_id != (DWORD)(-pid)) continue;
                found = true;
                result = WaitForSingleObject(node->handle, 0);
                if (result == WAIT_OBJECT_0) {
                    selected = node;
                    break;
                }
                if (result != WAIT_TIMEOUT) {
                    PCNTL_G(last_error) = pcntl_win32_errno(GetLastError());
                    return -1;
                }
            }
            if (selected) break;
            if (!found) {
                PCNTL_G(last_error) = ECHILD;
                return -1;
            }
            if (flags & PCNTL_WINDOWS_WNOHANG) {
                PCNTL_G(last_error) = 0;
                return 0;
            }
            if (!pcntl_win_wait_for_child_event()) return -1;
        }
    }
    if (!GetExitCodeProcess(selected->handle, &exit_code)) {
        PCNTL_G(last_error) = pcntl_win32_errno(GetLastError());
        return -1;
    }
    pcntl_win_emit_sigchld(selected);
    ZVAL_DEREF(status);
    zval_ptr_dtor(status);
    ZVAL_LONG(status, (zend_long) ((exit_code & 0xff) << 8));
    if (usage) pcntl_win_rusage(selected->handle, usage);
    pid = (zend_long) selected->pid;
    for (link = &PCNTL_G(children); *link; link = &(*link)->next) {
        if (*link == selected) {
            *link = selected->next;
            break;
        }
    }
    pcntl_win_release_child(selected);
    PCNTL_G(last_error) = 0;
    return pid;
}

PHP_FUNCTION(pcntl_waitpid)
{
    zend_long pid, flags = 0;
    zval *status, *usage = NULL;
    ZEND_PARSE_PARAMETERS_START(2, 4)
        Z_PARAM_LONG(pid)
        Z_PARAM_ZVAL(status)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(flags)
        Z_PARAM_ZVAL(usage)
    ZEND_PARSE_PARAMETERS_END();
    RETURN_LONG(pcntl_wait_impl(pid, status, flags, usage));
}
PHP_FUNCTION(pcntl_wait)
{
    zend_long flags = 0;
    zval *status, *usage = NULL;
    ZEND_PARSE_PARAMETERS_START(1, 3)
        Z_PARAM_ZVAL(status)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(flags)
        Z_PARAM_ZVAL(usage)
    ZEND_PARSE_PARAMETERS_END();
    RETURN_LONG(pcntl_wait_impl(-1, status, flags, usage));
}
PHP_FUNCTION(pcntl_wifexited)
{
    zend_long status;
    ZEND_PARSE_PARAMETERS_START(1, 1) Z_PARAM_LONG(status) ZEND_PARSE_PARAMETERS_END();
    RETURN_BOOL((status & 0x7f) == 0);
}
PHP_FUNCTION(pcntl_wifstopped)
{
    zend_long status;
    ZEND_PARSE_PARAMETERS_START(1, 1) Z_PARAM_LONG(status) ZEND_PARSE_PARAMETERS_END();
    RETURN_BOOL((status & 0xff) == 0x7f);
}
PHP_FUNCTION(pcntl_wifsignaled)
{
    zend_long status;
    ZEND_PARSE_PARAMETERS_START(1, 1) Z_PARAM_LONG(status) ZEND_PARSE_PARAMETERS_END();
    RETURN_BOOL((status & 0x7f) != 0 && (status & 0x7f) != 0x7f);
}
PHP_FUNCTION(pcntl_wexitstatus)
{
    zend_long status;
    ZEND_PARSE_PARAMETERS_START(1, 1) Z_PARAM_LONG(status) ZEND_PARSE_PARAMETERS_END();
    if ((status & 0x7f) != 0) RETURN_FALSE;
    RETURN_LONG((status >> 8) & 0xff);
}
PHP_FUNCTION(pcntl_wtermsig)
{
    zend_long status;
    ZEND_PARSE_PARAMETERS_START(1, 1) Z_PARAM_LONG(status) ZEND_PARSE_PARAMETERS_END();
    if ((status & 0x7f) == 0 || (status & 0x7f) == 0x7f) RETURN_FALSE;
    RETURN_LONG(status & 0x7f);
}
PHP_FUNCTION(pcntl_wstopsig)
{
    zend_long status;
    ZEND_PARSE_PARAMETERS_START(1, 1) Z_PARAM_LONG(status) ZEND_PARSE_PARAMETERS_END();
    if ((status & 0xff) != 0x7f) RETURN_FALSE;
    RETURN_LONG((status >> 8) & 0xff);
}
PHP_FUNCTION(pcntl_get_last_error)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_LONG(PCNTL_G(last_error));
}
PHP_FUNCTION(pcntl_strerror)
{
    zend_long code;
    ZEND_PARSE_PARAMETERS_START(1, 1) Z_PARAM_LONG(code) ZEND_PARSE_PARAMETERS_END();
    RETURN_STRING(strerror((int) code));
}

#include "pcntl_extra.h"
#include "pcntl_waitid.h"
#include "pcntl_signals.h"
#include "Zend/zend_enum.h"
#include "pcntl_sigwait.h"
#include "pcntl_qos.h"

#ifdef PHP_PCNTL_UNSAFE_CLONE
#include "pcntl_clone_experiment.h"
#endif

static const zend_function_entry pcntl_functions[] = {
#ifdef PHP_PCNTL_UNSAFE_CLONE
    PHP_FE(pcntl_windows_clone_experimental, arginfo_pcntl_windows_clone_experimental)
    PHP_FE(pcntl_windows_clone_checkpoint_experimental, arginfo_pcntl_windows_clone_checkpoint_experimental)
#endif
    PHP_FE(pcntl_windows_spawn, arginfo_pcntl_windows_spawn)
    PHP_FE(pcntl_windows_terminate, arginfo_pcntl_windows_terminate)
    PHP_FE(pcntl_getpriority, arginfo_pcntl_getpriority)
    PHP_FE(pcntl_setpriority, arginfo_pcntl_setpriority)
    PHP_FE(pcntl_getcpuaffinity, arginfo_pcntl_getcpuaffinity)
    PHP_FE(pcntl_setcpuaffinity, arginfo_pcntl_setcpuaffinity)
    PHP_FE(pcntl_getcpu, arginfo_pcntl_getcpu)
    PHP_FE(pcntl_getqos_class, arginfo_pcntl_getqos_class)
    PHP_FE(pcntl_setqos_class, arginfo_pcntl_setqos_class)
    PHP_FE(pcntl_wifcontinued, arginfo_pcntl_status_bool)
    PHP_FE(pcntl_signal, arginfo_pcntl_signal)
    PHP_FE(pcntl_signal_get_handler, arginfo_pcntl_signal_get_handler)
    PHP_FE(pcntl_signal_dispatch, arginfo_pcntl_signal_dispatch)
    PHP_FE(pcntl_async_signals, arginfo_pcntl_async_signals)
    PHP_FE(pcntl_sigprocmask, arginfo_pcntl_sigprocmask)
    PHP_FE(pcntl_alarm, arginfo_pcntl_alarm)
    PHP_FE(pcntl_sigwaitinfo, arginfo_pcntl_sigwaitinfo)
    PHP_FE(pcntl_sigtimedwait, arginfo_pcntl_sigtimedwait)
    PHP_FE(pcntl_windows_raise, arginfo_pcntl_windows_raise)
    PHP_FE(pcntl_waitid, arginfo_pcntl_waitid)
    PHP_FE(pcntl_waitpid, arginfo_pcntl_waitpid)
    PHP_FE(pcntl_wait, arginfo_pcntl_wait)
    PHP_FE(pcntl_wifexited, arginfo_pcntl_status_bool)
    PHP_FE(pcntl_wifstopped, arginfo_pcntl_status_bool)
    PHP_FE(pcntl_wifsignaled, arginfo_pcntl_status_bool)
    PHP_FE(pcntl_wexitstatus, arginfo_pcntl_status_int)
    PHP_FE(pcntl_wtermsig, arginfo_pcntl_status_int)
    PHP_FE(pcntl_wstopsig, arginfo_pcntl_status_int)
    PHP_FE(pcntl_get_last_error, arginfo_pcntl_get_last_error)
    PHP_FALIAS(pcntl_errno, pcntl_get_last_error, arginfo_pcntl_get_last_error)
    PHP_FE(pcntl_strerror, arginfo_pcntl_strerror)
    PHP_FE_END
};

PHP_MINIT_FUNCTION(pcntl)
{
    ZEND_INIT_MODULE_GLOBALS(pcntl, pcntl_globals_init, NULL);
    if (pcntl_register_qos() != SUCCESS) return FAILURE;
    pcntl_signal_ready_event = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (!pcntl_signal_ready_event) {
        php_error_docref(NULL, E_WARNING, "unable to create cooperative signal notification event");
        return FAILURE;
    }
    REGISTER_LONG_CONSTANT("PRIO_PROCESS", PCNTL_PRIO_PROCESS, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("PRIO_PGRP", PCNTL_PRIO_PGRP, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("PRIO_USER", PCNTL_PRIO_USER, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("WCONTINUED", 8, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("P_ALL", PCNTL_P_ALL, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("P_PID", PCNTL_P_PID, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("P_PGID", PCNTL_P_PGID, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("WEXITED", PCNTL_WEXITED, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("WNOWAIT", PCNTL_WNOWAIT, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("CLD_EXITED", PCNTL_CLD_EXITED, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SIGCHLD", PCNTL_SIGCHLD, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SIG_DFL", PCNTL_SIG_DFL, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SIG_IGN", PCNTL_SIG_IGN, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SIGHUP", PCNTL_SIGHUP, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SIGINT", PCNTL_SIGINT, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SIGUSR1", PCNTL_SIGUSR1, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SIGUSR2", PCNTL_SIGUSR2, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SIGALRM", PCNTL_SIGALRM, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SIGTERM", PCNTL_SIGTERM, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SIG_BLOCK", PCNTL_SIG_BLOCK, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SIG_UNBLOCK", PCNTL_SIG_UNBLOCK, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SIG_SETMASK", PCNTL_SIG_SETMASK, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("WNOHANG", PCNTL_WINDOWS_WNOHANG, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("PCNTL_ECHILD", ECHILD, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("PCNTL_EINVAL", EINVAL, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("PCNTL_ENOENT", ENOENT, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("PCNTL_EACCES", EACCES, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("PCNTL_ENOMEM", ENOMEM, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("PCNTL_EAGAIN", EAGAIN, CONST_CS | CONST_PERSISTENT);
    pcntl_previous_interrupt = zend_interrupt_function;
    zend_interrupt_function = pcntl_win_interrupt_handler;
    pcntl_console_installed = SetConsoleCtrlHandler(pcntl_win_console_handler, TRUE) != 0;
    return SUCCESS;
}
PHP_MSHUTDOWN_FUNCTION(pcntl)
{
    if (zend_interrupt_function == pcntl_win_interrupt_handler) zend_interrupt_function = pcntl_previous_interrupt;
    if (pcntl_console_installed) SetConsoleCtrlHandler(pcntl_win_console_handler, FALSE);
    if (pcntl_signal_ready_event) {
        CloseHandle(pcntl_signal_ready_event);
        pcntl_signal_ready_event = NULL;
    }
    return SUCCESS;
}
PHP_RINIT_FUNCTION(pcntl)
{
#if defined(ZTS) && defined(COMPILE_DL_PCNTL)
    ZEND_TSRMLS_CACHE_UPDATE();
#endif
    PCNTL_G(owns_active_request) = false;
    if (InterlockedCompareExchange(&pcntl_active_request, 1, 0) != 0) {
        php_error_docref(NULL, E_WARNING,
            "pcntl-windows supports only one active PHP request per process");
        return FAILURE;
    }
    PCNTL_G(owns_active_request) = true;
    PCNTL_G(children) = NULL;
    PCNTL_G(child_ready_event) = CreateEventW(NULL, FALSE, FALSE, NULL);
    PCNTL_G(child_wait_notifications_complete) = true;
    PCNTL_G(last_error) = 0;
    pcntl_win_signal_request_init();
    return SUCCESS;
}
PHP_RSHUTDOWN_FUNCTION(pcntl)
{
    pcntl_win_child *child, *next;
    if (!PCNTL_G(owns_active_request)) return SUCCESS;
    child = PCNTL_G(children);
    while (child) {
        next = child->next;
        pcntl_win_release_child(child); /* Detach; do not kill running children. */
        child = next;
    }
    PCNTL_G(children) = NULL;
    /* All child callbacks have completed before closing their shared event. */
    if (PCNTL_G(child_ready_event)) {
        CloseHandle(PCNTL_G(child_ready_event));
        PCNTL_G(child_ready_event) = NULL;
    }
    pcntl_win_signal_request_shutdown();
    PCNTL_G(owns_active_request) = false;
    InterlockedExchange(&pcntl_active_request, 0);
    return SUCCESS;
}
PHP_MINFO_FUNCTION(pcntl)
{
    php_info_print_table_start();
    php_info_print_table_row(2, "PCNTL on Windows", "experimental native Win32 process subset");
    php_info_print_table_row(2, "fork support", "not implemented");
    php_info_print_table_end();
}
zend_module_entry pcntl_module_entry = {
    STANDARD_MODULE_HEADER,
    "pcntl",
    pcntl_functions,
    PHP_MINIT(pcntl),
    PHP_MSHUTDOWN(pcntl),
    PHP_RINIT(pcntl),
    PHP_RSHUTDOWN(pcntl),
    PHP_MINFO(pcntl),
    PHP_PCNTL_WINDOWS_VERSION,
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_PCNTL
# ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
# endif
ZEND_GET_MODULE(pcntl)
#endif
