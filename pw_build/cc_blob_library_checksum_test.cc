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

#include <cstddef>
#include <cstdint>

#include "pw_build/test_blob_with_checksum.h"
#include "pw_unit_test/framework.h"

namespace {

TEST(CcBlobLibraryChecksumTest, Crc32ChecksumGenerated) {
  // Data in test_blob_0123.bin is {0x00, 0x01, 0x02, 0x03}
  // CRC32 of {0x00, 0x01, 0x02, 0x03} is 0x8bb98613
  constexpr uint32_t kExpectedCrc32 = 0x8bb98613;
  EXPECT_EQ(test::checksum_ns::kBlobWithChecksum_crc32, kExpectedCrc32);
}

}  // namespace
