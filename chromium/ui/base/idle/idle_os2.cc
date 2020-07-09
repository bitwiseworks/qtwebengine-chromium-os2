// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/idle/idle.h"

#include "base/logging.h"

namespace ui {

int CalculateIdleTime() {
  // TODO: Implement this on OS/2.
  NOTIMPLEMENTED();
  return 0;
}

bool CheckIdleStateIsLocked() {
  // TODO: Implement this on OS/2.
  NOTIMPLEMENTED();
  return false;
}

}  // namespace ui
