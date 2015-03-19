#ifndef OPENXT_DEBUG_H
#define OPENXT_DEBUG_H

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>

#include "openxtsettings.h"

#ifdef SYSLOG
#include <syslog.h>
#include <stdlib.h>
#else
#include <stdio.h>
#include <stdlib.h>
#endif

///
/// Define TAG in your settings if you want a custom TAG provided in the debug
/// statements
///
#ifndef TAG
#define TAG ""
#endif

///
/// This function can be used to tell the compiler that a variable is not used. 
/// 
/// @param a unused variable
///
#define openxt_unused(a) (void)a

///
/// The goal of this macro to provide a a simple debug statement that also 
/// encapsulates syslog, so that things are reported to syslog if desired. 
/// You should not use this directly, but instead use one of the openxt_
/// macros. 
///
#ifdef SYSLOG
    #define OPENXT_ERROR(...) \
        fprintf(stderr, __VA_ARGS__); \
        syslog(LOG_ERR, __VA_ARGS__)
    #define OPENXT_DEBUG(...) \
        fprintf(stdout, __VA_ARGS__); \
        syslog(LOG_DEBUG, __VA_ARGS__)
#else
    #define OPENXT_ERROR(...) \
        fprintf(stderr, __VA_ARGS__)
    #define OPENXT_DEBUG(...) \
        fprintf(stdout, __VA_ARGS__)
#endif

