// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/platform_thread.h"

#include "base/threading/platform_thread_internal_posix.h"
#include "base/threading/thread_id_name_manager.h"

#include "base/os2/os2_toolkit.h"

namespace base {

namespace internal {

bool SetCurrentThreadPriorityForPlatform(ThreadPriority priority) {
  ULONG cls = 0;
  switch (priority) {
    case ThreadPriority::BACKGROUND:
      cls = PRTYC_IDLETIME;
      break;
    case ThreadPriority::NORMAL:
      cls = PRTYC_REGULAR;
      break;
    case ThreadPriority::DISPLAY:
      cls = PRTYC_FOREGROUNDSERVER;
      break;
    case ThreadPriority::REALTIME_AUDIO:
      cls = PRTYC_TIMECRITICAL;
      break;
  }
  return DosSetPriority(PRTYS_THREAD, cls, 0, _gettid()) == NO_ERROR;
}

Optional<ThreadPriority> GetCurrentThreadPriorityForPlatform() {
  Optional<ThreadPriority> prio = base::nullopt;
  PTIB ptib;
  APIRET arc = DosGetInfoBlocks(&ptib, NULL);
  if (arc == NO_ERROR) {
    // The priority class is packed into the high byte of the low word.
    ULONG cls = (ptib->tib_ptib2->tib2_ulpri >> 8) & 0xFF;
    switch (cls) {
      case PRTYC_IDLETIME:
        prio = base::make_optional(ThreadPriority::BACKGROUND);
        break;
      case PRTYC_REGULAR:
        prio = base::make_optional(ThreadPriority::NORMAL);
        break;
      case PRTYC_FOREGROUNDSERVER:
        prio = base::make_optional(ThreadPriority::DISPLAY);
        break;
      case PRTYC_TIMECRITICAL:
        prio = base::make_optional(ThreadPriority::REALTIME_AUDIO);
        break;
    }
  }
  return prio;
}

}  // namespace internal

void InitThreading() {}

void TerminateOnThread() {}

size_t GetDefaultThreadStackSize(const pthread_attr_t& attributes) {
  return 0;
}

// static
void PlatformThread::SetName(const std::string& name) {
  ThreadIdNameManager::GetInstance()->SetName(name);
}

}  // namespace base
