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

#include "pw_containers/var_len_entry_queue.h"

#include <array>
#include <cstddef>

#include "pw_assert/check.h"
#include "pw_containers/internal/generic_var_len_entry_queue.h"
#include "pw_containers/test/generic_var_len_entry_queue_testing.h"
#include "pw_span/span.h"

namespace {

// Include tests of GenericVarLenEntryQueue, using a BasicInlineVarLenEntryQueue
// factory.
class BasicVarLenEntryQueueTest
    : public pw::containers::test::GenericVarLenEntryQueueTest<
          BasicVarLenEntryQueueTest> {
 public:
  constexpr static size_t kCapacity = 0x100;

  template <typename T, size_t kMaxSizeBytes>
  pw::BasicVarLenEntryQueue<T> MakeQueue() {
    size_t capacity =
        pw::BasicVarLenEntryQueue<T>::DataSizeBytes(kMaxSizeBytes);
    PW_CHECK_UINT_LE(capacity, kCapacity - offset_);
    T* ptr = reinterpret_cast<T*>(buffer_.data() + offset_);
    offset_ += capacity;
    return pw::BasicVarLenEntryQueue<T>(pw::span<T>(ptr, capacity));
  }

 private:
  std::array<std::byte, kCapacity> buffer_;
  size_t offset_ = 0;
};
PW_GENERIC_VAR_LEN_ENTRY_QUEUE_TESTS(BasicVarLenEntryQueueTest);

}  // namespace
