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

#include <libdxwifi/details/logging.h>


void init_syslogger();

void syslogger(dxwifi_log_module_t module, dxwifi_log_level_t log_level, const char* fmt, va_list args);

void close_syslogger();


#endif // LIBDXWIFI_SYSLOGGER_H

