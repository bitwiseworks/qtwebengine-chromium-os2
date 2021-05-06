// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cursor/cursor_loader_os2.h"

#include <vector>

#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/cursor_util.h"

namespace ui {

CursorLoaderOS2::CursorLoaderOS2() {
}

CursorLoaderOS2::~CursorLoaderOS2() {
  UnloadAll();
}

void CursorLoaderOS2::LoadImageCursor(mojom::CursorType id,
                                      int resource_id,
                                      const gfx::Point& hot) {
  // TODO: Implement this on OS/2.
  NOTIMPLEMENTED();
}

void CursorLoaderOS2::LoadAnimatedCursor(mojom::CursorType id,
                                         int resource_id,
                                         const gfx::Point& hot,
                                         int frame_delay_ms) {
  // TODO: Implement this on OS/2.
  NOTIMPLEMENTED();
}

void CursorLoaderOS2::UnloadAll() {
}

void CursorLoaderOS2::SetPlatformCursor(gfx::NativeCursor* cursor) {
  // TODO: Implement this on OS/2.
  NOTIMPLEMENTED();
}

CursorLoader* CursorLoader::Create() {
  return new CursorLoaderOS2();
}

}  // namespace ui
