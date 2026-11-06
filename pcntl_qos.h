/* Windows scheduler analogue of Darwin PCNTL QoS classes.
 * Mapping is intentionally approximate; Windows does not implement Apple QoS. */
static zend_class_entry *pcntl_qos_ce;

static zend_result pcntl_register_qos(void)
{
    pcntl_qos_ce = zend_register_internal_enum("Pcntl\\QosClass", IS_UNDEF, NULL);
    if (!pcntl_qos_ce) return FAILURE;
    zend_enum_add_case_cstr(pcntl_qos_ce, "UserInteractive", NULL);
    zend_enum_add_case_cstr(pcntl_qos_ce, "UserInitiated", NULL);
    zend_enum_add_case_cstr(pcntl_qos_ce, "Default", NULL);
    zend_enum_add_case_cstr(pcntl_qos_ce, "Utility", NULL);
    zend_enum_add_case_cstr(pcntl_qos_ce, "Background", NULL);
    return SUCCESS;
}

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_pcntl_getqos_class, 0, 0, Pcntl\\QosClass, 0)
ZEND_END_ARG_INFO()
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_pcntl_setqos_class, 0, 0, IS_VOID, 0)
    ZEND_ARG_OBJ_INFO_WITH_DEFAULT_VALUE(0, qos_class, Pcntl\\QosClass, 0, "Pcntl\\QosClass::Default")
ZEND_END_ARG_INFO()

PHP_FUNCTION(pcntl_getqos_class)
{
    int priority;
    const char *name;
    ZEND_PARSE_PARAMETERS_NONE();
    priority = GetThreadPriority(GetCurrentThread());
    if (priority == THREAD_PRIORITY_ERROR_RETURN) {
        PCNTL_G(last_error) = pcntl_win32_errno(GetLastError());
        zend_throw_error(NULL, "GetThreadPriority failed");
        RETURN_THROWS();
    }
    if (priority >= THREAD_PRIORITY_HIGHEST) name = "UserInteractive";
    else if (priority >= THREAD_PRIORITY_ABOVE_NORMAL) name = "UserInitiated";
    else if (priority >= THREAD_PRIORITY_NORMAL) name = "Default";
    else if (priority >= THREAD_PRIORITY_BELOW_NORMAL) name = "Utility";
    else name = "Background";
    PCNTL_G(last_error) = 0;
    RETURN_OBJ_COPY(zend_enum_get_case_cstr(pcntl_qos_ce, name));
}

PHP_FUNCTION(pcntl_setqos_class)
{
    zval *qos = NULL;
    int priority = THREAD_PRIORITY_NORMAL;
    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_OBJECT_OF_CLASS(qos, pcntl_qos_ce)
    ZEND_PARSE_PARAMETERS_END();
    if (qos) {
        zend_object *obj = Z_OBJ_P(qos);
        if (obj == zend_enum_get_case_cstr(pcntl_qos_ce, "UserInteractive")) {
            priority = THREAD_PRIORITY_HIGHEST;
        } else if (obj == zend_enum_get_case_cstr(pcntl_qos_ce, "UserInitiated")) {
            priority = THREAD_PRIORITY_ABOVE_NORMAL;
        } else if (obj == zend_enum_get_case_cstr(pcntl_qos_ce, "Utility")) {
            priority = THREAD_PRIORITY_BELOW_NORMAL;
        } else if (obj == zend_enum_get_case_cstr(pcntl_qos_ce, "Background")) {
            priority = THREAD_PRIORITY_LOWEST;
        }
    }
    if (!SetThreadPriority(GetCurrentThread(), priority)) {
        PCNTL_G(last_error) = pcntl_win32_errno(GetLastError());
        zend_throw_error(NULL, "SetThreadPriority failed");
        RETURN_THROWS();
    }
    PCNTL_G(last_error) = 0;
}
