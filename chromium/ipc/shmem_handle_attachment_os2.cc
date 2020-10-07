// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/shmem_handle_attachment_os2.h"

namespace IPC {
namespace internal {

ShmemHandleAttachmentOS2::ShmemHandleAttachmentOS2(const SHMEM& handle) {
  SHMEM duplicated_handle;
  int rc = shmem_duplicate(handle, 0);
  if (rc != -1) {
    handle_.reset(duplicated_handle);
  }
}

ShmemHandleAttachmentOS2::ShmemHandleAttachmentOS2(const SHMEM& handle,
                                                   FromWire from_wire)
    : handle_(handle) {}

ShmemHandleAttachmentOS2::~ShmemHandleAttachmentOS2() {}

MessageAttachment::Type ShmemHandleAttachmentOS2::GetType() const {
  return Type::OS2_SHMEM_HANDLE;
}

}  // namespace internal
}  // namespace IPC
