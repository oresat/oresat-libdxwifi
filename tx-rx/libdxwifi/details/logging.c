/**
 *  logging.h
 * 
 *  DESCRIPTION: DxWiFi Logging API Facade implementation. See logging.h for details
 * 
 *  https://github.com/oresat/oresat-dxwifi-software
 * 
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include <libdxwifi/details/utils.h>
#include <libdxwifi/details/assert.h>
#include <libdxwifi/details/logging.h>


typedef struct {
    dxwifi_logger       logger;
    dxwifi_log_level_t  log_level;
} dxwifi_log_handler;


static dxwifi_log_handler handlers[] = {
    { default_logger, DXWIFI_LOG_FATAL },
    { default_logger, DXWIFI_LOG_FATAL },
    { default_logger, DXWIFI_LOG_FATAL },
    { default_logger, DXWIFI_LOG_FATAL },
    { default_logger, DXWIFI_LOG_FATAL },
    { default_logger, DXWIFI_LOG_FATAL },

    // New modules should follow the same format

};
compiler_assert(NELEMS(handlers) == DXWIFI_LOG_MODULE_COUNT, "Handler count must match module count");


// table entry must match the name of the file and index of the enumeration
static const char* file_lookup_tbl[DXWIFI_LOG_MODULE_COUNT] = {
    "generic",
    "transmitter",
    "tx",
    "receiver",
    "rx",
    "dirwatch",

    // Add new modules here

};


void default_logger(dxwifi_log_module_t module, dxwifi_log_level_t log_level, const char* fmt, va_list args) {
    // For now just dump everything to stdout
    fprintf(stderr, "[ %s ][ %s ] : ", log_level_to_str(log_level), log_module_to_str(module));
    vfprintf(stderr, fmt, args);
    printf("\n");
    fflush(stderr);
}


const char* log_level_to_str(dxwifi_log_level_t level) {
    switch (level)
    {
    case DXWIFI_LOG_TRACE:
        return "TRACE";
    case DXWIFI_LOG_DEBUG:
        return "DEBUG";
    case DXWIFI_LOG_INFO:
        return "INFO";
    case DXWIFI_LOG_WARN:
        return "WARN";
    case DXWIFI_LOG_ERROR:
        return "ERROR";
    case DXWIFI_LOG_FATAL:
        return "FATAL";
    default:
        return "UNKNOWN";
    }
}


const char* log_module_to_str(dxwifi_log_module_t module) {
    if(0 <= module && module < DXWIFI_LOG_MODULE_COUNT) {
        return file_lookup_tbl[module];
    }
    return file_lookup_tbl[0];
}


dxwifi_log_module_t file_to_log_module(const char* file_name) {

    char* path  = strdup(file_name);
    char* bname = basename(path);

    if(bname) {
        char* extension = index(bname, '.'); // Drop the file extension
        for(dxwifi_log_module_t module = DXWIFI_LOG_GENERIC; module < DXWIFI_LOG_MODULE_COUNT; ++module) {
            if(strncmp(bname, file_lookup_tbl[module], extension - bname) == 0) {
                return module;
            }
        }
    }
    free(path);
    return DXWIFI_LOG_GENERIC;
}


bool set_logger(dxwifi_log_module_t module, dxwifi_logger logger) {
    bool success = false;
    if(module == DXWIFI_LOG_ALL_MODULES) {
        for(size_t i = 0; i < DXWIFI_LOG_MODULE_COUNT; ++i) {
            handlers[i].logger = logger;
        }
        success = true;
    }
    else if(module < DXWIFI_LOG_MODULE_COUNT) {
        handlers[module].logger = logger;
        success = true;
    }
    return success;
}


bool set_log_level(dxwifi_log_module_t module, dxwifi_log_level_t level) {
    bool success = false;
    if(module == DXWIFI_LOG_ALL_MODULES) {
        for(size_t i = 0; i < DXWIFI_LOG_MODULE_COUNT; ++i) {
            handlers[i].log_level = level;
        }
        success = true;
    }
    else if(module < DXWIFI_LOG_MODULE_COUNT) {
        handlers[module].log_level = level;
        success = true;
    }
    return success;
}


void __dxwifi_log(dxwifi_log_level_t log_level, const char* file, const char* fmt, ...) {

    dxwifi_log_module_t module  = file_to_log_module(file);
    dxwifi_log_handler  handler = handlers[module];

    if( handler.logger && log_level <= handler.log_level) {
        va_list args;
        va_start(args, fmt);
        handler.logger(module, log_level, fmt, args);
        va_end(args);
    }
}


void __dxwifi_log_hexdump(const char* file, const uint8_t* data, int size) {

    int i           = 0;
    int nbytes      = 0;
    int location    = 0;
    int num_rows    = (size / 16) + 1;

    // 8 for line header, 3 chars per byte of data, 16 bytes of data per row, and a newline
    int bytes_per_row = 57;

    char temp[16];
    // Two more bytes for newline and null-terminator
    char formatted_str[(num_rows * bytes_per_row) + 2];

    formatted_str[location++] = '\n';

    while (i < size) {
        nbytes = snprintf(temp, 16, "%08x", i);

        memcpy(formatted_str + location, temp, nbytes);
        location += nbytes;

        for(int j = 0; j < 16 && i < size; ++i, ++j) {
            nbytes = snprintf(temp, 16, " %02x", *(data + i));
            memcpy(formatted_str + location, temp, nbytes);
            location += nbytes;
        }
        formatted_str[location++] = '\n';
    }
    formatted_str[location] = '\0';

    __dxwifi_log(DXWIFI_LOG_TRACE, file, "%s", formatted_str);
}
