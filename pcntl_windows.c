/*
 * Native Win32 research primitives. This does NOT implement POSIX PCNTL.
 * CreateProcess is not fork, and Windows process status is not waitpid status.
 */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php.h"
#include "ext/standard/info.h"

#ifndef PHP_WIN32
# error "pcntl_windows requires native Windows PHP"
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

#define PHP_PCNTL_WINDOWS_VERSION "0.1.0-dev"

/* Wait for an accessible process. This is NOT POSIX waitpid.
 * Returns null on timeout, false on failure or a Windows DWORD exit code.
 */
PHP_FUNCTION(pcntl_windows_wait_process)
{
    zend_long pid;
    zend_long timeout_ms = -1;
    HANDLE process;
    DWORD result, exit_code;

    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_LONG(pid)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(timeout_ms)
    ZEND_PARSE_PARAMETERS_END();

    if (pid <= 0 || (uint64_t) pid > UINT32_MAX) {
        zend_argument_value_error(1, "must be a valid Windows process ID");
        RETURN_THROWS();
    }

    if (timeout_ms < -1 || (timeout_ms >= 0 && (uint64_t) timeout_ms > (uint64_t) (INFINITE - 1))) {
        zend_argument_value_error(2, "must be -1 or between 0 and 4294967294");
        RETURN_THROWS();
    }

    process = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION,
        FALSE, (DWORD) pid);
    if (process == NULL) {
        RETURN_FALSE;
    }

    result = WaitForSingleObject(process,
        timeout_ms < 0 ? INFINITE : (DWORD) timeout_ms);
    if (result == WAIT_TIMEOUT) {
        CloseHandle(process);
        RETURN_NULL();
    }

    if (result != WAIT_OBJECT_0 || !GetExitCodeProcess(process, &exit_code)) {
        CloseHandle(process);
        RETURN_FALSE;
    }

    CloseHandle(process);
    /* Exit code 259 is valid after a signaled process terminates. */
    RETURN_LONG((zend_long) exit_code);
}

/* Windows processors are identified by processor group and group-local index. */
PHP_FUNCTION(pcntl_windows_processor)
{
    PROCESSOR_NUMBER processor;

    ZEND_PARSE_PARAMETERS_NONE();

    GetCurrentProcessorNumberEx(&processor);
    array_init(return_value);
    add_assoc_long(return_value, "group", (zend_long) processor.Group);
    add_assoc_long(return_value, "number", (zend_long) processor.Number);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_pcntl_windows_wait_process, 0, 1, MAY_BE_LONG|MAY_BE_FALSE|MAY_BE_NULL)
    ZEND_ARG_TYPE_INFO(0, pid, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, timeout_ms, IS_LONG, 0, "-1")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_pcntl_windows_processor, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

static const zend_function_entry pcntl_windows_functions[] = {
    PHP_FE(pcntl_windows_wait_process, arginfo_pcntl_windows_wait_process)
    PHP_FE(pcntl_windows_processor, arginfo_pcntl_windows_processor)
    PHP_FE_END
};

PHP_MINFO_FUNCTION(pcntl_windows)
{
    php_info_print_table_start();
    php_info_print_table_header(2, "pcntl_windows", "experimental");
    php_info_print_table_row(2, "Version", PHP_PCNTL_WINDOWS_VERSION);
    php_info_print_table_row(2, "PCNTL compatible", "no");
    php_info_print_table_end();
}

zend_module_entry pcntl_windows_module_entry = {
    STANDARD_MODULE_HEADER,
    "pcntl_windows",
    pcntl_windows_functions,
    NULL,
    NULL,
    NULL,
    NULL,
    PHP_MINFO(pcntl_windows),
    PHP_PCNTL_WINDOWS_VERSION,
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_PCNTL_WINDOWS
# ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
# endif
ZEND_GET_MODULE(pcntl_windows)
#endif
