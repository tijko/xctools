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

// 
// The following can be used to figure out if the code is being execute under
// a unit test. This is useful for turning off functionality that cannot be 
// tested easily. 
// 
bool is_unittest(void);

///
/// This is the main function that runs the tests. If you add tests to this 
/// unit test, create a function(s) for your tests, and then add the 
/// function(s) into this function so that it can be run, if needed. 
///
void test_run(void);

#endif // UNITTEST_H