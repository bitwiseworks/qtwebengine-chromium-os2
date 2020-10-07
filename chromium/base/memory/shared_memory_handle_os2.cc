// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/shared_memory_handle.h"

#include "base/logging.h"
#include "base/unguessable_token.h"

namespace base {

SharedMemoryHandle::SharedMemoryHandle() {}

SharedMemoryHandle::SharedMemoryHandle(SHMEM h,
                                       size_t size,
                                       const base::UnguessableToken& guid)
    : handle_(h), guid_(guid), size_(size) {}

void SharedMemoryHandle::Close() const {
  DCHECK(handle_ != SHMEM_INVALID);
  if (shmem_close(handle_) == -1)
    PLOG(ERROR) << "shmem_close";
}

bool SharedMemoryHandle::IsValid() const {
  return handle_ != SHMEM_INVALID;
}

SharedMemoryHandle SharedMemoryHandle::Duplicate() const {
  SHMEM duped_handle = shmem_duplicate(handle_, 0);
  if (duped_handle == -1)
    return SharedMemoryHandle();
  base::SharedMemoryHandle handle(duped_handle, GetSize(), GetGUID());
  handle.SetOwnershipPassesToIPC(true);
  return handle;
}

SHMEM SharedMemoryHandle::GetHandle() const {
  return handle_;
}

void SharedMemoryHandle::SetOwnershipPassesToIPC(bool ownership_passes) {
  ownership_passes_to_ipc_ = ownership_passes;
}

bool SharedMemoryHandle::OwnershipPassesToIPC() const {
  return ownership_passes_to_ipc_;
}

}  // namespace base
