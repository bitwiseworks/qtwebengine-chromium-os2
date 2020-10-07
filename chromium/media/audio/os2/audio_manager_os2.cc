// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/os2/audio_manager_os2.h"

#include <memory>

namespace media {

AudioManagerOS2::AudioManagerOS2(
    std::unique_ptr<AudioThread> audio_thread,
    AudioLogFactory* audio_log_factory)
    : AudioManagerBase(std::move(audio_thread), audio_log_factory) {}

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
  NOTREACHED();
  return AudioParameters();
}

AudioParameters AudioManagerOS2::GetPreferredOutputStreamParameters(
    const std::string& output_device_id,
    const AudioParameters& input_params) {
  // TODO: Request real configuration from the device.
  return AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                         CHANNEL_LAYOUT_STEREO, 48000, 480);
}

const char* AudioManagerOS2::GetName() {
  return "OS/2";
}

AudioOutputStream* AudioManagerOS2::MakeLinearOutputStream(
    const AudioParameters& params,
    const LogCallback& log_callback) {
  NOTREACHED();
  return nullptr;
}

AudioOutputStream* AudioManagerOS2::MakeLowLatencyOutputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());

  if (!device_id.empty() &&
      device_id != AudioDeviceDescription::kDefaultDeviceId) {
    return nullptr;
  }

  // TODO: Implement it on OS/2.
  NOTIMPLEMENTED();
  return nullptr;
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
