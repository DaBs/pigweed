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

#include "pw_allocator/allocator.h"
#include "pw_allocator/capability.h"

namespace pw::allocator {

/// @submodule{pw_allocator,concrete}

/// Memory allocator that uses FreeRTOS memory management.
class FreeRtosAllocator final : public Allocator {
 public:
  friend FreeRtosAllocator& GetFreeRtosAllocator();

 private:
  static constexpr Capabilities kCapabilities = 0;
  constexpr FreeRtosAllocator() : Allocator(kCapabilities) {}

  /// @copydoc Allocator::Allocate
  void* DoAllocate(Layout layout) override;

  /// @copydoc Allocator::Deallocate
  void DoDeallocate(void* ptr) override;
};

/// Returns a reference to the FreeRtosAllocator singleton.
FreeRtosAllocator& GetFreeRtosAllocator();

/// @}

}  // namespace pw::allocator
