/* Copyright (c) 2020, bww bitwise works GmbH
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

#define _BSD_SOURCE // for arc4random from stdlib.h

#include <openssl/rand.h>

#if defined(OPENSSL_OS2) && !defined(BORINGSSL_UNSAFE_DETERMINISTIC_MODE)

#include <limits.h>
#include <stdlib.h>

#include "../fipsmodule/rand/internal.h"

void CRYPTO_sysrand(uint8_t *out, size_t requested) {
  // TODO: Port arc4random_buf to OS/2 (via LIBCn) and use it.
  if (requested) {
    uint8_t *p = out;
    size_t i = 0;
    for (; i < requested / 4; ++i, p += 4)
      *((uint32_t *)p) = arc4random();
    i = requested % 4;
    if (i) {
      uint32_t r = arc4random();
      memcpy(p, &r, i);
    }
  }
}

#endif  // OPENSSL_OS2 && !BORINGSSL_UNSAFE_DETERMINISTIC_MODE
