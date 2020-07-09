// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/swap_metrics_driver.h"

#include <memory>

#include "base/time/time.h"

namespace content {

// static
std::unique_ptr<SwapMetricsDriver> SwapMetricsDriver::Create(
    std::unique_ptr<Delegate> delegate,
    const base::TimeDelta update_interval) {
  // SwapMetricsDriver isn't available on OS/2 for now.
  // TODO: Figure out a way to measure swap rates on OS/2.
  return std::unique_ptr<SwapMetricsDriver>();
}

}  // namespace content
