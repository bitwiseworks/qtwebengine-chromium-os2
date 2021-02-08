/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkLeanWindows.h"
#include "SkThreadID.h"

#ifdef SK_BUILD_FOR_WIN
    SkThreadID SkGetThreadID() { return GetCurrentThreadId(); }
#elif defined SK_BUILD_FOR_OS2
    #include <stdlib.h>
    SkThreadID SkGetThreadID() { return (int64_t)_gettid(); }
#else
    #include <pthread.h>
    SkThreadID SkGetThreadID() { return (int64_t)pthread_self(); }
#endif
