/**
 *  syslogger.c
 * 
 *  DESCRIPTION: See syslogger.h for description
 * 
 *  https://github.com/oresat/oresat-dxwifi-software
 * 
 */

#include <syslog.h>

#include <libdxwifi/details/syslogger.h>


static inline int dxwifi_log_level_to_syslog(dxwifi_log_level_t log_level) {
    switch (log_level)
    {
    case DXWIFI_LOG_FATAL:
        return LOG_EMERG;
    case DXWIFI_LOG_ERROR:
        return LOG_ALERT;
    case DXWIFI_LOG_WARN:
        return LOG_CRIT;
    case DXWIFI_LOG_INFO:
        return LOG_NOTICE;
    case DXWIFI_LOG_DEBUG:
        return LOG_INFO;
    case DXWIFI_LOG_TRACE:
    default: // Intentional fallthrough
        return LOG_DEBUG;
    }
}


void init_syslogger() {
    openlog("dxwifi", LOG_CONS | LOG_PID, LOG_USER);
}


void syslogger(dxwifi_log_module_t module, dxwifi_log_level_t log_level, const char* fmt, va_list args) {
    __DXWIFI_UTILS_UNUSED(module);

    int priority = LOG_MAKEPRI(LOG_USER, dxwifi_log_level_to_syslog(log_level));

    vsyslog(priority, fmt, args);
}


void close_syslogger() {
    closelog();
}
