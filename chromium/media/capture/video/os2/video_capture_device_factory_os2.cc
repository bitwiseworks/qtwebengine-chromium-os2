// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/os2/video_capture_device_factory_os2.h"

#include "base/logging.h"

namespace media {

VideoCaptureDeviceFactoryOS2::VideoCaptureDeviceFactoryOS2() = default;
VideoCaptureDeviceFactoryOS2::~VideoCaptureDeviceFactoryOS2() = default;

std::unique_ptr<VideoCaptureDevice>
VideoCaptureDeviceFactoryOS2::CreateDevice(
    const VideoCaptureDeviceDescriptor& device_descriptor) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  NOTIMPLEMENTED();
  return nullptr;
}

void VideoCaptureDeviceFactoryOS2::GetDeviceDescriptors(
    VideoCaptureDeviceDescriptors* device_descriptors) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  device_descriptors->clear();
}

void VideoCaptureDeviceFactoryOS2::GetSupportedFormats(
    const VideoCaptureDeviceDescriptor& device,
    VideoCaptureFormats* capture_formats) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  capture_formats->clear();
}

}  // namespace media
