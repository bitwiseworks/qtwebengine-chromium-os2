// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/time_zone_monitor/time_zone_monitor.h"

#include <memory>

#include "base/logging.h"

namespace device {
namespace {

// TODO: Implement this on OS/2.
class TimeZoneMonitorOS2 : public TimeZoneMonitor {
 public:
  TimeZoneMonitorOS2() = default;
  ~TimeZoneMonitorOS2() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(TimeZoneMonitorOS2);
};

}  // namespace

// static
std::unique_ptr<TimeZoneMonitor> TimeZoneMonitor::Create(
    scoped_refptr<base::SequencedTaskRunner> file_task_runner) {
  // TODO: Implement this on OS/2.
  NOTIMPLEMENTED();

  return std::make_unique<TimeZoneMonitorOS2>();
}

}  // namespace device
