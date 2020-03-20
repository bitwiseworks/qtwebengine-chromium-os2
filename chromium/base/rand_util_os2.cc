// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/rand_util.h"

#include <stdlib.h>

#include "base/lazy_instance.h"

namespace {

// OS/2 doesn't have /dev/[u]random, use BSD random instead.
class BSDRandom {
 public:
  BSDRandom() { srandomdev(); }
};

base::LazyInstance<BSDRandom>::Leaky g_bsd_random = LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace base {

void RandBytes(void* output, size_t output_length) {
  // trigger a thread-safe one-time srandomdev() call.
  g_bsd_random.Pointer();
  char* output_ptr = static_cast<char*>(output);
  while (output_length > 0) {
    // Random retuns a 31 bit random number, use the first 24 bits.
    size_t len = std::min(output_length, 3U);
    long r = random();
    output_length -= len;
    while (len--) {
      *output_ptr++ = r;
      r >>= 8;
    }
  }
}

}  // namespace base
