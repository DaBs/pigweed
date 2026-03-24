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

#include "pw_sync/timed_mutex.h"

#include <zephyr/kernel.h>

#include <limits>

#include "pw_assert/check.h"
#include "pw_chrono/system_clock.h"
#include "pw_interrupt/context.h"

using pw::chrono::SystemClock;

namespace pw::sync {

bool TimedMutex::try_lock_for(SystemClock::duration timeout) {
  // Enforce the pw::sync::TimedMutex IRQ contract.
  PW_DCHECK(!interrupt::InInterruptContext());

  // Use non-blocking try_acquire for negative and zero length durations.
  if (timeout <= SystemClock::duration::zero()) {
    return try_lock();
  }

#ifndef CONFIG_TIMEOUT_64BIT
  // In case the timeout is too long for us to express through the native
  // Zephyr API, we repeatedly wait with shorter durations. Note that on a
  // tick based kernel we cannot tell how far along we are on the current tick,
  // ergo we add one whole tick to the final duration. However, this also means
  // that the loop must ensure that timeout + 1 is less than the max timeout.
  constexpr SystemClock::duration kLongTimeout =
      chrono::SystemClock::duration(std::numeric_limits<int32_t>::max());
  while (timeout > kLongTimeout) {
    if (k_mutex_lock(&native_handle(), K_TICKS(kLongTimeout.count())) == 0) {
      return true;
    }

    timeout -= kLongTimeout;
  }

  /// Do the final wait. Cast to uint32_t as the underlying duration
  /// representation may be larger, but as per the exit condition, we know that
  /// it must hold that timeout <= INT32_MAX, which in turn must fit in a
  /// uint32_t.
  return k_mutex_lock(&native_handle(),
                      K_TICKS(static_cast<uint32_t>(timeout.count()))) == 0;
#else

  // Note that unlike many other RTOSes, for a duration timeout in ticks, the
  // core kernel wait routine, z_add_timeout, for relative timeouts will always
  // add +1 tick to the duration to ensure proper "wait for at least" behavior
  // while in between a tick. This means that we do not need to add anything
  // here and the kernel will guarantee we wait the proper number of ticks plus
  // some time in the range of [1,2) extra ticks.
  return k_mutex_lock(&native_handle(), K_TICKS(timeout.count())) == 0;

#endif  // CONFIG_TIMEOUT_64BIT
}

#ifdef CONFIG_TIMEOUT_64BIT
bool TimedMutex::try_lock_until(chrono::SystemClock::time_point deadline) {
  PW_DASSERT(!interrupt::InInterruptContext());

  const chrono::SystemClock::time_point now = chrono::SystemClock::now();

  // Check if the expiration deadline has already passed, and attempt to acquire
  // without a timeout.
  if (deadline <= now) {
    return try_lock();
  }

  // With 64-bit timeouts we can wait on a time_point, so do this directly when
  // Zephyr has been configured this way. We will sleep until the time since the
  // epoch start (boot, for the case of a monotonic system clock) we'd like to
  // wait for. Even if enough time has passed such that we're making this call
  // after the wakeup time has passed -- so if we were preempted between the
  // yield and here and we passed the deadline -- we'll then sleep for a single
  // tick
  return k_mutex_lock(
             &native_handle(),
             K_TIMEOUT_ABS_TICKS(deadline.time_since_epoch().count())) == 0;
}
#endif  // CONFIG_TIMEOUT_64BIT

}  // namespace pw::sync
