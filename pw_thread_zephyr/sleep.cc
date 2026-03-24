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
#include "pw_thread/sleep.h"

#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

#include <cstdint>
#include <limits>

#include "pw_assert/check.h"
#include "pw_chrono/system_clock.h"
#include "pw_thread/thread.h"

namespace pw::this_thread {

void sleep_for(chrono::SystemClock::duration sleep_duration) {
  // Ensure this is being called by a thread.
  PW_DCHECK(get_id() != Thread::id());

  // Yield for negative and zero length durations.
  if (sleep_duration <= chrono::SystemClock::duration::zero()) {
    k_yield();
    return;
  }

  // The return value of sleep is the remaining time needed to sleep if woken
  // early. We don't allow or expect k_wakeup use to wakeup the thread
  // prematurely, and thus expect for the returned value to always be 0,
  // indicating a successful sleep.
  int32_t remaining_ms_to_sleep = 0;
#ifdef CONFIG_TIMEOUT_64BIT
  // Note that unlike many other RTOSes, for a duration timeout in ticks, the
  // core kernel wait routine, z_add_timeout, for relative timeouts will always
  // add +1 tick to the duration to ensure proper "wait for at least" behavior
  // while in between a tick. This means that we do not need to add anything
  // here and the kernel will guarantee we wait the proper number of ticks plus
  // some time in the range of [1,2) extra ticks.
  remaining_ms_to_sleep = k_sleep(K_TICKS(sleep_duration.count()));
  // We currently do not handle k_wakeup and instead here check that the sleep
  // duration was what was requested
  PW_CHECK(remaining_ms_to_sleep == 0);
#else
  // We need some value we know won't overflow any internal math in the kernel.
  // We will use half of the uint32_t space as a reasonable midpoint, with
  // enough headroom that is a sufficiently long wait (~2.5 days at 10 KHz).
  // Note that if we were to use the return value of k_sleep, the number of
  // milliseconds remaining that it returns is capped at INT32_MAX, and thus
  // we'd like to be fairly certain that the requested duration of remaining
  // wait can be accurately represented if we were to reasonably return.
  constexpr chrono::SystemClock::duration kLongTimeout =
      chrono::SystemClock::duration(std::numeric_limits<int32_t>::max());
  while (sleep_duration > kLongTimeout) {
    remaining_ms_to_sleep = k_sleep(K_TICKS(kLongTimeout.count()));
    // We currently do not handle k_wakeup and instead here check that the sleep
    // duration was what was requested
    PW_CHECK(remaining_ms_to_sleep == 0);
    sleep_duration -= kLongTimeout;
  }
  // We do not add a +1 offset here. See the comment above for the 64-bit wait.
  // We do need to cast to a 32-bit value to properly downsize the duration if
  // it uses a 64-bit representation. We know the represented value must fit in
  // an unsigned 32-bit number, as we tested for negativity at the start of this
  // function, and the exit condition of the above loop means we must be <= to
  // INT32_MAX which must be representable in a uint32_t.
  remaining_ms_to_sleep =
      k_sleep(K_TICKS(static_cast<uint32_t>(sleep_duration.count())));
  // We currently do not handle k_wakeup and instead here check that the sleep
  // duration was what was requested
  PW_CHECK(remaining_ms_to_sleep == 0);
#endif
}

#ifdef CONFIG_TIMEOUT_64BIT
void sleep_until(chrono::SystemClock::time_point wakeup_time) {
  // Ensure this is being called by a thread.
  PW_DCHECK(get_id() != Thread::id());

  // Check if the expiration deadline has already passed, yield.
  if (wakeup_time <= chrono::SystemClock::now()) {
    k_yield();
    return;
  }

  // With 64-bit timeouts we can wait on a time_point, so do this directly when
  // Zephyr has been configured this way. We will sleep until the time since the
  // epoch start (boot, for the case of a monotonic system clock) we'd like to
  // wait for. Even if enough time has passed such that we're making this call
  // after the wakeup time has passed -- so if we were preempted between the
  // yield and here and we passed the deadline -- we'll then sleep for a single
  // tick
  const int32_t remaining_ms_to_sleep =
      k_sleep(K_TIMEOUT_ABS_TICKS(wakeup_time.time_since_epoch().count()));
  PW_CHECK(remaining_ms_to_sleep == 0);
}
#endif  // CONFIG_TIMEOUT_64BIT

}  // namespace pw::this_thread
