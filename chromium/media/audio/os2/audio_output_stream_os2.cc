// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/os2/audio_output_stream_os2.h"

#include "media/audio/os2/audio_manager_os2.h"
#include "media/base/audio_sample_types.h"
#include "media/base/audio_timestamp_helper.h"

namespace media {

AudioOutputStreamOS2::AudioOutputStreamOS2(
    AudioManagerOS2* manager,
    const AudioParameters& parameters)
    : manager_(manager),
      parameters_(parameters),
      audio_bus_(AudioBus::Create(parameters)) {}

AudioOutputStreamOS2::~AudioOutputStreamOS2() {
  // Close() must be called first.
  DCHECK(!handle_);
}

bool AudioOutputStreamOS2::Open() {
  DCHECK(!handle_);

  KAISPEC wanted, obtained;
  memset(&wanted, 0, sizeof(wanted));
  wanted.ulType = KAIT_PLAY;
  wanted.ulBitsPerSample = kOS2BitsPerSample;
  wanted.ulSamplingRate = parameters_.sample_rate();
  wanted.ulChannels = parameters_.channels() >= kOS2MaxChannels ? 2 : 1;
  wanted.pfnCallBack = kaiCallback;
  wanted.pCallBackData = this;

  APIRET arc = kaiOpen(&wanted, &obtained, &handle_);
  if (arc) {
    LOG(ERROR) << "kaiOpen returned " << (LONG)arc << " for sampling rate "
               << wanted.ulSamplingRate << " and channels "
               << wanted.ulChannels;
    return false;
  }

  return true;
}

void AudioOutputStreamOS2::Start(AudioSourceCallback* callback) {
  DCHECK(handle_);
  DCHECK(!callback_);

  callback_ = callback;

  // Before starting, the buffer might have audio from previous user of this
  // device.
  kaiClearBuffer(handle_);

  APIRET arc = kaiPlay(handle_);
  if (arc) {
    LOG(ERROR) << "kaiPlay("<< std::hex << handle_ << ") returned "
               << std::dec << (LONG)arc;
    callback_->OnError(AudioSourceCallback::ErrorType::kUnknown);
  }
}

void AudioOutputStreamOS2::Stop() {
  DCHECK(handle_);

  APIRET arc = kaiStop(handle_);
  if (arc) {
    LOG(ERROR) << "kaiStop("<< std::hex << handle_ << ") returned "
               << std::dec << (LONG)arc;
    if (callback_)
      callback_->OnError(AudioSourceCallback::ErrorType::kUnknown);
  }

  callback_ = nullptr;
}

// This stream is always used with sub second buffer sizes, where it's
// sufficient to simply always flush upon Start().
void AudioOutputStreamOS2::Flush() {}

void AudioOutputStreamOS2::SetVolume(double volume) {
  DCHECK(0.0 <= volume && volume <= 1.0) << volume;
  volume_ = volume;
}

void AudioOutputStreamOS2::GetVolume(double* volume) {
  *volume = volume_;
}

void AudioOutputStreamOS2::Close() {
  DCHECK(handle_);

  Stop();

  kaiClose(handle_);
  handle_ = 0;

  // Signal to the manager that we're closed and can be removed. This should be
  // the last call in the function as it deletes |this|.
  manager_->ReleaseOutputStream(this);
}

ULONG AudioOutputStreamOS2::Callback(PVOID buf, ULONG size) {
  DCHECK(callback_);

  int frames_filled =
      callback_->OnMoreData(base::TimeDelta(), base::TimeTicks::Now(),
                            0, audio_bus_.get());

  uint32_t used = frames_filled * audio_bus_->channels() * kOS2BitsPerSample / 8;

  audio_bus_->Scale(volume_);

  if (used <= size) {
    // Note: If this ever changes to output raw float the data must be clipped
    // and sanitized since it may come from an untrusted source such as NaCl.
    audio_bus_->ToInterleaved<SignedInt16SampleTypeTraits>(
        frames_filled, reinterpret_cast<int16_t*>(buf));
    return used;
  }

  // Error.
  callback_->OnError(AudioSourceCallback::ErrorType::kUnknown);
  return 0;
}

// static
ULONG APIENTRY AudioOutputStreamOS2::kaiCallback(PVOID data, PVOID buf,
                                                 ULONG size) {
  AudioOutputStreamOS2* stream = reinterpret_cast<AudioOutputStreamOS2*>(data);
  DCHECK(stream);
  return stream->Callback(buf, size);
}

}  // namespace media
