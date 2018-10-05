//
// Copyright (c) 2015 Assured Information Security, Inc
//
// Dates Modified:
//  - 4/8/2015: Initial commit
//    Rian Quinn <quinnr@ainfosec.com>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

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
    openlog (NULL, LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
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
