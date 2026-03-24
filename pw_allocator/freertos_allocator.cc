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

#include "pw_allocator/freertos_allocator.h"

#include "FreeRTOS.h"

namespace pw::allocator {

void* FreeRtosAllocator::DoAllocate(Layout layout) {
  // TODO: b/301930507 - Add support for larger alignments.
  if (layout.alignment() > portBYTE_ALIGNMENT) {
    return nullptr;
  }
  return pvPortMalloc(layout.size());
}

void FreeRtosAllocator::DoDeallocate(void* ptr) { vPortFree(ptr); }

FreeRtosAllocator& GetFreeRtosAllocator() {
  static FreeRtosAllocator instance;
  return instance;
}

}  // namespace pw::allocator
