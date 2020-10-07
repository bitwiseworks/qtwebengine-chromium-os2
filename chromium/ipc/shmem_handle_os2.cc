// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/shmem_handle_os2.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "ipc/shmem_handle_attachment_os2.h"
#include "ipc/ipc_message.h"

namespace IPC {

ShmemHandleOS2::ShmemHandleOS2() : handle_(SHMEM_INVALID) {}

ShmemHandleOS2::ShmemHandleOS2(const SHMEM& handle) : handle_(handle) {}

// static
void ParamTraits<ShmemHandleOS2>::Write(base::Pickle* m, const param_type& p) {
  scoped_refptr<IPC::internal::ShmemHandleAttachmentOS2> attachment(
      new IPC::internal::ShmemHandleAttachmentOS2(p.get_handle()));
  if (!m->WriteAttachment(std::move(attachment)))
    NOTREACHED();
}

// static
bool ParamTraits<ShmemHandleOS2>::Read(const base::Pickle* m,
                                       base::PickleIterator* iter,
                                       param_type* r) {
  scoped_refptr<base::Pickle::Attachment> base_attachment;
  if (!m->ReadAttachment(iter, &base_attachment))
    return false;
  MessageAttachment* attachment =
      static_cast<MessageAttachment*>(base_attachment.get());
  if (attachment->GetType() != MessageAttachment::Type::OS2_SHMEM_HANDLE)
    return false;
  IPC::internal::ShmemHandleAttachmentOS2* handle_attachment =
      static_cast<IPC::internal::ShmemHandleAttachmentOS2*>(attachment);
  r->set_handle(handle_attachment->Take());
  return true;
}

// static
void ParamTraits<ShmemHandleOS2>::Log(const param_type& p, std::string* l) {
  l->append(base::StringPrintf("%u", p.get_handle()));
}

}  // namespace IPC
