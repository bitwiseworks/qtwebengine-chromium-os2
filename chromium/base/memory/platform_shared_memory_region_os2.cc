// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/platform_shared_memory_region.h"

#include "base/bits.h"

namespace base {
namespace subtle {

// static
PlatformSharedMemoryRegion PlatformSharedMemoryRegion::Take(
    os2::ScopedShmemHandle handle,
    Mode mode,
    size_t size,
    const UnguessableToken& guid) {
  if (!handle.is_valid())
    return {};

  if (size == 0)
    return {};

  if (size > static_cast<size_t>(std::numeric_limits<int>::max()))
    return {};

  CHECK(CheckPlatformHandlePermissionsCorrespondToMode(handle.get(),
                                                       mode, size));

  return PlatformSharedMemoryRegion(std::move(handle), mode, size, guid);
}

// static
PlatformSharedMemoryRegion
PlatformSharedMemoryRegion::TakeFromSharedMemoryHandle(
    const SharedMemoryHandle& handle,
    Mode mode) {
  CHECK(mode == Mode::kReadOnly || mode == Mode::kUnsafe);
  if (!handle.IsValid())
    return {};

  return Take(base::os2::ScopedShmemHandle(handle.GetHandle()), mode, handle.GetSize(),
              handle.GetGUID());
}

SHMEM PlatformSharedMemoryRegion::GetPlatformHandle() const {
  return handle_.get();
}

bool PlatformSharedMemoryRegion::IsValid() const {
  return handle_.is_valid();
}

PlatformSharedMemoryRegion PlatformSharedMemoryRegion::Duplicate() const {
  if (!IsValid())
    return {};

  CHECK_NE(mode_, Mode::kWritable)
      << "Duplicating a writable shared memory region is prohibited";

  SHMEM duped_handle = shmem_duplicate(handle_.get(), 0);
  if (duped_handle == -1) {
    DPLOG(ERROR) << "shmem_duplicate(" << handle_.get() << ") failed";
    return {};
  }

  return PlatformSharedMemoryRegion(os2::ScopedShmemHandle(duped_handle),
                                    mode_, size_, guid_);
}

bool PlatformSharedMemoryRegion::ConvertToReadOnly() {
  if (!IsValid())
    return false;

  CHECK_EQ(mode_, Mode::kWritable)
      << "Only writable shared memory region can be converted to read-only";

  SHMEM duped_handle = shmem_duplicate(handle_.get(), SHMEM_READONLY);
  if (duped_handle == SHMEM_INVALID) {
    DPLOG(ERROR) << "shmem_duplicate(" << handle_.get() << ", RO) failed";
    return false;
  }

  handle_.reset(duped_handle);
  mode_ = Mode::kReadOnly;
  return true;
}

bool PlatformSharedMemoryRegion::ConvertToUnsafe() {
  if (!IsValid())
    return false;

  CHECK_EQ(mode_, Mode::kWritable)
      << "Only writable shared memory region can be converted to unsafe";

  mode_ = Mode::kUnsafe;
  return true;
}

bool PlatformSharedMemoryRegion::MapAtInternal(off_t offset,
                                               size_t size,
                                               void** memory,
                                               size_t* mapped_size) const {
  SHMEM handle = handle_.get();

  *memory = shmem_map(handle, offset, size);
  if (!*memory) {
    DPLOG(ERROR) << "shmem_mmap(" << handle << ") failed";
    return false;
  }

  *mapped_size = size;
  return true;
}

// static
PlatformSharedMemoryRegion PlatformSharedMemoryRegion::Create(Mode mode,
                                                              size_t size) {
  if (size == 0)
    return {};

  CHECK_NE(mode, Mode::kReadOnly) << "Creating a region in read-only mode will "
                                     "lead to this region being non-modifiable";

  SHMEM handle = shmem_create(size, 0);
  if (handle == SHMEM_INVALID) {
    DPLOG(ERROR) << "shmem_create(" << size << ") failed";
    return {};
  }
  return PlatformSharedMemoryRegion(os2::ScopedShmemHandle(handle), mode, size,
                                    UnguessableToken::Create());
}

// static
bool PlatformSharedMemoryRegion::CheckPlatformHandlePermissionsCorrespondToMode(
    PlatformHandle handle,
    Mode mode,
    size_t size) {
  int flags;
  int rc = shmem_get_info(handle, &flags, nullptr, nullptr);
  if (rc == -1) {
    DPLOG(ERROR) << "shmem_get_info(" << handle << ") failed";
    return {};
  }

  const bool is_read_only = flags & SHMEM_READONLY;
  const bool expected_read_only = mode == Mode::kReadOnly;

  if (is_read_only != expected_read_only) {
    DLOG(ERROR) << "Shared memory handle has wrong access rights: it is"
                << (is_read_only ? " " : " not ") << "read-only but it should"
                << (expected_read_only ? " " : " not ") << "be";
    return false;
  }

  return true;
}

PlatformSharedMemoryRegion::PlatformSharedMemoryRegion(
    os2::ScopedShmemHandle handle,
    Mode mode,
    size_t size,
    const UnguessableToken& guid)
    : handle_(std::move(handle)), mode_(mode), size_(size), guid_(guid) {}

}  // namespace subtle
}  // namespace base
