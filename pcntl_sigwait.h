/* Cooperative signal waiting, not kernel POSIX sigwait. */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_pcntl_sigwaitinfo, 0, 1, MAY_BE_LONG | MAY_BE_FALSE)
    ZEND_ARG_TYPE_INFO(0, signals, IS_ARRAY, 0)
    ZEND_ARG_INFO_WITH_DEFAULT_VALUE(1, info, "[]")
ZEND_END_ARG_INFO()
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_pcntl_sigtimedwait, 0, 1, MAY_BE_LONG | MAY_BE_FALSE)
    ZEND_ARG_TYPE_INFO(0, signals, IS_ARRAY, 0)
    ZEND_ARG_INFO_WITH_DEFAULT_VALUE(1, info, "[]")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, seconds, IS_LONG, 0, "0")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, nanoseconds, IS_LONG, 0, "0")
ZEND_END_ARG_INFO()

static bool pcntl_win_signal_set(HashTable *signals, DWORD *mask)
{
    zval *value;
    *mask = 0;
    ZEND_HASH_FOREACH_VAL(signals, value) {
        if (Z_TYPE_P(value) != IS_LONG || !pcntl_signal_supported(Z_LVAL_P(value))) {
            zend_argument_value_error(1, "must contain supported Windows signal numbers");
            return false;
        }
        *mask |= ((DWORD) 1) << Z_LVAL_P(value);
    } ZEND_HASH_FOREACH_END();
    if (!*mask) {
        zend_argument_value_error(1, "must not be empty");
        return false;
    }
    return true;
}

static zend_long pcntl_win_signal_take(DWORD mask)
{
    int signum;
    for (signum = 1; signum < 32; signum++) {
        LONG current;
        if (!(mask & (((DWORD) 1) << signum))) continue;
        for (;;) {
            current = InterlockedCompareExchange(&pcntl_pending[signum], 0, 0);
            if (current <= 0) break;
            if (InterlockedCompareExchange(&pcntl_pending[signum], current - 1, current) == current) {
                return signum;
            }
        }
    }
    return -1;
}

static zend_long pcntl_win_wait_signal(DWORD mask, bool timeout, ULONGLONG duration_ms)
{
    ULONGLONG begin = GetTickCount64();
    for (;;) {
        zend_long signum = pcntl_win_signal_take(mask);
        if (signum >= 0) {
            PCNTL_G(last_error) = 0;
            return signum;
        }
        if (timeout && (GetTickCount64() - begin >= duration_ms)) {
            PCNTL_G(last_error) = EAGAIN;
            return -1;
        }
        Sleep(1);
    }
}

static void pcntl_win_signal_info(zval *info, zend_long signum)
{
    if (!info) return;
    ZVAL_DEREF(info);
    zval_ptr_dtor(info);
    array_init(info);
    add_assoc_long(info, "signo", signum);
    add_assoc_long(info, "errno", 0);
    add_assoc_long(info, "code", 0);
    add_assoc_long(info, "pid", (zend_long) GetCurrentProcessId());
}

PHP_FUNCTION(pcntl_sigwaitinfo)
{
    HashTable *signals;
    zval *info = NULL;
    DWORD mask;
    zend_long signum;
    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_ARRAY_HT(signals)
        Z_PARAM_OPTIONAL
        Z_PARAM_ZVAL(info)
    ZEND_PARSE_PARAMETERS_END();
    if (!pcntl_win_signal_set(signals, &mask)) RETURN_THROWS();
    signum = pcntl_win_wait_signal(mask, false, 0);
    pcntl_win_signal_info(info, signum);
    RETURN_LONG(signum);
}

PHP_FUNCTION(pcntl_sigtimedwait)
{
    HashTable *signals;
    zval *info = NULL;
    zend_long seconds = 0, nanoseconds = 0, signum;
    DWORD mask;
    ULONGLONG timeout;
    ZEND_PARSE_PARAMETERS_START(1, 4)
        Z_PARAM_ARRAY_HT(signals)
        Z_PARAM_OPTIONAL
        Z_PARAM_ZVAL(info)
        Z_PARAM_LONG(seconds)
        Z_PARAM_LONG(nanoseconds)
    ZEND_PARSE_PARAMETERS_END();
    if (!pcntl_win_signal_set(signals, &mask)) RETURN_THROWS();
    if (seconds < 0 || seconds > 4294967) {
        zend_argument_value_error(3, "must be between 0 and 4294967");
        RETURN_THROWS();
    }
    if (nanoseconds < 0 || nanoseconds >= 1000000000) {
        zend_argument_value_error(4, "must be between 0 and 999999999");
        RETURN_THROWS();
    }
    timeout = (ULONGLONG) seconds * 1000 + ((ULONGLONG) nanoseconds + 999999) / 1000000;
    signum = pcntl_win_wait_signal(mask, true, timeout);
    if (signum < 0) RETURN_FALSE;
    pcntl_win_signal_info(info, signum);
    RETURN_LONG(signum);
}
