#pragma once



#define GEN_ASSERT_IN_RELEASE 1

#if GEN_ASSERT_IN_RELEASE
   #ifdef NDEBUG
      #undef NDEBUG
   #endif
#endif

#include <cassert>

#define GEN_ASSERT(p)     assert(p);
