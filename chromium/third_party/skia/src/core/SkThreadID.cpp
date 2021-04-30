/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifdef __OS2__
// SkThreadID.h includes stdlib.h w/o this define but _gettid needs it.
#define _EMX_SOURCE
#endif

#include "include/private/SkThreadID.h"

#ifdef SK_BUILD_FOR_WIN
    #include "src/core/SkLeanWindows.h"
    SkThreadID SkGetThreadID() { return GetCurrentThreadId(); }
#elif defined SK_BUILD_FOR_OS2
    #define _EMX_SOURCE
    #include <stdlib.h>
    SkThreadID SkGetThreadID() { return (int64_t)_gettid(); }
#else
    #include <pthread.h>
    SkThreadID SkGetThreadID() { return (int64_t)pthread_self(); }
#endif
