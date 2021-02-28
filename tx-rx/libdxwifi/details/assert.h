/**
 *  assert.h
 * 
 *  DESCRIPTION: DxWiFi Assertion utility
 * 
 *  NOTES:
 *  Why not use the standard library <assert.h>? The standard assert libary does
 *  not support formatted messages which can be extremely helpful for debugging.
 *  Also, assert calls are compiled out if NDEBUG is defined, which means that 
 *  if we need to hard assert in a release builds to alert the user or parent 
 *  process of some irrecoverable error then we wouldn't be able to. 
 * 
 *  With that said, assert functions prefixed with `debug` are culled during 
 *  release builds and should be used to verify programmer errors like failure 
 *  to comply with a functions preconditions (i.e. null pointers). Hard asserts 
 *  should be used sparingly to indicate a systematic failure of the application.
 *  
 *  https://github.com/oresat/oresat-dxwifi-software
 * 
 */ 

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>


#include <libgen.h>

#include <libdxwifi/details/utils.h>


#ifdef LIBDXWIFI_DISABLE_ASSERTS
#define assert(expr)                      __DXWIFI_UTILS_UNUSED(expr)
#define assert_M(expr, msg, ...)          __DXWIFI_UTILS_UNUSED(expr, msg, ##__VA_ARGS__)
#define assert_always(msg, ...)           __DXWIFI_UTILS_UNUSED(msg, ##__VA_ARGS__)
#define assert_continue(expr, msg, ...)   __DXWIFI_UTILS_UNUSED(expr, msg, ##__VA_ARGS__)
#else

/* The first occurrence of expr is not evaluated due to the sizeof,
   but will trigger any pedantic warnings masked by the __extension__
   for the second occurrence. */
#define assert_M(expr, msg, ...)                                                \
    ((void) sizeof ((expr) ? 1 : 0), __extension__ ({                           \
        if (expr)                                                               \
            ; /* empty */                                                       \
        else                                                                    \
            __assert_M (true, #expr, __FILE__, __LINE__, msg, ##__VA_ARGS__);   \
    }))



#define assert_continue(expr, msg, ...)                                         \
    ((void) sizeof ((expr) ? 1 : 0), __extension__ ({                           \
        if (expr)                                                               \
            ; /* empty */                                                       \
        else                                                                    \
            __assert_M (false, #expr, __FILE__, __LINE__, msg, ##__VA_ARGS__);  \
    }))


#define assert(expr) assert_M(expr, "")


#define assert_always(msg, ...) assert_M(0, msg, ##__VA_ARGS__)


static void __assert_M(bool exit, const char* expr, const char* file, int line, const char* msg, ...) {

    char* path  = strdup(file);
    char* bname = basename(path);
    va_list args;
    fprintf(stderr, "%s:%d Assertion `%s` failed : ", bname, line, expr);
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
    fprintf(stderr, "\n");
    free(path);
    if( exit ) {
        abort();
    }
}


#endif // LIBDXWIFI_DISABLE_ASSERTS


#ifdef NDEBUG
#define debug_assert(expr)                      __DXWIFI_UTILS_UNUSED(expr)
#define debug_assert_M(expr, msg, ...)          __DXWIFI_UTILS_UNUSED(expr, msg, ##__VA_ARGS__)
#define debug_assert_always(msg, ...)           __DXWIFI_UTILS_UNUSED(msg, ##__VA_ARGS__)
#define debug_assert_continue(expr, msg, ...)   __DXWIFI_UTILS_UNUSED(expr, msg, ##__VA_ARGS__)
#else
#define debug_assert(expr)                      assert_M(expr, "")
#define debug_assert_M(expr, msg, ...)          assert_M(expr, msg, ##__VA_ARGS__)
#define debug_assert_always(msg, ...)           assert_always(msg, ##__VA_ARGS__)
#define debug_assert_continue(expr, msg, ...)   assert_continue(expr, msg, ##__VA_ARGS__)
#endif // NDEBUG
