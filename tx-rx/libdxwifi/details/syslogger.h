/**
 *  syslogger.h
 * 
 *  DESCRIPTION: GNU syslog adapter for the DxWiFi logging module
 * 
 *  https://github.com/oresat/oresat-dxwifi-software
 * 
 */

#ifndef LIBDXWIFI_SYSLOGGER_H
#define LIBDXWIFI_SYSLOGGER_H

#include <syslog.h>

#include <libdxwifi/details/logging.h>


/**
 *  DESCRIPTION:    Converts dxwifi log level to syslog priority level
 * 
 *  ARGUMENTS:
 *      log_lvel:   Log level of the message
 * 
 *  RETURNS:
 *      int:        SysLog priority mask
 * 
 */
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


/**
 *  DESCRIPTION:    Syslog adapter for the DxWiFI logging facade. See logging.h
 *                  for description of arguments
 * 
 */
void syslogger(dxwifi_log_module_t module, dxwifi_log_level_t log_level, const char* fmt, va_list args) {
    __DXWIFI_UTILS_UNUSED(module);

    openlog("dxwifi", LOG_CONS | LOG_PID, LOG_USER);

    int priority = LOG_MAKEPRI(LOG_USER, dxwifi_log_level_to_syslog(log_level));

    vsyslog(priority, fmt, args);

    closelog();
}


#endif // LIBDXWIFI_SYSLOGGER_H

