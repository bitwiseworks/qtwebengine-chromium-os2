// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/shared_memory.h"

#include <limits>

#include <libcx/shmem.h>

#include "base/bits.h"
#include "base/logging.h"
#include "base/memory/shared_memory_tracker.h"

namespace base {

SharedMemory::SharedMemory() {}

SharedMemory::SharedMemory(const SharedMemoryHandle& handle, bool read_only)
    : shm_(handle), read_only_(read_only) {}

SharedMemory::~SharedMemory() {
  Unmap();
  Close();
}

// static
bool SharedMemory::IsHandleValid(const SharedMemoryHandle& handle) {
  return handle.IsValid();
}

// static
void SharedMemory::CloseHandle(const SharedMemoryHandle& handle) {
  DCHECK(handle.IsValid());
  handle.Close();
}

// static
size_t SharedMemory::GetHandleLimit() {
  return shmem_max_handles();
}

bool SharedMemory::CreateAndMapAnonymous(size_t size) {
  return CreateAnonymous(size) && Map(size);
}

bool SharedMemory::Create(const SharedMemoryCreateOptions& options) {
  DCHECK(!options.executable);
  DCHECK(!shm_.IsValid());
  if (options.size == 0) {
    return false;
  }

  if (options.size > static_cast<size_t>(std::numeric_limits<int>::max()))
    return false;

  SHMEM handle = shmem_create(options.size, 0);
  if (handle == SHMEM_INVALID) {
    DPLOG(ERROR) << "shmem_create(" << options.size << ") failed";
    return false;
  }

  requested_size_ = options.size;
  shm_ = SharedMemoryHandle(handle, options.size, UnguessableToken::Create());
  return true;
}

bool SharedMemory::MapAt(off_t offset, size_t bytes) {
  if (!shm_.IsValid()) {
    DLOG(ERROR) << "Invalid SharedMemoryHandle.";
    return false;
  }

  if (bytes > static_cast<size_t>(std::numeric_limits<int>::max())) {
    DLOG(ERROR) << "Bytes required exceeds the 2G limitation.";
    return false;
  }

  if (memory_) {
    DLOG(ERROR) << "The SharedMemory has been mapped already.";
    return false;
  }

  SHMEM handle = shm_.GetHandle();

  memory_ = shmem_map(handle, offset, bytes);
  if (!memory_) {
    DPLOG(ERROR) << "shmem_mmap(" << handle << ") failed";
    return false;
  }

  DCHECK_EQ(0U, reinterpret_cast<uintptr_t>(memory_) &
                    (SharedMemory::MAP_MINIMUM_ALIGNMENT - 1));
  mapped_size_ = bytes;
  mapped_id_ = shm_.GetGUID();
  SharedMemoryTracker::GetInstance()->IncrementMemoryUsage(*this);
  return true;
}

bool SharedMemory::Unmap() {
  if (!memory_)
    return false;

  SharedMemoryTracker::GetInstance()->DecrementMemoryUsage(*this);
  shmem_unmap(memory_);
  memory_ = nullptr;
  mapped_size_ = 0;
  mapped_id_ = UnguessableToken();
  return true;
}

void SharedMemory::Close() {
  if (shm_.IsValid()) {
    shm_.Close();
    shm_ = SharedMemoryHandle();
  }
}

SharedMemoryHandle SharedMemory::handle() const {
  return shm_;
}

SharedMemoryHandle SharedMemory::TakeHandle() {
  SharedMemoryHandle handle(shm_);
  handle.SetOwnershipPassesToIPC(true);
  Unmap();
  shm_ = SharedMemoryHandle();
  return handle;
}

SharedMemoryHandle SharedMemory::DuplicateHandle(
    const SharedMemoryHandle& handle) {
  return handle.Duplicate();
}

SharedMemoryHandle SharedMemory::GetReadOnlyHandle() const {
  SHMEM duped_handle = shmem_duplicate(shm_.GetHandle(), SHMEM_READONLY);
  if (duped_handle == -1) {
    DPLOG(ERROR) << "shmem_duplicate(" << shm_.GetHandle() << ") failed";
    return SharedMemoryHandle();
  }

  SharedMemoryHandle handle(duped_handle, shm_.GetSize(), shm_.GetGUID());
  handle.SetOwnershipPassesToIPC(true);
  return handle;
}

}  // namespace base
