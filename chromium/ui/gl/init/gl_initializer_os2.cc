// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/init/gl_initializer.h"

#include "base/logging.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_surface.h"

namespace gl {
namespace init {

bool InitializeGLOneOffPlatform() {
  // TODO: Implement it on OS/2.

  switch (GetGLImplementation()) {
    case kGLImplementationMockGL:
    case kGLImplementationStubGL:
      return true;
    default:
      NOTREACHED();
  }
  return false;
}

bool InitializeStaticGLBindings(GLImplementation implementation) {
  // Prevent reinitialization with a different implementation. Once the gpu
  // unit tests have initialized with kGLImplementationMock, we don't want to
  // later switch to another GL implementation.
  DCHECK_EQ(kGLImplementationNone, GetGLImplementation());

  // TODO: Implement it on OS/2.

  switch (implementation) {
    case kGLImplementationMockGL:
    case kGLImplementationStubGL:
      SetGLImplementation(implementation);
      InitializeStaticGLBindingsGL();
      return true;
    default:
      NOTREACHED();
  }

  return false;
}

void InitializeDebugGLBindings() {
  // TODO: Implement it on OS/2.

  InitializeDebugGLBindingsGL();
}

void ShutdownGLPlatform() {
  // TODO: Implement it on OS/2.

  ClearBindingsGL();
}

}  // namespace init
}  // namespace gl
