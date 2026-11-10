/* CLI-only cooperative Windows signal subsystem.
 * All PHP callbacks run from pcntl_signal_dispatch on the PHP thread. */
#define PCNTL_SIG_DFL 0
#define PCNTL_SIG_IGN 1
#define PCNTL_SIGHUP 1
#define PCNTL_SIGINT 2
#define PCNTL_SIGUSR1 10
#define PCNTL_SIGUSR2 12
#define PCNTL_SIGALRM 14
#define PCNTL_SIGTERM 15
#define PCNTL_SIG_BLOCK 0
#define PCNTL_SIG_UNBLOCK 1
#define PCNTL_SIG_SETMASK 2

static volatile LONG pcntl_pending[32];
/* Process lifetime event, shared by the single permitted active PHP request. */
static HANDLE pcntl_signal_ready_event = NULL;
static volatile LONG pcntl_console_enabled = 0;
static bool pcntl_console_installed = false;
static volatile LONG pcntl_async_enabled = 0;
static SRWLOCK pcntl_vm_lock = SRWLOCK_INIT;
static zend_atomic_bool *pcntl_vm_target = NULL;
static void (*pcntl_previous_interrupt)(zend_execute_data *) = NULL;

static void pcntl_win_enqueue(int signum)
{
    InterlockedIncrement(&pcntl_pending[signum]);
    if (pcntl_signal_ready_event) SetEvent(pcntl_signal_ready_event);
    if (InterlockedCompareExchange(&pcntl_async_enabled, 0, 0)) {
        AcquireSRWLockShared(&pcntl_vm_lock);
        if (pcntl_vm_target) zend_atomic_bool_store_ex(pcntl_vm_target, true);
        ReleaseSRWLockShared(&pcntl_vm_lock);
    }
}

static void pcntl_win_schedule_pending_signals(void)
{
    int signum;
    if (!PCNTL_G(async_signals)) return;
    for (signum = 1; signum < 32; ++signum) {
        if (!(PCNTL_G(signal_mask) & (((DWORD) 1) << signum))
            && InterlockedCompareExchange(&pcntl_pending[signum], 0, 0) > 0) {
            zend_atomic_bool_store_ex(&EG(vm_interrupt), true);
            break;
        }
    }
}


static bool pcntl_signal_supported(zend_long signum)
{
    return signum == PCNTL_SIGHUP || signum == PCNTL_SIGINT ||
        signum == PCNTL_SIGUSR1 || signum == PCNTL_SIGUSR2 ||
        signum == PCNTL_SIGALRM || signum == PCNTL_SIGTERM ||
        signum == PCNTL_SIGCHLD;
}

static BOOL WINAPI pcntl_win_console_handler(DWORD control)
{
    LONG enabled = InterlockedCompareExchange(&pcntl_console_enabled, 0, 0);
    if (control == CTRL_C_EVENT && (enabled & 1)) {
        pcntl_win_enqueue(PCNTL_SIGINT);
        return TRUE;
    }
    if (control == CTRL_BREAK_EVENT && (enabled & 2)) {
        pcntl_win_enqueue(PCNTL_SIGTERM);
        return TRUE;
    }
    return FALSE;
}

static VOID CALLBACK pcntl_win_alarm_callback(PVOID context, BOOLEAN fired)
{
    (void) context;
    (void) fired;
    pcntl_win_enqueue(PCNTL_SIGALRM);
}

static void pcntl_win_signal_request_init(void)
{
    int i;
    for (i = 0; i < 32; ++i) {
        ZVAL_UNDEF(&PCNTL_G(signal_handlers)[i]);
        InterlockedExchange(&pcntl_pending[i], 0);
    }
    if (pcntl_signal_ready_event) ResetEvent(pcntl_signal_ready_event);
    PCNTL_G(signal_mask) = 0;
    PCNTL_G(async_signals) = false;
    PCNTL_G(dispatching_signals) = false;
    AcquireSRWLockExclusive(&pcntl_vm_lock);
    pcntl_vm_target = &EG(vm_interrupt);
    ReleaseSRWLockExclusive(&pcntl_vm_lock);
    PCNTL_G(alarm_timer) = NULL;
    PCNTL_G(alarm_deadline) = 0;
}

