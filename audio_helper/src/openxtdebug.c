#include "openxtdebug.h"

////////////////////////////////////////////////////////////////////////////////
// Global Variables                                                           //
////////////////////////////////////////////////////////////////////////////////

bool debugging_enabled = false;

////////////////////////////////////////////////////////////////////////////////
// Functions                                                                  //
////////////////////////////////////////////////////////////////////////////////

inline void openxt_debug_init(void) 
{
#ifdef SYSLOG
    openlog ("", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
#endif

    openxt_debug_set_enabled(true);
}

inline void openxt_debug_fini(void) 
{
#ifdef SYSLOG
    closelog ();
#endif

    openxt_debug_set_enabled(false);
}

bool openxt_debug_is_enabled(void)
{
    return debugging_enabled;
}

void openxt_debug_set_enabled(bool enabled)
{
    debugging_enabled = enabled;
}
