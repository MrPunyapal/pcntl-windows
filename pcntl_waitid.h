/* POSIX-shaped waitid for registered Win32 children; only exited events. */
#define PCNTL_P_ALL 0
#define PCNTL_P_PID 1
#define PCNTL_P_PGID 2
#define PCNTL_WEXITED 4
#define PCNTL_WNOWAIT 0x01000000
#define PCNTL_CLD_EXITED 1
#define PCNTL_SIGCHLD 17

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_pcntl_waitid, 0, 0, _IS_BOOL, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, idtype, IS_LONG, 0, "P_ALL")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, id, IS_LONG, 1, "null")
    ZEND_ARG_INFO_WITH_DEFAULT_VALUE(1, info, "[]")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, flags, IS_LONG, 0, "WEXITED")
    ZEND_ARG_INFO_WITH_DEFAULT_VALUE(1, resource_usage, "[]")
ZEND_END_ARG_INFO()

PHP_FUNCTION(pcntl_waitid)
{
    zend_long idtype = PCNTL_P_ALL, id = 0, flags = PCNTL_WEXITED;
    bool null_id = true;
    zval *info = NULL, *usage = NULL;
    pcntl_win_child *node, *selected = NULL, **link;
    DWORD result, code;
    ZEND_PARSE_PARAMETERS_START(0, 5)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(idtype)
        Z_PARAM_LONG_OR_NULL(id, null_id)
        Z_PARAM_ZVAL(info)
        Z_PARAM_LONG(flags)
        Z_PARAM_ZVAL(usage)
    ZEND_PARSE_PARAMETERS_END();
    if ((idtype != PCNTL_P_ALL && idtype != PCNTL_P_PID && idtype != PCNTL_P_PGID)
        || (idtype == PCNTL_P_PID && (null_id || id <= 0 || (uint64_t) id > UINT32_MAX))
        || (idtype == PCNTL_P_PGID && (id < 0 || (uint64_t) id > UINT32_MAX))
        || !(flags & PCNTL_WEXITED)
        || (flags & ~(PCNTL_WEXITED | PCNTL_WNOWAIT | PCNTL_WINDOWS_WNOHANG))) {
        PCNTL_G(last_error) = EINVAL;
        RETURN_FALSE;
    }
    for (;;) {
        bool found = false;
        for (node = PCNTL_G(children); node; node = node->next) {
            if (idtype == PCNTL_P_PID && id != (zend_long) node->pid) continue;
            if (idtype == PCNTL_P_PGID && id != (zend_long) node->group_id) continue;
            found = true;
            result = WaitForSingleObject(node->handle, 0);
            if (result == WAIT_OBJECT_0) { selected = node; break; }
            if (result != WAIT_TIMEOUT) {
                PCNTL_G(last_error) = pcntl_win32_errno(GetLastError());
                RETURN_FALSE;
            }
        }
        if (!found) {
            PCNTL_G(last_error) = ECHILD;
            RETURN_FALSE;
        }
        if (selected || (flags & PCNTL_WINDOWS_WNOHANG)) break;
        if (!pcntl_win_wait_for_child_event()) RETURN_FALSE;
    }
    if (!selected) {
        if (info) {
            ZVAL_DEREF(info);
            zval_ptr_dtor(info);
            array_init(info);
        }
        if (usage) {
            ZVAL_DEREF(usage);
            zval_ptr_dtor(usage);
            array_init(usage);
        }
        PCNTL_G(last_error) = 0;
        RETURN_TRUE;
    }
    if (!GetExitCodeProcess(selected->handle, &code)) {
        PCNTL_G(last_error) = pcntl_win32_errno(GetLastError());
        RETURN_FALSE;
    }
    pcntl_win_emit_sigchld(selected);
    if (info) {
        ZVAL_DEREF(info);
        zval_ptr_dtor(info);
        array_init(info);
        /* No native POSIX SIGCHLD; exported value is a compatibility marker. */
        add_assoc_long(info, "signo", PCNTL_SIGCHLD);
        add_assoc_long(info, "errno", 0);
        add_assoc_long(info, "code", PCNTL_CLD_EXITED);
        add_assoc_long(info, "status", (zend_long) (code & 0xff));
        add_assoc_long(info, "pid", (zend_long) selected->pid);
    }
    if (usage) pcntl_win_rusage(selected->handle, usage);
    if (!(flags & PCNTL_WNOWAIT)) {
        for (link = &PCNTL_G(children); *link; link = &(*link)->next) {
            if (*link == selected) {
                *link = selected->next;
                break;
            }
        }
        pcntl_win_release_child(selected);
    }
    PCNTL_G(last_error) = 0;
    RETURN_TRUE;
}
