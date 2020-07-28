// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PAGE_ALLOCATOR_INTERNALS_OS2_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PAGE_ALLOCATOR_INTERNALS_OS2_H_

#include "base/allocator/partition_allocator/oom.h"
#include "base/allocator/partition_allocator/page_allocator_internal.h"
#include "base/logging.h"

namespace base {

namespace {

// Emulates semantics of Windows |VirtualAlloc|/|VirtualFree| that allow to
// commit and decommit already committed or decommitted pages.
APIRET MyDosSetMem(PVOID base, ULONG length, ULONG flags) {
  if (!(flags & (PAG_COMMIT | PAG_DECOMMIT)))
    return DosSetMem(base, length, flags);

  // Query the current state of each range to avoid committing/decommitting
  // already commited/decommitted pages.
  PVOID addr = base;
  APIRET arc;
  while (length) {
    ULONG act_len = length, act_flags, new_flags = flags;
    arc = DosQueryMem(addr, &act_len, &act_flags);
    if (arc != NO_ERROR)
      break;
    if ((new_flags & PAG_COMMIT) && (act_flags & PAG_COMMIT))
      new_flags &= ~PAG_COMMIT;
    if ((new_flags & PAG_DECOMMIT) && !(act_flags & (PAG_COMMIT | PAG_FREE)))
      new_flags &= ~PAG_DECOMMIT;
    if ((new_flags & (PAG_COMMIT | PAG_DECOMMIT)) ||
        (new_flags & fPERM) != (act_flags & fPERM)) {
      arc = DosSetMem(addr, act_len, new_flags);
      if (arc != NO_ERROR)
        break;
    }
    addr = static_cast<PVOID>(static_cast<char *>(addr) + act_len);
    length -= act_len;
  }

  return NO_ERROR;
}

}

// |DosAllocMemEx| will fail if allocation at the hint address is blocked.
constexpr bool kHintIsAdvisory = false;
std::atomic<int32_t> s_allocPageErrorCode{NO_ERROR};

int GetAccessFlags(PageAccessibilityConfiguration accessibility) {
  switch (accessibility) {
    case PageRead:
      return PAG_READ;
    case PageReadWrite:
      return PAG_READ | PAG_WRITE;
    case PageReadExecute:
      return PAG_READ | PAG_EXECUTE;
    case PageReadWriteExecute:
      return PAG_READ | PAG_WRITE | PAG_EXECUTE;
    default:
      NOTREACHED();
      FALLTHROUGH;
    case PageInaccessible:
      return 0;
  }
}

void* SystemAllocPagesInternal(void* hint,
                               size_t length,
                               PageAccessibilityConfiguration accessibility,
                               PageTag page_tag,
                               bool commit) {
  ULONG flags = GetAccessFlags(accessibility);
  if (flags == 0)
    flags = PAG_READ; // OS/2 requires at least one permission bit.
  if (commit)
    flags |= PAG_COMMIT;
  if (hint)
    flags |= OBJ_LOCATION;
  else
    flags |= OBJ_ANY; // Requiest high memory.

  void *base = hint;
  APIRET arc = DosAllocMemEx(&base, length, flags);
  if (arc != NO_ERROR && (flags & OBJ_ANY)) {
    // Try low memory.
    flags &= ~OBJ_ANY;
    arc = DosAllocMemEx(&base, length, flags);
  }
  if (arc != NO_ERROR) {
    s_allocPageErrorCode = arc;
    return nullptr;
  }
  return base;
}

void* TrimMappingInternal(void* base,
                          size_t base_length,
                          size_t trim_length,
                          PageAccessibilityConfiguration accessibility,
                          bool commit,
                          size_t pre_slack,
                          size_t post_slack) {
  void* ret = base;
  if (pre_slack || post_slack) {
    // We cannot resize the allocation run. Free it and retry at the aligned
    // address within the freed range.
    ret = reinterpret_cast<char*>(base) + pre_slack;
    FreePages(base, base_length);
    ret = SystemAllocPages(ret, trim_length, accessibility, PageTag::kChromium,
                           commit);
  }
  return ret;
}

bool TrySetSystemPagesAccessInternal(
    void* address,
    size_t length,
    PageAccessibilityConfiguration accessibility) {
  if (accessibility == PageInaccessible)
    return MyDosSetMem(address, length, PAG_DECOMMIT) == NO_ERROR;
  return MyDosSetMem(address, length, PAG_COMMIT |
                     GetAccessFlags(accessibility)) == NO_ERROR;
}

void SetSystemPagesAccessInternal(
    void* address,
    size_t length,
    PageAccessibilityConfiguration accessibility) {
  if (accessibility == PageInaccessible) {
    APIRET arc = MyDosSetMem(address, length, PAG_DECOMMIT);
    if (arc != NO_ERROR) {
      // We check `arc` for `NO_ERROR` here so that in a crash
      // report we get the error number.
      CHECK_EQ(static_cast<ULONG>(NO_ERROR), arc);
    }
  } else {
    APIRET arc = MyDosSetMem(address, length, PAG_COMMIT |
                             GetAccessFlags(accessibility));
    if (arc != NO_ERROR) {
      if (arc == ERROR_NOT_ENOUGH_MEMORY)
        OOM_CRASH();
      // We check `arc` for `NO_ERROR` here so that in a crash
      // report we get the arc number.
      CHECK_EQ(static_cast<ULONG>(NO_ERROR), arc);
    }
  }
}

void FreePagesInternal(void* address, size_t length) {
  APIRET arc = DosFreeMemEx(address);
  CHECK_EQ(static_cast<ULONG>(NO_ERROR), arc);
}

void DecommitSystemPagesInternal(void* address, size_t length) {
  SetSystemPagesAccess(address, length, PageInaccessible);
}

bool RecommitSystemPagesInternal(void* address,
                                 size_t length,
                                 PageAccessibilityConfiguration accessibility) {
  return TrySetSystemPagesAccess(address, length, accessibility);
}

void DiscardSystemPagesInternal(void* address, size_t length) {
  // TODO: OS/2 doen't seem to have madvise(MADV_DONTNEED)/MEM_RESET semantics.
}

}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PAGE_ALLOCATOR_INTERNALS_OS2_H_
