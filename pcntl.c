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

#define PCNTL_WINDOWS_WNOHANG 1

typedef struct pcntl_win_child {
    DWORD pid;
    HANDLE handle;
    struct pcntl_win_child *next;
} pcntl_win_child;

ZEND_BEGIN_MODULE_GLOBALS(pcntl)
    pcntl_win_child *children;
    int last_error;
ZEND_END_MODULE_GLOBALS(pcntl)
ZEND_DECLARE_MODULE_GLOBALS(pcntl)
#define PCNTL_G(v) ZEND_MODULE_GLOBALS_ACCESSOR(pcntl, v)

static void pcntl_globals_init(zend_pcntl_globals *g)
{
    g->children = NULL;
    g->last_error = 0;
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
    int count = 0;
    bool invalid = false;

    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_STR(executable)
        Z_PARAM_OPTIONAL
        Z_PARAM_ARRAY_HT(arguments)
    ZEND_PARSE_PARAMETERS_END();

    if (ZSTR_LEN(executable) == 0) {
        zend_argument_value_error(1, "must not be empty");
        RETURN_THROWS();
    }
    for (child = PCNTL_G(children); child; child = child->next) count++;
    if (count >= MAXIMUM_WAIT_OBJECTS) {
        PCNTL_G(last_error) = EAGAIN;
        RETURN_FALSE;
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
        if (!CreateProcessW(argv[0], command_line, NULL, NULL, FALSE, 0, NULL, NULL, &startup, &process)) {
            PCNTL_G(last_error) = pcntl_win32_errno(GetLastError());
            invalid = true;
        } else {
            CloseHandle(process.hThread);
            child = emalloc(sizeof(pcntl_win_child));
            child->pid = process.dwProcessId;
            child->handle = process.hProcess;
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

/* Waits only for children created through pcntl_windows_spawn in this request. */
static zend_long pcntl_wait_impl(zend_long pid, zval *status, zend_long flags, zval *usage)
{
    pcntl_win_child *node, *selected = NULL, **link;
    HANDLE handles[MAXIMUM_WAIT_OBJECTS];
    pcntl_win_child *nodes[MAXIMUM_WAIT_OBJECTS];
    DWORD n = 0, result, exit_code;

    if ((flags & ~PCNTL_WINDOWS_WNOHANG) != 0 || pid == 0 || pid < -1) {
        PCNTL_G(last_error) = EINVAL;
        return -1;
    }
    for (node = PCNTL_G(children); node; node = node->next) {
        if (pid == -1 || pid == (zend_long) node->pid) {
            handles[n] = node->handle;
            nodes[n] = node;
            n++;
        }
    }
    if (!n) {
        PCNTL_G(last_error) = ECHILD;
        return -1;
    }
    result = WaitForMultipleObjects(n, handles, FALSE, (flags & PCNTL_WINDOWS_WNOHANG) ? 0 : INFINITE);
    if (result == WAIT_TIMEOUT) return 0;
    if (result == WAIT_FAILED || result >= WAIT_OBJECT_0 + n) {
        PCNTL_G(last_error) = pcntl_win32_errno(GetLastError());
        return -1;
    }
    selected = nodes[result - WAIT_OBJECT_0];
    if (!GetExitCodeProcess(selected->handle, &exit_code)) {
        PCNTL_G(last_error) = pcntl_win32_errno(GetLastError());
        return -1;
    }
    ZVAL_DEREF(status);
    zval_ptr_dtor(status);
    ZVAL_LONG(status, (zend_long) ((exit_code & 0xff) << 8));
    if (usage) {
        ZVAL_DEREF(usage);
        zval_ptr_dtor(usage);
        array_init(usage); /* No Win32 rusage translation yet. */
    }
    pid = (zend_long) selected->pid;
    for (link = &PCNTL_G(children); *link; link = &(*link)->next) {
        if (*link == selected) {
            *link = selected->next;
            break;
        }
    }
    CloseHandle(selected->handle);
    efree(selected);
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

static const zend_function_entry pcntl_functions[] = {
    PHP_FE(pcntl_windows_spawn, arginfo_pcntl_windows_spawn)
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
    REGISTER_LONG_CONSTANT("WNOHANG", PCNTL_WINDOWS_WNOHANG, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("PCNTL_ECHILD", ECHILD, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("PCNTL_EINVAL", EINVAL, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("PCNTL_ENOENT", ENOENT, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("PCNTL_EACCES", EACCES, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("PCNTL_ENOMEM", ENOMEM, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("PCNTL_EAGAIN", EAGAIN, CONST_CS | CONST_PERSISTENT);
    return SUCCESS;
}
PHP_RINIT_FUNCTION(pcntl)
{
#if defined(ZTS) && defined(COMPILE_DL_PCNTL)
    ZEND_TSRMLS_CACHE_UPDATE();
#endif
    PCNTL_G(children) = NULL;
    PCNTL_G(last_error) = 0;
    return SUCCESS;
}
PHP_RSHUTDOWN_FUNCTION(pcntl)
{
    pcntl_win_child *child = PCNTL_G(children), *next;
    while (child) {
        next = child->next;
        CloseHandle(child->handle); /* Detach; do not kill running children. */
        efree(child);
        child = next;
    }
    PCNTL_G(children) = NULL;
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
    NULL,
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
