// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process_metrics.h"

namespace base {

size_t GetSystemCommitCharge() {
  // TODO https://github.com/bitwiseworks/qtwebengine-chromium-os2/issues/8
  NOTIMPLEMENTED_LOG_ONCE();
  return 0;
}

// static
std::unique_ptr<ProcessMetrics> ProcessMetrics::CreateProcessMetrics(
    ProcessHandle process) {
  // TODO https://github.com/bitwiseworks/qtwebengine-chromium-os2/issues/8
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
}

TimeDelta ProcessMetrics::GetCumulativeCPUUsage() {
  // TODO https://github.com/bitwiseworks/qtwebengine-chromium-os2/issues/8
  NOTIMPLEMENTED_LOG_ONCE();
  return TimeDelta();
}

bool GetSystemMemoryInfo(SystemMemoryInfoKB* meminfo) {
  // TODO https://github.com/bitwiseworks/qtwebengine-chromium-os2/issues/8
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

}  // namespace base
