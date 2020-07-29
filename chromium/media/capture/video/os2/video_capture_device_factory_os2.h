// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_FUCHSIA_VIDEO_CAPTURE_DEVICE_FACTORY_OS2_H_
#define MEDIA_CAPTURE_VIDEO_FUCHSIA_VIDEO_CAPTURE_DEVICE_FACTORY_OS2_H_

#include "media/capture/video/video_capture_device_factory.h"

namespace media {

class CAPTURE_EXPORT VideoCaptureDeviceFactoryOS2
    : public VideoCaptureDeviceFactory {
 public:
  VideoCaptureDeviceFactoryOS2();
  ~VideoCaptureDeviceFactoryOS2() override;

  std::unique_ptr<VideoCaptureDevice> CreateDevice(
      const VideoCaptureDeviceDescriptor& device_descriptor) override;
  void GetDeviceDescriptors(
      VideoCaptureDeviceDescriptors* device_descriptors) override;
  void GetSupportedFormats(const VideoCaptureDeviceDescriptor& device,
                           VideoCaptureFormats* supported_formats) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoCaptureDeviceFactoryOS2);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_FUCHSIA_VIDEO_CAPTURE_DEVICE_FACTORY_OS2_H_
