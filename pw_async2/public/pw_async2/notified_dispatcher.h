// Copyright 2025 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.
#pragma once

#include "pw_async2/runnable_dispatcher.h"
#include "pw_sync/thread_notification.h"

namespace pw::async2 {

/// @submodule{pw_async2,dispatchers}

/// Simple `RunnableDispatcher` implementation that uses a
/// `pw::sync::ThreadNotification` to wait for tasks to wake.
///
/// The thread notification must be valid for the lifetime of this object.
class NotifiedDispatcher : public RunnableDispatcher {
 public:
  constexpr explicit NotifiedDispatcher(sync::ThreadNotification& notify)
      : notify_(notify) {}

  ~NotifiedDispatcher() override { Terminate(); }

  sync::ThreadNotification& notification() const { return notify_; }

 private:
  void DoWake() override { notify_.release(); }

  void DoWaitForWake() override { notify_.acquire(); }

  sync::ThreadNotification& notify_;
};

/// @endsubmodule

}  // namespace pw::async2
