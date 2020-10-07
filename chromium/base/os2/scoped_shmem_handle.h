// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_OS2_SCOPED_SHMEM_HANDLE_H_
#define BASE_OS2_SCOPED_SHMEM_HANDLE_H_

#include <libcx/shmem.h>

#include "base/scoped_generic.h"

namespace base {
namespace os2 {

struct BASE_EXPORT ScopedShmemHandleCloseTraits {
  static int InvalidValue() {
    return SHMEM_INVALID;
  }
  static void Free(SHMEM h) {
    // It's important to crash here. Keeping the memory handle open will
    // keep access to the underlying memory object making it non-freeable.
    int ret = shmem_close(h);
    PCHECK(0 == ret);
  }
};

typedef ScopedGeneric<SHMEM, ScopedShmemHandleCloseTraits> ScopedShmemHandle;

}  // namespace os2
}  // namespace base

#endif  // BASE_OS2_SCOPED_SHMEM_HANDLE_H_
