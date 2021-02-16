// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/alias.h"
#include "base/logging.h"
#include "base/process/memory.h"
#include "build/build_config.h"

#if defined(OS_OS2)
#include "base/os2/os2_toolkit.h"
#include <umalloc.h>
#endif

namespace base {

// Defined in memory_win.cc for Windows.
#if !defined(OS_WIN)

namespace {

// Breakpad server classifies base::`anonymous namespace'::OnNoMemory as
// out-of-memory crash.
NOINLINE void OnNoMemory(size_t size) {
  size_t tmp_size = size;
  base::debug::Alias(&tmp_size);
#if defined(OS_OS2)
  LOG(ERROR) << "Out of memory. size=" << tmp_size;
  // Log some numbers from LIBC and system before crashing.
  _HEAPSTATS hst;
  if (!_ustats(_udefault(nullptr), &hst))
    LOG(ERROR) << "LIBC heap: provided "
               << hst._provided << ", used " << hst._used
               << ", maxfree " << hst._max_free;
  else
    PLOG(ERROR) << "_ustats failed";
  ULONG buf[2];
  if (!DosQuerySysInfo(QSV_MAXPRMEM, QSV_MAXSHMEM, buf, sizeof(buf)))
    LOG(ERROR) << "System low mem: max private " << buf[0]
               << ", max shared " << buf[1];
  if (!DosQuerySysInfo(QSV_MAXHPRMEM, QSV_MAXHSHMEM, buf, sizeof(buf)))
    LOG(ERROR) << "System high mem: max private " << buf[0]
               << ", max shared " << buf[1];
  TRAP_SEQUENCE();
#else
  LOG(FATAL) << "Out of memory. size=" << tmp_size;
#endif
}

}  // namespace

void TerminateBecauseOutOfMemory(size_t size) {
  OnNoMemory(size);
}

#endif

// Defined in memory_mac.mm for Mac.
#if !defined(OS_MACOSX)

bool UncheckedCalloc(size_t num_items, size_t size, void** result) {
  const size_t alloc_size = num_items * size;

  // Overflow check
  if (size && ((alloc_size / size) != num_items)) {
    *result = nullptr;
    return false;
  }

  if (!UncheckedMalloc(alloc_size, result))
    return false;

  memset(*result, 0, alloc_size);
  return true;
}

#endif

}  // namespace base
