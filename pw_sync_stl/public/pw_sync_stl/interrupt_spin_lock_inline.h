// Copyright 2020 The Pigweed Authors
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

#include <thread>

#include "pw_assert/assert.h"
#include "pw_sync/interrupt_spin_lock.h"

namespace pw::sync {

constexpr InterruptSpinLock::InterruptSpinLock() : native_type_(false) {}

inline void InterruptSpinLock::lock() {
  while (!try_lock()) {
    std::this_thread::yield();
  }
}

inline bool InterruptSpinLock::try_lock() {
  return !native_type_.exchange(true, std::memory_order_acquire);
}

inline void InterruptSpinLock::unlock() {
  PW_ASSERT(native_type_.exchange(false, std::memory_order_release));
}

inline InterruptSpinLock::native_handle_type
InterruptSpinLock::native_handle() {
  return native_type_;
}

}  // namespace pw::sync
