// Copyright 2024 The Pigweed Authors
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

#include "pw_assert/check.h"
#include "pw_system/config.h"
#include "pw_thread/thread.h"

// For now, pw_system:async only supports FreeRTOS or standard library threads.
//
// This file will be rewritten once the SEED-0128 generic thread creation APIs
// are available. Details of the threads owned by pw_system should be an
// internal implementation detail. If configuration is necessary, it can be
// exposed through regular config options, rather than requiring users to
// implement functions.

#if __has_include("FreeRTOS.h")

#include "FreeRTOS.h"
#include "task.h"

namespace pw::system {

[[noreturn]] void StartSchedulerAndClobberTheStack() {
  // !!!IMPORTANT!!!
  //
  // In at least some ports of FreeRTOS, this call reset the stack pointer to
  // reuses the entirety of the call stack. Any C++ types that were constructed
  // on the stack will not have their destructors called, and the contents will
  // be corrupted as threads and ISRs stomp over whatever was stored there.
  //
  // https://www.freertos.org/Why-FreeRTOS/FAQs/Troubleshooting#main-stack
  //
  // An example of a port that does this is the GCC ARM_CM55_NTZ port, which
  // resets the ARM MSP register in vStartFirstTask(). See
  // portable/GCC/ARM_CM55_NTZ/non_secure/portasm.c
  vTaskStartScheduler();

  // The call may return if here isn't enough heap to actually start the
  // scheduler. Crash since this is a [[noreturn]] function.
  PW_CRASH("vTaskStartScheduler() failed.");
}

}  // namespace pw::system

#else  // STL

#include <chrono>
#include <thread>

#include "pw_thread_stl/options.h"

namespace pw::system {

[[noreturn]] void StartSchedulerAndClobberTheStack() {
  // This generic implementation does NOT actually clobber the stack.
  while (true) {
    std::this_thread::sleep_for(std::chrono::system_clock::duration::max());
  }
}

}  // namespace pw::system

#endif  // __has_include("FreeRTOS.h")
