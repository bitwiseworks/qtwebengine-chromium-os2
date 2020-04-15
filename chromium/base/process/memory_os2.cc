// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/memory.h"

#include <new>
#include <stdlib.h>

namespace base {

namespace {

void OnNoMemory() {
  TerminateBecauseOutOfMemory(0);
}

}  // namespace

void EnableTerminationOnOutOfMemory() {
  // Set the new-out of memory handler.
  std::set_new_handler(&OnNoMemory);
  // TODO: Figure out how to catch malloc failures in kLIBC.
}

void EnableTerminationOnHeapCorruption() {
  // TODO: Figure out how to catch heap corruption in kLIBC.
}

bool UncheckedMalloc(size_t size, void** result) {
  *result = malloc(size);
  return *result != nullptr;
}

}  // namespace base
