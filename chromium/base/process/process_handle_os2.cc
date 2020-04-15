// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process_handle.h"

#include "base/stl_util.h"

#include <stdlib.h>
#include <sys/process.h>

namespace base {

ProcessId GetParentProcessId(ProcessHandle process) {
  return getppid();
}

FilePath GetProcessExecutablePath(ProcessHandle process) {
  char pathname[PATH_MAX];

  if (_execname(pathname, PATH_MAX) == -1) {
    return FilePath();
  }

  return FilePath(std::string(pathname));
}

}  // namespace base