static void pcntl_win_signal_request_shutdown(void)
{
    int i;
    InterlockedExchange(&pcntl_async_enabled, 0);
    AcquireSRWLockExclusive(&pcntl_vm_lock);
    if (pcntl_vm_target == &EG(vm_interrupt)) pcntl_vm_target = NULL;
    ReleaseSRWLockExclusive(&pcntl_vm_lock);
    if (PCNTL_G(alarm_timer)) {
        DeleteTimerQueueTimer(NULL, PCNTL_G(alarm_timer), INVALID_HANDLE_VALUE);
        PCNTL_G(alarm_timer) = NULL;
    }
    InterlockedExchange(&pcntl_console_enabled, 0);
    for (i = 0; i < 32; ++i) {
        if (!Z_ISUNDEF(PCNTL_G(signal_handlers)[i])) {
            zval_ptr_dtor(&PCNTL_G(signal_handlers)[i]);
            ZVAL_UNDEF(&PCNTL_G(signal_handlers)[i]);
        }
        InterlockedExchange(&pcntl_pending[i], 0);
    }
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_pcntl_signal, 0, 2, _IS_BOOL, 0)
    ZEND_ARG_TYPE_INFO(0, signal, IS_LONG, 0)
    ZEND_ARG_INFO(0, handler)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, restart_syscalls, _IS_BOOL, 0, "true")
ZEND_END_ARG_INFO()
ZEND_BEGIN_ARG_INFO_EX(arginfo_pcntl_signal_get_handler, 0, 0, 1)
    ZEND_ARG_TYPE_INFO(0, signal, IS_LONG, 0)
ZEND_END_ARG_INFO()
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_pcntl_signal_dispatch, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_pcntl_async_signals, 0, 0, _IS_BOOL, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, enable, _IS_BOOL, 1, "null")
ZEND_END_ARG_INFO()
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_pcntl_alarm, 0, 1, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, seconds, IS_LONG, 0)
ZEND_END_ARG_INFO()
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_pcntl_sigprocmask, 0, 2, _IS_BOOL, 0)
    ZEND_ARG_TYPE_INFO(0, mode, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, signals, IS_ARRAY, 0)
    ZEND_ARG_INFO_WITH_DEFAULT_VALUE(1, old_signals, "null")
ZEND_END_ARG_INFO()
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_pcntl_windows_raise, 0, 1, _IS_BOOL, 0)
    ZEND_ARG_TYPE_INFO(0, signal, IS_LONG, 0)
ZEND_END_ARG_INFO()

PHP_FUNCTION(pcntl_signal)
{
    zend_long signum;
    zval *handler, *slot;
    bool restart = true;
    ZEND_PARSE_PARAMETERS_START(2, 3)
        Z_PARAM_LONG(signum)
        Z_PARAM_ZVAL(handler)
        Z_PARAM_OPTIONAL
        Z_PARAM_BOOL(restart)
    ZEND_PARSE_PARAMETERS_END();
    (void) restart; /* Windows console controls cannot restart POSIX syscalls. */
    if (!pcntl_signal_supported(signum)) {
        PCNTL_G(last_error) = EINVAL;
        RETURN_FALSE;
    }
    if (Z_TYPE_P(handler) == IS_LONG) {
        if (Z_LVAL_P(handler) != PCNTL_SIG_DFL && Z_LVAL_P(handler) != PCNTL_SIG_IGN) {
            zend_argument_value_error(2, "must be SIG_DFL, SIG_IGN, or a callable");
            RETURN_THROWS();
        }
    } else if (!zend_is_callable(handler, 0, NULL)) {
        zend_argument_type_error(2, "must be SIG_DFL, SIG_IGN, or a callable");
        RETURN_THROWS();
    }
    slot = &PCNTL_G(signal_handlers)[signum];
    if (!Z_ISUNDEF_P(slot)) zval_ptr_dtor(slot);
    ZVAL_COPY(slot, handler);
    if (signum == PCNTL_SIGINT || signum == PCNTL_SIGTERM) {
        LONG bit = signum == PCNTL_SIGINT ? 1 : 2;
        if (Z_TYPE_P(handler) == IS_LONG && Z_LVAL_P(handler) == PCNTL_SIG_DFL) {
            InterlockedAnd(&pcntl_console_enabled, ~bit);
        } else {
            if (!pcntl_console_installed) {
                PCNTL_G(last_error) = ENOSYS;
                RETURN_FALSE;
            }
            InterlockedOr(&pcntl_console_enabled, bit);
        }
    }
    PCNTL_G(last_error) = 0;
    RETURN_TRUE;
}

