/* PCNTL scheduling functions backed by Win32. Included in pcntl.c. */
#define PCNTL_PRIO_PROCESS 0
#define PCNTL_PRIO_PGRP 1
#define PCNTL_PRIO_USER 2

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_pcntl_getpriority, 0, 0, MAY_BE_LONG | MAY_BE_FALSE)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, process_id, IS_LONG, 1, "null")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, mode, IS_LONG, 0, "PRIO_PROCESS")
ZEND_END_ARG_INFO()
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_pcntl_setpriority, 0, 1, _IS_BOOL, 0)
    ZEND_ARG_TYPE_INFO(0, priority, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, process_id, IS_LONG, 1, "null")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, mode, IS_LONG, 0, "PRIO_PROCESS")
ZEND_END_ARG_INFO()
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_pcntl_getcpuaffinity, 0, 0, MAY_BE_ARRAY | MAY_BE_FALSE)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, process_id, IS_LONG, 1, "null")
ZEND_END_ARG_INFO()
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_pcntl_setcpuaffinity, 0, 0, _IS_BOOL, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, process_id, IS_LONG, 1, "null")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, cpu_ids, IS_ARRAY, 0, "[]")
ZEND_END_ARG_INFO()
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_pcntl_getcpu, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

static HANDLE pcntl_open_process(zend_long pid, bool is_null, DWORD rights, bool *close_needed)
{
    HANDLE h;
    *close_needed = false;
    if (is_null || pid == 0 || pid == (zend_long) GetCurrentProcessId()) {
        return GetCurrentProcess();
    }
    if (pid < 0 || (uint64_t) pid > UINT32_MAX) {
        PCNTL_G(last_error) = EINVAL;
        return NULL;
    }
    h = OpenProcess(rights, FALSE, (DWORD) pid);
    if (!h) {
        PCNTL_G(last_error) = pcntl_win32_errno(GetLastError());
        return NULL;
    }
    *close_needed = true;
    return h;
}
PHP_FUNCTION(pcntl_getpriority)
{
    zend_long pid = 0, mode = PCNTL_PRIO_PROCESS, mapped;
    bool null_pid = true, close_needed;
    HANDLE h;
    DWORD cls;
    ZEND_PARSE_PARAMETERS_START(0, 2)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG_OR_NULL(pid, null_pid)
        Z_PARAM_LONG(mode)
    ZEND_PARSE_PARAMETERS_END();
    if (mode != PCNTL_PRIO_PROCESS) {
        PCNTL_G(last_error) = EINVAL;
        RETURN_FALSE;
    }
    h = pcntl_open_process(pid, null_pid, PROCESS_QUERY_LIMITED_INFORMATION, &close_needed);
    if (!h) RETURN_FALSE;
    cls = GetPriorityClass(h);
    if (!cls) PCNTL_G(last_error) = pcntl_win32_errno(GetLastError());
    if (close_needed) CloseHandle(h);
    if (!cls) RETURN_FALSE;
    switch (cls) {
        case IDLE_PRIORITY_CLASS: mapped = 19; break;
        case BELOW_NORMAL_PRIORITY_CLASS: mapped = 10; break;
        case NORMAL_PRIORITY_CLASS: mapped = 0; break;
        case ABOVE_NORMAL_PRIORITY_CLASS: mapped = -5; break;
        case HIGH_PRIORITY_CLASS: mapped = -15; break;
        case REALTIME_PRIORITY_CLASS: mapped = -20; break;
        default: PCNTL_G(last_error) = EINVAL; RETURN_FALSE;
    }
    PCNTL_G(last_error) = 0;
    RETURN_LONG(mapped);
}
PHP_FUNCTION(pcntl_setpriority)
{
    zend_long priority, pid = 0, mode = PCNTL_PRIO_PROCESS;
    bool null_pid = true, close_needed;
    HANDLE h;
    BOOL ok;
    DWORD cls;
    ZEND_PARSE_PARAMETERS_START(1, 3)
        Z_PARAM_LONG(priority)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG_OR_NULL(pid, null_pid)
        Z_PARAM_LONG(mode)
    ZEND_PARSE_PARAMETERS_END();
    if (priority < -20 || priority > 19) {
        zend_argument_value_error(1, "must be between -20 and 19");
        RETURN_THROWS();
    }
    if (mode != PCNTL_PRIO_PROCESS) {
        PCNTL_G(last_error) = EINVAL;
        RETURN_FALSE;
    }
    /* Never set REALTIME_PRIORITY_CLASS, which can starve the host. */
    cls = priority <= -15 ? HIGH_PRIORITY_CLASS :
          priority <= -5 ? ABOVE_NORMAL_PRIORITY_CLASS :
          priority <= 4 ? NORMAL_PRIORITY_CLASS :
          priority <= 12 ? BELOW_NORMAL_PRIORITY_CLASS : IDLE_PRIORITY_CLASS;
    h = pcntl_open_process(pid, null_pid, PROCESS_SET_INFORMATION, &close_needed);
    if (!h) RETURN_FALSE;
    ok = SetPriorityClass(h, cls);
    PCNTL_G(last_error) = ok ? 0 : pcntl_win32_errno(GetLastError());
    if (close_needed) CloseHandle(h);
    RETURN_BOOL(ok);
}
PHP_FUNCTION(pcntl_getcpuaffinity)
{
    zend_long pid = 0;
    bool null_pid = true, close_needed;
    HANDLE h;
    DWORD_PTR mask, system;
    size_t i;
    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG_OR_NULL(pid, null_pid)
    ZEND_PARSE_PARAMETERS_END();
    h = pcntl_open_process(pid, null_pid, PROCESS_QUERY_LIMITED_INFORMATION, &close_needed);
    if (!h) RETURN_FALSE;
    if (!GetProcessAffinityMask(h, &mask, &system) || !mask) {
        PCNTL_G(last_error) = pcntl_win32_errno(GetLastError());
        if (close_needed) CloseHandle(h);
        RETURN_FALSE;
    }
    if (close_needed) CloseHandle(h);
    array_init(return_value);
    for (i = 0; i < sizeof(DWORD_PTR) * CHAR_BIT; ++i) {
        if (mask & (((DWORD_PTR) 1) << i)) add_next_index_long(return_value, (zend_long) i);
    }
    PCNTL_G(last_error) = 0;
}
PHP_FUNCTION(pcntl_setcpuaffinity)
{
    zend_long pid = 0;
    bool null_pid = true, close_needed;
    HashTable *cpus = NULL;
    zval *cpu;
    HANDLE h;
    DWORD_PTR mask = 0, current, system;
    BOOL ok;
    ZEND_PARSE_PARAMETERS_START(0, 2)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG_OR_NULL(pid, null_pid)
        Z_PARAM_ARRAY_HT(cpus)
    ZEND_PARSE_PARAMETERS_END();
    if (!cpus || !zend_hash_num_elements(cpus)) {
        zend_argument_value_error(2, "must not be empty");
        RETURN_THROWS();
    }
    ZEND_HASH_FOREACH_VAL(cpus, cpu) {
        zend_long id;
        if (Z_TYPE_P(cpu) != IS_LONG) {
            zend_argument_type_error(2, "must contain only integer CPU ids");
            RETURN_THROWS();
        }
        id = Z_LVAL_P(cpu);
        if (id < 0 || id >= (zend_long) (sizeof(DWORD_PTR) * CHAR_BIT)) {
            zend_argument_value_error(2, "contains an unsupported CPU id");
            RETURN_THROWS();
        }
        mask |= ((DWORD_PTR) 1) << id;
    } ZEND_HASH_FOREACH_END();
    h = pcntl_open_process(pid, null_pid, PROCESS_SET_INFORMATION | PROCESS_QUERY_LIMITED_INFORMATION, &close_needed);
    if (!h) RETURN_FALSE;
    if (!GetProcessAffinityMask(h, &current, &system)) {
        PCNTL_G(last_error) = pcntl_win32_errno(GetLastError());
        if (close_needed) CloseHandle(h);
        RETURN_FALSE;
    }
    if (mask & ~system) {
        PCNTL_G(last_error) = EINVAL;
        if (close_needed) CloseHandle(h);
        RETURN_FALSE;
    }
    ok = SetProcessAffinityMask(h, mask);
    PCNTL_G(last_error) = ok ? 0 : pcntl_win32_errno(GetLastError());
    if (close_needed) CloseHandle(h);
    RETURN_BOOL(ok);
}
PHP_FUNCTION(pcntl_getcpu)
{
    ZEND_PARSE_PARAMETERS_NONE();
    /* Windows CPU ID is group-relative; multiple groups are not modelled. */
    RETURN_LONG((zend_long) GetCurrentProcessorNumber());
}
PHP_FUNCTION(pcntl_wifcontinued)
{
    zend_long status;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(status)
    ZEND_PARSE_PARAMETERS_END();
    RETURN_BOOL(status == 0xffff);
}