///
/// This function can be used to validate that a pointer is not equal to NULL. 
/// It takes a second, variable set of arguments that allows you to provide a 
/// return value when the error occurs. If the function is a void, you can 
/// remove the second argument, and the compiler will equate that to "return;"
/// 
/// @param a pointer to validate
/// @param ... return value (for none void functions)
///
#define openxt_checkp(a,...) \
    if ((a) == 0) { \
        if (openxt_debug_is_enabled() == true) { \
            OPENXT_ERROR("%s: ERROR: {%s} == NULL, line: %d, func: %s\n", TAG, #a, __LINE__, __PRETTY_FUNCTION__); \
        } \
        return __VA_ARGS__; \
    }

///
/// This function can be used to validate that a pointer is not equal to NULL. 
/// It takes a second, variable set of arguments that allows you to provide a 
/// return value when the error occurs. If the function is a void, you can 
/// remove the second argument, and the compiler will equate that to "return;"
/// 
/// @param a pointer to validate
/// @param b goto label
///
#define openxt_checkp_goto(a,b) \
    if ((a) == 0) { \
        if (openxt_debug_is_enabled() == true) { \
            OPENXT_ERROR("%s: ERROR: {%s} == NULL, line: %d, func: %s\n", TAG, #a, __LINE__, __PRETTY_FUNCTION__); \
        } \
        goto b; \
    }

///
/// This is an assert, with a return. Most asserts are removed in "production"
/// but this version is not. This macro (or it's alternatives) will likely be 
/// used a lot as it makes sure that what you are calling executed correctly. 
/// Use this to validate that a function executed correctly, or use it to 
/// validate that a variable has an expected value. The goal is to use this 
/// macro enough such that, if an error occurs, the function gracefully exits
/// before something really bad happens. 
/// 
/// @param a the expression to validate
/// @param ... return value (for none void functions)
///
#define openxt_assert(a,...) \
    if (!(a)) { \
        if (openxt_debug_is_enabled() == true) { \
            OPENXT_ERROR("%s: ERROR: {%s} == false, line: %d, func: %s\n", TAG, #a, __LINE__, __PRETTY_FUNCTION__); \
        } \
        return __VA_ARGS__; \
    }

///
/// This is an assert, with a goto. Most asserts are removed in "production"
/// but this version is not. This macro (or it's alternatives) will likely be 
/// used a lot as it makes sure that what you are calling executed correctly. 
/// Use this to validate that a function executed correctly, or use it to 
/// validate that a variable has an expected value. The goal is to use this 
/// macro enough such that, if an error occurs, the function gracefully exits
/// before something really bad happens. 
/// 
/// @param a the expression to validate
/// @param b goto label
///
#define openxt_assert_goto(a,b) \
    if (!(a)) { \
        if (openxt_debug_is_enabled() == true) { \
            OPENXT_ERROR("%s: ERROR: {%s} == false, line: %d, func: %s\n", TAG, #a, __LINE__, __PRETTY_FUNCTION__); \
        } \
        goto b; \
    }

///
/// This is an assert, with a return. Most asserts are removed in "production"
/// but this version is not. This macro (or it's alternatives) will likely be 
/// used a lot as it makes sure that what you are calling executed correctly. 
/// Use this to validate that a function executed correctly, or use it to 
/// validate that a variable has an expected value. The goal is to use this 
/// macro enough such that, if an error occurs, the function gracefully exits
/// before something really bad happens. 
/// 
/// @param a the expression to validate
/// @param b linux error code (can either be ret, or errno)
/// @param ... return value (for none void functions)
///
#define openxt_assert_ret(a,b,...) \
    if (!(a)) { \
        if (openxt_debug_is_enabled() == true) { \
            OPENXT_ERROR("%s: ERROR: {%s} == false, error: %d, srterror: %s, line: %d, func: %s\n", TAG, #a, b, strerror(b), __LINE__, __PRETTY_FUNCTION__); \
        } \
        return __VA_ARGS__; \
    }

///
/// This is an assert, with a return. Most asserts are removed in "production"
/// but this version is not. This macro (or it's alternatives) will likely be 
/// used a lot as it makes sure that what you are calling executed correctly. 
/// Use this to validate that a function executed correctly, or use it to 
/// validate that a variable has an expected value. The goal is to use this 
/// macro enough such that, if an error occurs, the function gracefully exits
/// before something really bad happens. 
/// 
/// @param a the expression to validate
/// @param ... return value (for none void functions)
///
#define openxt_assert_quiet(a,...) \
    if (!(a)) { \
        return __VA_ARGS__; \
    }

///
/// This function provide a wrapped printf (info). You can also define SYSLOG 
/// in your settings file, and the functionality will convert to using 
/// syslog instead of using printf. Use these functions just like 
/// printf. 
///
#ifdef DEBUGGING_ENABLED
#define openxt_info(...) \
    if (openxt_debug_is_enabled() == true) { \
        OPENXT_ERROR(TAG ": INFO: " __VA_ARGS__); \
    }
#else
#define openxt_info(...)
#endif

///
/// This function provide a wrapped printf (warn). You can also define SYSLOG 
/// in your settings file, and the functionality will convert to using 
/// syslog instead of using printf. Use these functions just like 
/// printf. 
///
#define openxt_warn(...) \
    if (openxt_debug_is_enabled() == true) { \
        OPENXT_ERROR(TAG ": WARNING: " __VA_ARGS__); \
    }

///
/// This function provide a wrapped printf (error). You can also define SYSLOG 
/// in your settings file, and the functionality will convert to using 
/// syslog instead of using printf. Use these functions just like 
/// printf. 
///
#define openxt_error(...) \
    if (openxt_debug_is_enabled() == true) { \
        OPENXT_ERROR(TAG ": ERROR: " __VA_ARGS__); \
    }

///
/// This function provide a wrapped printf (error). You can also define SYSLOG 
/// in your settings file, and the functionality will convert to using 
/// syslog instead of using printf. Use these functions just like 
/// printf. 
///
#define openxt_debug(...) \
        OPENXT_ERROR(TAG ": DEBUG: " __VA_ARGS__); \

///
/// Helpful for debugging issues
///
#define openxt_line openxt_info("file: %s, line: %d\n", __FILE__, __LINE__);

///
/// This should be the first thing your run in your program. This initializes 
/// debugging
///
void openxt_debug_init(void);

///
/// This should be the last thing your program does. This cleans up debugging. 
///
void openxt_debug_fini(void);

///
/// The following tells the debug statements where or not debugging is 
/// turned on
///
/// @return true = debugging is enabled
///
bool openxt_debug_is_enabled(void);

/// 
/// The following enables / disables debugging
/// 
/// @param enabled true = turn on debug messages
/// 
void openxt_debug_set_enabled(bool enabled);

#endif // OPENXT_DEBUG_H
