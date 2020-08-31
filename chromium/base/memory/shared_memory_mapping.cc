// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/shared_memory_mapping.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/shared_memory_tracker.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"

#if defined(OS_POSIX)
#include <sys/mman.h>
#endif

#if defined(OS_WIN)
#include <aclapi.h>
#endif

#if defined(OS_OS2)
#include "base/os2/os2_toolkit.h"
#endif

#if defined(OS_MACOSX) && !defined(OS_IOS)
#include <mach/mach_vm.h>
#include "base/mac/mach_logging.h"
#endif

#if defined(OS_FUCHSIA)
#include <lib/zx/vmar.h>
#include "base/fuchsia/fuchsia_logging.h"
#endif

namespace base {

SharedMemoryMapping::SharedMemoryMapping() = default;

SharedMemoryMapping::SharedMemoryMapping(SharedMemoryMapping&& mapping)
    : memory_(mapping.memory_),
      size_(mapping.size_),
      mapped_size_(mapping.mapped_size_),
      guid_(mapping.guid_) {
  mapping.memory_ = nullptr;
}

SharedMemoryMapping& SharedMemoryMapping::operator=(
    SharedMemoryMapping&& mapping) {
  Unmap();
  memory_ = mapping.memory_;
  size_ = mapping.size_;
  mapped_size_ = mapping.mapped_size_;
  guid_ = mapping.guid_;
  mapping.memory_ = nullptr;
  return *this;
}

SharedMemoryMapping::~SharedMemoryMapping() {
  Unmap();
}

SharedMemoryMapping::SharedMemoryMapping(void* memory,
                                         size_t size,
                                         size_t mapped_size,
                                         const UnguessableToken& guid)
    : memory_(memory), size_(size), mapped_size_(mapped_size), guid_(guid) {
  SharedMemoryTracker::GetInstance()->IncrementMemoryUsage(*this);
}

void SharedMemoryMapping::Unmap() {
  if (!IsValid())
    return;

  SharedMemoryTracker::GetInstance()->DecrementMemoryUsage(*this);
#if defined(OS_WIN)
  if (!UnmapViewOfFile(memory_))
    DPLOG(ERROR) << "UnmapViewOfFile";
#elif defined(OS_OS2)
  // Only free private aliases and not shared objects themselves as it will
  // destroy the memory object if this process is holding the last reference but
  // Unmap API expects that the object survives (TODO: OS/2 has no concept of
  // handles to memory objects that could be held and passed along separately so
  // in order to preserve the object we must preserve the allocation itself. A
  // possible solution is to introduce our own inter-process reference counting
  // but it's a job for LIBCn/LIBCx, not Chromium).
  ULONG len = ~0, flags;
  APIRET arc = DosQueryMem(memory_, &len, &flags);
  if (!arc && !(flags & PAG_SHARED))
    arc = DosFreeMem(memory_);
  DCHECK_EQ(arc, 0U);
#elif defined(OS_FUCHSIA)
  uintptr_t addr = reinterpret_cast<uintptr_t>(memory_);
  zx_status_t status = zx::vmar::root_self()->unmap(addr, mapped_size_);
  if (status != ZX_OK)
    ZX_DLOG(ERROR, status) << "zx_vmar_unmap";
#elif defined(OS_MACOSX) && !defined(OS_IOS)
  kern_return_t kr = mach_vm_deallocate(
      mach_task_self(), reinterpret_cast<mach_vm_address_t>(memory_),
      mapped_size_);
  MACH_DLOG_IF(ERROR, kr != KERN_SUCCESS, kr) << "mach_vm_deallocate";
#else
  if (munmap(memory_, mapped_size_) < 0)
    DPLOG(ERROR) << "munmap";
#endif
}

ReadOnlySharedMemoryMapping::ReadOnlySharedMemoryMapping() = default;
ReadOnlySharedMemoryMapping::ReadOnlySharedMemoryMapping(
    ReadOnlySharedMemoryMapping&&) = default;
ReadOnlySharedMemoryMapping& ReadOnlySharedMemoryMapping::operator=(
    ReadOnlySharedMemoryMapping&&) = default;
ReadOnlySharedMemoryMapping::ReadOnlySharedMemoryMapping(
    void* address,
    size_t size,
    size_t mapped_size,
    const UnguessableToken& guid)
    : SharedMemoryMapping(address, size, mapped_size, guid) {}

WritableSharedMemoryMapping::WritableSharedMemoryMapping() = default;
WritableSharedMemoryMapping::WritableSharedMemoryMapping(
    WritableSharedMemoryMapping&&) = default;
WritableSharedMemoryMapping& WritableSharedMemoryMapping::operator=(
    WritableSharedMemoryMapping&&) = default;
WritableSharedMemoryMapping::WritableSharedMemoryMapping(
    void* address,
    size_t size,
    size_t mapped_size,
    const UnguessableToken& guid)
    : SharedMemoryMapping(address, size, mapped_size, guid) {}

}  // namespace base
