// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/shared_memory.h"

#include <limits>

#include "base/os2/os2_toolkit.h"

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
  // Since the handle is actually a virtual address of the memory object,
  // there is no particular limit on the number of its copies. Use what
  // Windows returns (8M). Note that this value is used e.g. for the maximum
  // amount of "in-flight" HTTP requests (converted to int), so set with care.
  return static_cast<size_t>(1 << 23);
}

bool SharedMemory::CreateAndMapAnonymous(size_t size) {
  return CreateAnonymous(size) && Map(size);
}

bool SharedMemory::Create(const SharedMemoryCreateOptions& options) {
  // On OS/2 Warp, memory objects are allocated using 64K blocks.
  static const size_t kSectionMask = 65536 - 1;
  DCHECK(!options.executable);
  DCHECK(!shm_.IsValid());
  if (options.size == 0) {
    return false;
  }

  // Check maximum accounting for overflow.
  if (options.size >
      static_cast<size_t>(std::numeric_limits<int>::max()) - kSectionMask) {
    return false;
  }

  size_t rounded_size = (options.size + kSectionMask) & ~kSectionMask;

  PVOID base;
  ULONG flags = PAG_READ | PAG_EXECUTE | PAG_WRITE | OBJ_GETTABLE;
  APIRET arc = DosAllocSharedMem(&base, nullptr, rounded_size, flags);
  if (arc) {
    DCHECK_EQ(arc, 0U);
    return false;
  }

  requested_size_ = options.size;
  shm_ = SharedMemoryHandle(base, mapped_size_, UnguessableToken::Create());
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

  // On OS/2, we always map the whole object so need to check the limits
  // manually.
  if (offset + bytes > shm_.GetSize()) {
    DLOG(ERROR) << "Offset + bytes exceeds the SharedMemory size.";
    return false;
  }

  PVOID mem = shm_.GetHandle();

  ULONG len = ~0, flags;
  APIRET arc = DosQueryMem(mem, &len, &flags);
  if (arc || len < shm_.GetSize()) {
    DCHECK_EQ(arc, 0U);
    DCHECK_GE(len, shm_.GetSize());
    return false;
  }

  if (flags & PAG_FREE) {
    // Get access to the memory object.
    flags = PAG_READ | PAG_EXECUTE;
    if (!read_only_)
      flags |= PAG_WRITE;
    arc = DosGetSharedMem(mem, flags);
    if (arc) {
      DCHECK_EQ(arc, 0U);
      return false;
    }
  } else {
    // DCHECK(!(flags & PAG_COMMIT));
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
    if (need_alias || bool(flags & PAG_WRITE) != (!read_only_)) {
      // Access mode of the region and the underlying mapping disagree so we
      // cannot use it directly and need an alias for this process.
      arc = DosAliasMem(static_cast<char*>(mem) + offset, bytes,
                        &mem, OBJ_SELMAPALL | SEL_USE32);
      // Set the desired access mode.
      if (!arc) {
        flags = PAG_READ | PAG_EXECUTE;
        if (!read_only_)
          flags |= PAG_WRITE;
        arc = DosSetMem(mem, bytes, flags);
      }
      if (arc) {
        DCHECK_EQ(arc, 0U);
        return false;
      }
    }
  }

  memory_ = static_cast<char*>(mem) + offset;
  mapped_size_ = bytes;
  mapped_id_ = shm_.GetGUID();
  SharedMemoryTracker::GetInstance()->IncrementMemoryUsage(*this);
  return true;
}

bool SharedMemory::Unmap() {
  if (!memory_)
    return false;

  SharedMemoryTracker::GetInstance()->DecrementMemoryUsage(*this);

  // Only free private aliases and not shared objects themselves as it will
  // destroy the memory object if this process is holding the last reference but
  // Unmap API expects that the object survives (TODO: OS/2 has no concept of
  // handles to memory objects that could be held and passed along separately so
  // in order to preserve the object we must preserve the allocation itself. A
  // possible solution is to introduce our own inter-process reference counting
  // but it's a job for LIBCn/LIBCx, not Chromium).
  ULONG len = ~0, flags;
  APIRET arc = DosQueryMem(memory_, &len, &flags);
  if (!arc && !(flags & PAG_SHARED)) {
    arc = DosFreeMem(memory_);
  }
  if (arc) {
    DCHECK_EQ(arc, 0U);
    return false;
  }

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
  // On OS/2, setting the memory object to read-only will affect all uses of it
  // within the same process (as it's the same virtual address) but will not
  // affect other processes that will gain write permission via DosGetSharedMem
  // anyway (despite it's still the same virtal address) and will have to revoke
  // it manually if needed.
  ULONG flags = PAG_READ | PAG_EXECUTE;
  APIRET arc = DosSetMem(shm_.GetHandle(), shm_.GetSize(), flags);
  if (arc) {
    DCHECK_EQ(arc, 0U);
    return SharedMemoryHandle();
  }

  SharedMemoryHandle handle(shm_.GetHandle(), shm_.GetSize(), shm_.GetGUID());
  handle.SetOwnershipPassesToIPC(true);
  return handle;
}

}  // namespace base