PHP_FUNCTION(pcntl_signal_get_handler)
{
    zend_long signum;
    zval *handler;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(signum)
    ZEND_PARSE_PARAMETERS_END();
    if (!pcntl_signal_supported(signum)) {
        zend_argument_value_error(1, "is not a supported Windows signal");
        RETURN_THROWS();
    }
    handler = &PCNTL_G(signal_handlers)[signum];
    if (Z_ISUNDEF_P(handler)) RETURN_LONG(PCNTL_SIG_DFL);
    RETURN_COPY(handler);
}

PHP_FUNCTION(pcntl_windows_raise)
{
    zend_long signum;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(signum)
    ZEND_PARSE_PARAMETERS_END();
    if (!pcntl_signal_supported(signum)) {
        PCNTL_G(last_error) = EINVAL;
        RETURN_FALSE;
    }
    /* This is a cooperative intra-process event, not POSIX kill(). */
    pcntl_win_enqueue((int) signum);
    PCNTL_G(last_error) = 0;
    RETURN_TRUE;
}

static void pcntl_win_dispatch_signals(void)
{
    int signum;
    if (PCNTL_G(dispatching_signals)) return;
    PCNTL_G(dispatching_signals) = true;
    for (signum = 1; signum < 32; ++signum) {
        LONG i, count;
        zval *handler = &PCNTL_G(signal_handlers)[signum];
        zval fn;
        if (PCNTL_G(signal_mask) & (((DWORD) 1) << signum)) continue;
        count = InterlockedExchange(&pcntl_pending[signum], 0);
        if (!count || Z_ISUNDEF_P(handler) || Z_TYPE_P(handler) == IS_LONG) continue;
        /* Keep the original handler alive even if it changes itself. */
        ZVAL_COPY(&fn, handler);
        for (i = 0; i < count; ++i) {
            zval parameter, retval;
            ZVAL_LONG(&parameter, signum);
            ZVAL_UNDEF(&retval);
            call_user_function(EG(function_table), NULL, &fn, &retval, 1, &parameter);
            if (!Z_ISUNDEF(retval)) zval_ptr_dtor(&retval);
            if (EG(exception)) break;
        }
        zval_ptr_dtor(&fn);
        if (EG(exception)) break;
    }
    PCNTL_G(dispatching_signals) = false;
    /* A nested VM interrupt may have been suppressed by the dispatch guard.
     * Preserve async progress for signals queued by a running PHP handler. */
    pcntl_win_schedule_pending_signals();
}

static void pcntl_win_interrupt_handler(zend_execute_data *execute_data)
{
    if (PCNTL_G(async_signals)) pcntl_win_dispatch_signals();
    if (pcntl_previous_interrupt) pcntl_previous_interrupt(execute_data);
}

PHP_FUNCTION(pcntl_signal_dispatch)
{
    ZEND_PARSE_PARAMETERS_NONE();
    pcntl_win_dispatch_signals();
    if (EG(exception)) RETURN_THROWS();
    RETURN_TRUE;
}

