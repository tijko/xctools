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

#ifndef UNITTEST_H
#define UNITTEST_H

#include <stdbool.h>
#include <sys/types.h>

///
/// The following can be used to validate a test. Note that this can only be
/// used inside the unittest.c as it relies on global variables for some
/// stats.
///
#define UT_CHECK(a) \
    { \
        if ((a)) { \
            openxt_debug("    %s%04d%-20s%s\n", "test (", __LINE__, "): ", "[pass]"); \
            pass++; \
        } else { \
            openxt_debug("--> %s%04d%-20s%s\n", "test (", __LINE__, "): ", "[fail] <--"); \
            fail++; \
        } \
    }

///
/// This is the main function that runs the tests. If you add tests to this
/// unit test, create a function(s) for your tests, and then add the
/// function(s) into this function so that it can be run, if needed.
///
int openxt_unittest(int argc, char *argv[]);

#endif // UNITTEST_H
