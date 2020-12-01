// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_OS2_AUDIO_OUTPUT_STREAM_OS2_H_
#define MEDIA_AUDIO_OS2_AUDIO_OUTPUT_STREAM_OS2_H_

#include "media/audio/audio_io.h"
#include "media/base/audio_parameters.h"

#include <kai.h>

namespace media {

constexpr int kOS2MaxChannels = 2;
constexpr int kOS2BitsPerSample = 16;
constexpr SampleFormat kOS2SampleFormat = kSampleFormatS16;

class AudioManagerOS2;

class AudioOutputStreamOS2 : public AudioOutputStream {
 public:
  // Caller must ensure that manager outlives the stream.
  AudioOutputStreamOS2(AudioManagerOS2* manager,
                       const AudioParameters& parameters);

  // AudioOutputStream interface.
  bool Open() override;
  void Start(AudioSourceCallback* callback) override;
  void Stop() override;
  void SetVolume(double volume) override;
  void GetVolume(double* volume) override;
  void Close() override;

 private:
  ~AudioOutputStreamOS2() override;

  ULONG Callback(PVOID buf, ULONG size);

  static ULONG APIENTRY kaiCallback(PVOID data, PVOID buf, ULONG size);

  AudioManagerOS2* manager_;
  AudioParameters parameters_;

  double volume_ = 1.0;

  AudioSourceCallback* callback_ = nullptr;

  // Container for retrieving data from AudioSourceCallback::OnMoreData().
  std::unique_ptr<AudioBus> audio_bus_;

  HKAI handle_ = 0;

  DISALLOW_COPY_AND_ASSIGN(AudioOutputStreamOS2);
};

}  // namespace media

#endif  // MEDIA_AUDIO_OS2_AUDIO_OUTPUT_STREAM_OS2_H_
