// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This header is a single source of inclusion of all OS/2 Toolkit headers.
// It shall be used instead of direct inclusion of <os2.h> which is driven
// by INCL_xxx defines and therefore is subject to conflicts when multiple
// headers including it end up in a single compliation unit but disagree about
// these INCL_xxx directives. Depending on the eventual order of inclusion,
// parts of requested definitions might be missing despite INCL_xxx specs
// because of a single inclusion guard present in <os2.h>.

// This header is also a place to fix conflicts between OS/2 definitions
// and Chromium definitions.

// Note that normally this header should not be used in third_party sources
// because it doesn't belong to them and might cause problems when syncing
// with their upstream repositories.

#ifndef BASE_OS2_OS2_TOOLKIT_H
#define BASE_OS2_OS2_TOOLKIT_H

// Make PSZ char * instead of unsigned char * (for compatibility with C/C++).
#define OS2EMX_PLAIN_CHAR

// Drag in as much as we might need anywhere in Chromium.
#define INCL_BASE
#define INCL_PM
#define INCL_EXAPIS
#include <os2.h>

// Fix a clash with a decl in generated file_chooser.mojom-shared-internal.h.
#undef FILE_SYSTEM

#endif  // BASE_OS2_OS2_TOOLKIT_H