PHP_FUNCTION(pcntl_async_signals)
{
    bool enabled = false, is_null = true;
    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_BOOL_OR_NULL(enabled, is_null)
    ZEND_PARSE_PARAMETERS_END();
    if (is_null) RETURN_BOOL(PCNTL_G(async_signals));
    RETVAL_BOOL(PCNTL_G(async_signals));
    PCNTL_G(async_signals) = enabled;
    InterlockedExchange(&pcntl_async_enabled, enabled ? 1 : 0);
    pcntl_win_schedule_pending_signals();
}

PHP_FUNCTION(pcntl_alarm)
{
    zend_long seconds;
    zend_long remaining = 0;
    ULONGLONG now;
    HANDLE timer = NULL;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(seconds)
    ZEND_PARSE_PARAMETERS_END();
    if (seconds < 0 || seconds > 4294967) {
        zend_argument_value_error(1, "must be between 0 and 4294967");
        RETURN_THROWS();
    }
    now = GetTickCount64();
    if (PCNTL_G(alarm_deadline) > now) {
        remaining = (zend_long) ((PCNTL_G(alarm_deadline) - now + 999) / 1000);
    }
    if (PCNTL_G(alarm_timer)) {
        DeleteTimerQueueTimer(NULL, PCNTL_G(alarm_timer), INVALID_HANDLE_VALUE);
        PCNTL_G(alarm_timer) = NULL;
    }
    PCNTL_G(alarm_deadline) = 0;
    if (seconds > 0) {
        if (!CreateTimerQueueTimer(&timer, NULL, pcntl_win_alarm_callback, NULL,
                (DWORD) seconds * 1000, 0, WT_EXECUTEDEFAULT)) {
            PCNTL_G(last_error) = pcntl_win32_errno(GetLastError());
            RETURN_LONG(remaining);
        }
        PCNTL_G(alarm_timer) = timer;
        PCNTL_G(alarm_deadline) = now + ((ULONGLONG) seconds * 1000);
    }
    PCNTL_G(last_error) = 0;
    RETURN_LONG(remaining);
}

PHP_FUNCTION(pcntl_sigprocmask)
{
    zend_long mode;
    HashTable *signals;
    zval *old = NULL, *signal;
    DWORD bits = 0, before;
    int i;
    ZEND_PARSE_PARAMETERS_START(2, 3)
        Z_PARAM_LONG(mode)
        Z_PARAM_ARRAY_HT(signals)
        Z_PARAM_OPTIONAL
        Z_PARAM_ZVAL(old)
    ZEND_PARSE_PARAMETERS_END();
    if (mode != PCNTL_SIG_BLOCK && mode != PCNTL_SIG_UNBLOCK && mode != PCNTL_SIG_SETMASK) {
        zend_argument_value_error(1, "must be SIG_BLOCK, SIG_UNBLOCK, or SIG_SETMASK");
        RETURN_THROWS();
    }
    ZEND_HASH_FOREACH_VAL(signals, signal) {
        if (Z_TYPE_P(signal) != IS_LONG || !pcntl_signal_supported(Z_LVAL_P(signal))) {
            zend_argument_value_error(2, "must contain only supported Windows signal numbers");
            RETURN_THROWS();
        }
        bits |= ((DWORD) 1) << Z_LVAL_P(signal);
    } ZEND_HASH_FOREACH_END();
    before = PCNTL_G(signal_mask);
    if (mode == PCNTL_SIG_BLOCK) PCNTL_G(signal_mask) |= bits;
    else if (mode == PCNTL_SIG_UNBLOCK) PCNTL_G(signal_mask) &= ~bits;
    else PCNTL_G(signal_mask) = bits;
    /* An earlier VM interrupt may have observed only blocked events. Request
     * another interrupt when changing the mask makes queued work deliverable. */
    pcntl_win_schedule_pending_signals();
    if (old) {
        old = zend_try_array_init(old);
        if (!old) RETURN_THROWS();
        for (i = 1; i < 32; ++i) {
            if (before & (((DWORD) 1) << i)) add_next_index_long(old, i);
        }
    }
    PCNTL_G(last_error) = 0;
    RETURN_TRUE;
}
