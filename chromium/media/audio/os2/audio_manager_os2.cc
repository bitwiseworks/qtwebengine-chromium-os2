// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/os2/audio_manager_os2.h"

#include "media/audio/os2/audio_output_stream_os2.h"

#include <kai.h>

#include <memory>

namespace media {

AudioManagerOS2::AudioManagerOS2(
    std::unique_ptr<AudioThread> audio_thread,
    AudioLogFactory* audio_log_factory)
    : AudioManagerBase(std::move(audio_thread), audio_log_factory) {
  // Initialize the KAI subsystem.
  APIRET arc = kaiInit(KAIM_AUTO);
  if (arc)
    LOG(ERROR) << "kaiInit() returned " << (LONG)arc;
}

AudioManagerOS2::~AudioManagerOS2() = default;

bool AudioManagerOS2::HasAudioOutputDevices() {
  // TODO: Implement it on OS/2.
  return false;
}

bool AudioManagerOS2::HasAudioInputDevices() {
  NOTIMPLEMENTED();
  return false;
}

void AudioManagerOS2::GetAudioInputDeviceNames(
    AudioDeviceNames* device_names) {
  device_names->clear();
  NOTIMPLEMENTED();
}

void AudioManagerOS2::GetAudioOutputDeviceNames(
    AudioDeviceNames* device_names) {
  // TODO: Implement device enumeration.
  *device_names = {AudioDeviceName::CreateDefault()};
}

AudioParameters AudioManagerOS2::GetInputStreamParameters(
    const std::string& device_id) {
  NOTIMPLEMENTED();
  return AudioParameters();
}

AudioParameters AudioManagerOS2::GetPreferredOutputStreamParameters(
    const std::string& output_device_id,
    const AudioParameters& input_params) {
  KAICAPS caps;
  APIRET arc = kaiCaps(&caps);
  if (arc) {
    LOG(ERROR) << "kaiCaps() returned " << (LONG)arc;
    return AudioParameters();
  }
  VLOG(1) << "input_params rate " << input_params.sample_rate()
          << ", layout " << input_params.channel_layout()
          << ", channels " << input_params.channels()
          << ", frames per buf " << input_params.frames_per_buffer();
  VLOG(1) << "kai mode " << (caps.ulMode == KAIM_DART ? "DART" : "UniAud")
          << ", max channels " << caps.ulMaxChannels
          << ", name " << caps.szPDDName;

  // Try to open the device to get the optimal frequency and buffer size.
  HKAI handle;
  KAISPEC wanted, obtained;
  memset(&wanted, 0, sizeof(wanted));
  wanted.ulType = KAIT_PLAY;
  wanted.ulBitsPerSample = kOS2BitsPerSample;
  wanted.ulSamplingRate = input_params.sample_rate();
  wanted.ulChannels = input_params.channels() >= kOS2MaxChannels ? 2 : 1;
  wanted.pfnCallBack = (PFNKAICB)this; // Any valid pointer, won't be used anyway.

  arc = kaiOpen(&wanted, &obtained, &handle);
  if (arc) {
    LOG(ERROR) << "kaiOpen returned " << (LONG)arc << " for sampling rate "
               << wanted.ulSamplingRate << " and channels "
               << wanted.ulChannels;
  }
  kaiClose(handle);

  int frames_per_buffer =
      obtained.ulBufferSize / obtained.ulChannels / (kOS2BitsPerSample / 8);

  VLOG(1) << "optimal rate " << obtained.ulSamplingRate
          << ", channels " << obtained.ulChannels
          << ", bufsize " << obtained.ulBufferSize
          << ", frames per buf " << frames_per_buffer;

  return AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                         obtained.ulChannels == 2 ?
                             CHANNEL_LAYOUT_STEREO : CHANNEL_LAYOUT_MONO,
                         obtained.ulSamplingRate, frames_per_buffer);
}

const char* AudioManagerOS2::GetName() {
  return "OS/2";
}

AudioOutputStream* AudioManagerOS2::MakeLinearOutputStream(
    const AudioParameters& params,
    const LogCallback& log_callback) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format());
  if (params.channels() > kOS2MaxChannels)
    return nullptr;

  NOTREACHED();
  return nullptr;
}

AudioOutputStream* AudioManagerOS2::MakeLowLatencyOutputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());
  if (params.channels() > kOS2MaxChannels)
    return nullptr;

  if (!device_id.empty() &&
      device_id != AudioDeviceDescription::kDefaultDeviceId) {
    return nullptr;
  }

  return new AudioOutputStreamOS2(this, params);
}

AudioInputStream* AudioManagerOS2::MakeLinearInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  NOTREACHED();
  return nullptr;
}

AudioInputStream* AudioManagerOS2::MakeLowLatencyInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  NOTREACHED();
  return nullptr;
}

std::unique_ptr<AudioManager> CreateAudioManager(
    std::unique_ptr<AudioThread> audio_thread,
    AudioLogFactory* audio_log_factory) {
  return std::make_unique<AudioManagerOS2>(std::move(audio_thread),
                                               audio_log_factory);
}

}  // namespace media
