// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_OS2_SCOPED_SHARED_MEM_OBJ_H_
#define BASE_OS2_SCOPED_SHARED_MEM_OBJ_H_

#include "base/scoped_generic.h"

namespace base {
namespace os2 {

// Shared memory is identified with a virtual address so there is nothing to
// do with it other than set to nullptr when moving away.
struct BASE_EXPORT ScopedSharedMemObjTraits {
  static void * InvalidValue() {
    return nullptr;
  }
  static void Free(void *) {}
};

typedef ScopedGeneric<void *, ScopedSharedMemObjTraits> ScopedSharedMemObj;

}  // namespace os2
}  // namespace base

#endif  // BASE_OS2_SCOPED_SHARED_MEM_OBJ_H_
