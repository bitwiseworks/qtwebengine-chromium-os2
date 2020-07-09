// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/child_process_launcher_helper.h"

#include "base/command_line.h"
#include "base/process/launch.h"
#include "content/browser/child_process_launcher.h"
#include "content/public/browser/child_process_launcher_utils.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "services/service_manager/embedder/result_codes.h"

namespace content {
namespace internal {

void ChildProcessLauncherHelper::SetProcessPriorityOnLauncherThread(
    base::Process process,
    const ChildProcessLauncherPriority& priority) {
  DCHECK(CurrentlyOnProcessLauncherTaskRunner());
  // TODO: Implement this on OS/2.
  NOTIMPLEMENTED();
}

ChildProcessTerminationInfo ChildProcessLauncherHelper::GetTerminationInfo(
    const ChildProcessLauncherHelper::Process& process,
    bool known_dead) {
  ChildProcessTerminationInfo info;
  info.status =
      base::GetTerminationStatus(process.process.Handle(), &info.exit_code);
  return info;
}

// static
bool ChildProcessLauncherHelper::TerminateProcess(const base::Process& process,
                                                  int exit_code) {
  return process.Terminate(exit_code, false);
}

// static
void ChildProcessLauncherHelper::SetRegisteredFilesForService(
    const std::string& service_name,
    std::map<std::string, base::FilePath> required_files) {
  // TODO: Implement this on OS/2.
  NOTIMPLEMENTED();
}

// static
void ChildProcessLauncherHelper::ResetRegisteredFilesForTesting() {
  // TODO: Implement this on OS/2.
  NOTIMPLEMENTED();
}

void ChildProcessLauncherHelper::BeforeLaunchOnClientThread() {
  DCHECK_CURRENTLY_ON(client_thread_id_);
}

base::Optional<mojo::NamedPlatformChannel>
ChildProcessLauncherHelper::CreateNamedPlatformChannelOnClientThread() {
  DCHECK_CURRENTLY_ON(client_thread_id_);
  return base::nullopt;
}

std::unique_ptr<FileMappedForLaunch>
ChildProcessLauncherHelper::GetFilesToMap() {
  DCHECK(CurrentlyOnProcessLauncherTaskRunner());
  return std::unique_ptr<FileMappedForLaunch>();
}

bool ChildProcessLauncherHelper::BeforeLaunchOnLauncherThread(
    const PosixFileDescriptorInfo& files_to_register,
    base::LaunchOptions* options) {
  DCHECK(CurrentlyOnProcessLauncherTaskRunner());

  return true;
}

ChildProcessLauncherHelper::Process
ChildProcessLauncherHelper::LaunchProcessOnLauncherThread(
    const base::LaunchOptions& options,
    std::unique_ptr<FileMappedForLaunch> files_to_register,
    bool* is_synchronous_launch,
    int* launch_result) {
  DCHECK(CurrentlyOnProcessLauncherTaskRunner());
  DCHECK(mojo_channel_);
  DCHECK(mojo_channel_->remote_endpoint().is_valid());

  // TODO: Implement sandboxed/isolated subprocess launching on OS/2.
  Process child_process;
  child_process.process = base::LaunchProcess(*command_line(), options);
  return child_process;
}

void ChildProcessLauncherHelper::AfterLaunchOnLauncherThread(
    const ChildProcessLauncherHelper::Process& process,
    const base::LaunchOptions& options) {
}

// static
void ChildProcessLauncherHelper::ForceNormalProcessTerminationSync(
    ChildProcessLauncherHelper::Process process) {
  DCHECK(CurrentlyOnProcessLauncherTaskRunner());
  process.process.Terminate(service_manager::RESULT_CODE_NORMAL_EXIT, true);
}

}  // namespace internal
}  // namespace content