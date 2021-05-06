// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CURSOR_CURSOR_LOADER_OS2_H_
#define UI_BASE_CURSOR_CURSOR_LOADER_OS2_H_

#include <map>

#include "base/macros.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/cursor_loader.h"

namespace ui {

class CursorFactoryOS2;

class UI_BASE_EXPORT CursorLoaderOS2 : public CursorLoader {
 public:
  CursorLoaderOS2();
  ~CursorLoaderOS2() override;

  // CursorLoader overrides:
  void LoadImageCursor(mojom::CursorType id,
                       int resource_id,
                       const gfx::Point& hot) override;
  void LoadAnimatedCursor(mojom::CursorType id,
                          int resource_id,
                          const gfx::Point& hot,
                          int frame_delay_ms) override;
  void UnloadAll() override;
  void SetPlatformCursor(gfx::NativeCursor* cursor) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CursorLoaderOS2);
};

}  // namespace ui

#endif  // UI_BASE_CURSOR_CURSOR_LOADER_OS2_H_
