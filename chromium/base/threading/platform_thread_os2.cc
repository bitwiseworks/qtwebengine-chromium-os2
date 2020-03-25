// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/platform_thread.h"

#include "base/threading/platform_thread_internal_posix.h"
#include "base/threading/thread_id_name_manager.h"

namespace base {

namespace internal {

// kLIBC maps nice values to OS/2 priorities in __libc_back_priorityOS2FromUnix,
// consult it for details.
const ThreadPriorityToNiceValuePair kThreadPriorityToNiceValueMap[4] = {
    {ThreadPriority::BACKGROUND, 20}, // PRTYC_IDLETIME + 0
    {ThreadPriority::NORMAL, 0}, // PRTYC_REGULAR + 0
    {ThreadPriority::DISPLAY, -10}, // PRTYC_FOREGROUNDSERVER + 0
    {ThreadPriority::REALTIME_AUDIO, -18}, // PRTYC_TIMECRITICAL + 0
};

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
