/**
 *  logging.h
 * 
 *  DESCRIPTION: DxWiFi Logging API Facade
 * 
 *  This logging facade supports formatted strings as well as user, module, and 
 *  compiler log levels. Logging under the compiler log level gets culled out 
 *  and won't be present in release builds. By default logging is disabled as it 
 *  has no concrete implementation. To enable logging you will need to provide a 
 *  logger function that fullfills the dxwifi_logger interface and bridge it to
 *  your own logging library.
 * 
 *  https://github.com/oresat/oresat-dxwifi-software
 * 
 */

#ifndef LIBDXWIFI_LOGGING_H
#define LIBDXWIFI_LOGGING_H

#include <stdint.h>
#include <stdbool.h>

#include <libdxwifi/details/utils.h>

typedef enum {
    DXWIFI_LOG_OFF      = 0,
    DXWIFI_LOG_FATAL    = 1,
    DXWIFI_LOG_ERROR    = 2,
    DXWIFI_LOG_WARN     = 3,
    DXWIFI_LOG_INFO     = 4,
    DXWIFI_LOG_DEBUG    = 5,
    DXWIFI_LOG_TRACE    = 6
} dxwifi_log_level_t;

// If you want module specific logging add it here and update file_to_log_module(). 
// Otherwise log statements  will get grouped into the generic sink. 
typedef enum {
    DXWIFI_LOG_GENERIC      = 0,
    DXWIFI_LOG_TRANSMITTER  = 1,

    // Add new modules here

    DXWIFI_LOG_MODULE_COUNT,
    DXWIFI_LOG_ALL_MODULES 
} dxwifi_log_module_t;


typedef void(*dxwifi_logger)(dxwifi_log_level_t, dxwifi_log_module_t, const char* fmt, va_list args);

void init_logging();

const char* log_level_to_str(dxwifi_log_level_t level);

dxwifi_log_module_t file_to_log_module(const char* file_name);

bool set_logger(dxwifi_log_module_t module, dxwifi_logger logger);

bool set_log_level(dxwifi_log_module_t module, dxwifi_log_level_t level);


#if defined(LIBDXWIFI_DISABLE_LOGGING)
    #define DXWIFI_LOG_LEVEL 0
#elif defined(NDEBUG)
    #define DXWIFI_LOG_LEVEL 4
#else 
    #define DXWIFI_LOG_LEVEL 6
#endif


#if DXWIFI_LOG_LEVEL < 1
  #define log_fatal(fmt, ...) __DXWIFI_UTILS_UNUSED(fmt, ##__VA_ARGS__)
#else
  #define log_fatal(fmt, ...) __log(DXWIFI_LOG_FATAL, __FILE__, fmt, ##__VA_ARGS__)
#endif

#if DXWIFI_LOG_LEVEL < 2
  #define log_error(fmt, ...) __DXWIFI_UTILS_UNUSED(fmt, ##__VA_ARGS__)
#else
  #define log_error(fmt, ...) __log(DXWIFI_LOG_ERROR, __FILE__, fmt, ##__VA_ARGS__)
#endif

#if DXWIFI_LOG_LEVEL < 3
  #define log_warning(fmt, ...) __DXWIFI_UTILS_UNUSED(fmt, ##__VA_ARGS__)
#else
  #define log_warning(fmt, ...) __log(DXWIFI_LOG_WARN, __FILE__, fmt, ##__VA_ARGS__)
#endif

#if DXWIFI_LOG_LEVEL < 4
  #define log_info(fmt, ...) __DXWIFI_UTILS_UNUSED(fmt, ##__VA_ARGS__)
#else
  #define log_info(fmt, ...) __log(DXWIFI_LOG_INFO, __FILE__, fmt, ##__VA_ARGS__)
#endif

#if DXWIFI_LOG_LEVEL < 5
  #define log_debug(fmt, ...) __DXWIFI_UTILS_UNUSED(fmt, ##__VA_ARGS__)
#else
  #define log_debug(fmt, ...) __log(DXWIFI_LOG_DEBUG, __FILE__, fmt, ##__VA_ARGS__)
#endif

// Hexdump is expensive, so it's only enabled for trace logging
#if DXWIFI_LOG_LEVEL < 6
  #define log_trace(fmt, ...) __DXWIFI_UTILS_UNUSED(fmt, ##__VA_ARGS__)
  #define log_hexdump(data, size) __DXWIFI_UTILS_UNUSED(data, size)
#else
  #define log_trace(fmt, ...) __log(DXWIFI_LOG_TRACE, __FILE__, fmt, ##__VA_ARGS__)
  #define log_hexdump(data, size) __log_hexdump(__FILE__, const uint8_t* data, int size);
#endif


void __log(dxwifi_log_level_t log_level, const char* file, const char* fmt, ...);
void __log_hexdump(const char* file, const uint8_t* data, int size);


#endif // LIBDXWIFI_LOGGING_H
