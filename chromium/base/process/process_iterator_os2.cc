// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process_iterator.h"

namespace base {

ProcessIterator::ProcessIterator(const ProcessFilter* filter) {
  // TODO https://github.com/bitwiseworks/qtwebengine-chromium-os2/issues/9
  NOTREACHED();
}

ProcessIterator::~ProcessIterator() {}

bool ProcessIterator::CheckForNextProcess() {
  // TODO https://github.com/bitwiseworks/qtwebengine-chromium-os2/issues/9
  return false;
}

bool NamedProcessIterator::IncludeEntry() {
  // TODO https://github.com/bitwiseworks/qtwebengine-chromium-os2/issues/9
  return false;
}

}  // namespace base
