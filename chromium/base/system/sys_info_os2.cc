// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/os2/os2_toolkit.h"

#include "base/system/sys_info.h"

#include "base/logging.h"

namespace base {

// static
int64_t SysInfo::AmountOfPhysicalMemoryImpl() {
  ULONG data;
  if (DosQuerySysInfo(QSV_TOTPHYSMEM, QSV_TOTPHYSMEM, &data, sizeof(data)))
    return 0;
  return data;
}

// static
int64_t SysInfo::AmountOfAvailablePhysicalMemoryImpl() {
  ULONG data;
  if (DosQuerySysInfo(QSV_TOTAVAILMEM, QSV_TOTAVAILMEM, &data, sizeof(data)))
    return 0;
  return data;
}

}  // namespace base
