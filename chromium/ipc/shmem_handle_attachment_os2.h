// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_SHMEM_HANDLE_ATTACHMENT_OS2_H_
#define IPC_SHMEM_HANDLE_ATTACHMENT_OS2_H_

#include "base/os2/scoped_shmem_handle.h"
#include "ipc/shmem_handle_os2.h"
#include "ipc/ipc_message_attachment.h"
#include "ipc/ipc_message_support_export.h"

namespace IPC {
namespace internal {

// This class represents a LIBCx SHMEM handle attached to a Chrome IPC message.
class IPC_MESSAGE_SUPPORT_EXPORT ShmemHandleAttachmentOS2
    : public MessageAttachment {
 public:
  // This constructor makes a copy of |handle| and takes ownership of the
  // result. Should only be called by the sender of a Chrome IPC message.
  explicit ShmemHandleAttachmentOS2(const SHMEM& handle);

  enum FromWire {
    FROM_WIRE,
  };
  // This constructor takes ownership of |handle|. Should only be called by the
  // receiver of a Chrome IPC message.
  ShmemHandleAttachmentOS2(const SHMEM& handle, FromWire from_wire);

  // MessageAttachment interface.
  Type GetType() const override;

  SHMEM Take() { return handle_.release(); }

 private:
  ~ShmemHandleAttachmentOS2() override;

  base::os2::ScopedShmemHandle handle_;
};

}  // namespace internal
}  // namespace IPC

#endif  // IPC_SHMEM_HANDLE_ATTACHMENT_OS2_H_
