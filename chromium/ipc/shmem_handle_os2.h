// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_SHMEM_HANDLE_OS2_H_
#define IPC_SHMEM_HANDLE_OS2_H_

#include <libcx/shmem.h>

#include <string>

#include "ipc/ipc_message_support_export.h"
#include "ipc/ipc_param_traits.h"

namespace base {
class Pickle;
class PickleIterator;
}  // namespace base

namespace IPC {

// HandleWin is a wrapper around a LIBCx SHMEM handle that can be transported
// across Chrome IPC channels that support attachment brokering. The SHMEM
// handle will be duplicated into the destination process.
//
// The ownership semantics for the underlying |handle_| are complex. See
// ipc/mach_port_mac.h (the OSX analog of this class) for an extensive
// discussion.
class IPC_MESSAGE_SUPPORT_EXPORT ShmemHandleOS2 {
 public:
  // Default constructor makes an invalid SHMEM.
  ShmemHandleOS2();
  explicit ShmemHandleOS2(const SHMEM& handle);

  SHMEM get_handle() const { return handle_; }
  void set_handle(SHMEM handle) { handle_ = handle; }

 private:
  SHMEM handle_;
};

template <>
struct IPC_MESSAGE_SUPPORT_EXPORT ParamTraits<ShmemHandleOS2> {
  typedef ShmemHandleOS2 param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* p);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // IPC_SHMEM_HANDLE_OS2_H_
