// Copyright 2017 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// This file is a Posix-specific part of spinlock_wait.cc

#ifdef __OS2__
#include <unistd.h>
#else
#include <sched.h>
#endif

#include <atomic>
#include <ctime>

#include "absl/base/internal/errno_saver.h"
#include "absl/base/internal/scheduling_mode.h"
#include "absl/base/port.h"

extern "C" {

ABSL_ATTRIBUTE_WEAK void AbslInternalSpinLockDelay(
    std::atomic<uint32_t>* /* lock_word */, uint32_t /* value */, int loop,
    absl::base_internal::SchedulingMode /* mode */) {
  absl::base_internal::ErrnoSaver errno_saver;
  if (loop == 0) {
  } else if (loop == 1) {
#ifdef __OS2__
    // This will end up in DosSleep(0) which does yielding.
    sleep(0);
#else
    sched_yield();
#endif
  } else {
    struct timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = absl::base_internal::SpinLockSuggestedDelayNS(loop);
    nanosleep(&tm, nullptr);
  }
}

ABSL_ATTRIBUTE_WEAK void AbslInternalSpinLockWake(
    std::atomic<uint32_t>* /* lock_word */, bool /* all */) {}

}  // extern "C"
