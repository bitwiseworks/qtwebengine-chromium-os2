// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/cursors/webcursor.h"

#include "base/logging.h"

namespace content {

#if defined(USE_AURA)
ui::PlatformCursor WebCursor::GetPlatformCursor(const ui::Cursor& cursor) {
  // TODO: Implement it on OS/2.
  return nullptr;
}
#endif

bool WebCursor::IsPlatformDataEqual(const WebCursor& other) const {
  return true;
}

void WebCursor::CleanupPlatformData() {}

void WebCursor::CopyPlatformData(const WebCursor& other) {}

}  // namespace content
