// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/platform_shared_memory_region.h"

#include "base/bits.h"
#include "base/os2/os2_toolkit.h"

namespace base {
namespace subtle {

// static
PlatformSharedMemoryRegion PlatformSharedMemoryRegion::Take(
    os2::ScopedSharedMemObj handle,
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

  return Take(base::os2::ScopedSharedMemObj(handle.GetHandle()), mode, handle.GetSize(),
              handle.GetGUID());
}

void * PlatformSharedMemoryRegion::GetPlatformHandle() const {
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

  return PlatformSharedMemoryRegion(os2::ScopedSharedMemObj(handle_.get()),
                                    mode_, size_, guid_);
}

bool PlatformSharedMemoryRegion::ConvertToReadOnly() {
  if (!IsValid())
    return false;

  CHECK_EQ(mode_, Mode::kWritable)
      << "Only writable shared memory region can be converted to read-only";

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
  PVOID mem = handle_.get();

  ULONG len = ~0, flags;
  APIRET arc = DosQueryMem(mem, &len, &flags);
  if (arc) {
    DCHECK_EQ(arc, 0U);
    return false;
  }

  // On OS/2, we always map the whole object so need to check the limits
  // manually.
  if (offset + size > len) {
    DLOG(ERROR) << "Offset + size exceeds the PlatformSharedMemoryRegion size.";
    return false;
  }

  if (flags & PAG_FREE) {
    // Get access to the memory object allocated by another process.
    flags = PAG_READ | PAG_EXECUTE;
    if (mode_ != Mode::kReadOnly)
      flags |= PAG_WRITE;
    arc = DosGetSharedMem(mem, flags);
    if (arc) {
      DCHECK_EQ(arc, 0U);
      return false;
    }
  } else {
    bool need_alias = false;
    if (!(flags & PAG_COMMIT)) {
      // Commit the memory object if not done so yet.
      arc = DosSetMem(mem, len, PAG_COMMIT | PAG_DEFAULT);
      if (arc) {
        DCHECK_EQ(arc, 0U);
        return false;
      }
    } else {
      // This object is already mapped and committed into this process, a new
      // mapping is requested. We have to use an alias so that when this mapping
      // is freed, other mappings remain intact (TODO: Use a refcounter instead
      // to save vritual address space. This also requires fixing assertions in
      // SharedMemoryTracker::IncrementMemoryUsage as it expects that each map
      // request returns a distinct address).
      need_alias = true;
    }
    if (need_alias || bool(flags & PAG_WRITE) != (mode_ != Mode::kReadOnly)) {
      // Access mode of the region and the underlying mapping disagree so we
      // cannot use it directly and need an alias for this process.
      arc = DosAliasMem(static_cast<char*>(mem) + offset, size,
                        &mem, OBJ_SELMAPALL | SEL_USE32);
      // Set the desired access mode.
      if (!arc) {
        flags = PAG_READ | PAG_EXECUTE;
        if (mode_ != Mode::kReadOnly)
          flags |= PAG_WRITE;
        arc = DosSetMem(mem, size, flags);
      }
      if (arc) {
        DCHECK_EQ(arc, 0U);
        return false;
      }
    }
  }

  *memory = static_cast<char*>(mem) + offset;
  *mapped_size = size;
  return true;
}

// static
PlatformSharedMemoryRegion PlatformSharedMemoryRegion::Create(Mode mode,
                                                              size_t size) {
  // On OS/2 Warp, memory objects are allocated using 64K blocks.
  static const size_t kSectionSize = 65536;
  if (size == 0)
    return {};

  size_t rounded_size = bits::Align(size, kSectionSize);
  if (rounded_size > static_cast<size_t>(std::numeric_limits<int>::max()))
    return {};

  CHECK_NE(mode, Mode::kReadOnly) << "Creating a region in read-only mode will "
                                     "lead to this region being non-modifiable";

  PVOID base;
  ULONG flags = PAG_READ | PAG_EXECUTE | OBJ_GETTABLE;
  if (mode != Mode::kReadOnly)
    flags |= PAG_WRITE;
  APIRET arc = DosAllocSharedMem(&base, nullptr, size, flags);
  if (arc) {
    DCHECK_EQ(arc, 0U);
    return {};
  }

  return PlatformSharedMemoryRegion(os2::ScopedSharedMemObj(base), mode, size,
                                    UnguessableToken::Create());
}

// static
bool PlatformSharedMemoryRegion::CheckPlatformHandlePermissionsCorrespondToMode(
    PlatformHandle handle,
    Mode mode,
    size_t size) {
  ULONG len = size, flags;
  APIRET arc = DosQueryMem(handle, &len, &flags);
  if (arc) {
    DCHECK_EQ(arc, 0U);
    return false;
  }

  if (!(flags & PAG_BASE)) {
    DLOG(ERROR) << "Memory object " << handle << " is not base";
    return false;
  }

  if (!(flags & PAG_SHARED)) {
    DLOG(ERROR) << "Memory object " << handle << " is not shared";
    return false;
  }

  // Note that we don't check if mode_ matches access permissions as it's a
  // property of the process where this object is mapped to, not object itself.
  // On OS/2, any shared memory object may be mapped as read-write.

  return true;
}

PlatformSharedMemoryRegion::PlatformSharedMemoryRegion(
    os2::ScopedSharedMemObj handle,
    Mode mode,
    size_t size,
    const UnguessableToken& guid)
    : handle_(std::move(handle)), mode_(mode), size_(size), guid_(guid) {}

}  // namespace subtle
}  // namespace base
