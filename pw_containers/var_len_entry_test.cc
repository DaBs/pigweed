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

#include "pw_containers/internal/var_len_entry.h"

#include <array>
#include <cstddef>
#include <string_view>

#include "pw_bytes/array.h"
#include "pw_containers/var_len_entry_queue.h"
#include "pw_unit_test/constexpr.h"
#include "pw_unit_test/framework.h"

namespace {

using pw::containers::internal::VarLenEntry;
using namespace std::literals::string_view_literals;

PW_CONSTEXPR_TEST(VarLenEntry, DefaultConstructed, {
  VarLenEntry<std::byte> entry;
  PW_TEST_EXPECT_TRUE(entry.empty());
  PW_TEST_EXPECT_EQ(entry.size(), 0u);
  PW_TEST_EXPECT_EQ(entry.begin(), entry.end());

  VarLenEntry<const std::byte> const_entry;
  PW_TEST_EXPECT_TRUE(const_entry.empty());
  PW_TEST_EXPECT_EQ(const_entry.size(), 0u);
  PW_TEST_EXPECT_EQ(const_entry.begin(), const_entry.end());
});

PW_CONSTEXPR_TEST_IF_CLANG(VarLenEntry, Contiguous, {
  std::byte buffer[10] = {};
  pw::VarLenEntryQueue queue(buffer);
  auto data = pw::bytes::String("ABCDE");
  queue.push(data);

  VarLenEntry<const std::byte> entry = queue.front();

  PW_TEST_EXPECT_FALSE(entry.empty());
  PW_TEST_EXPECT_EQ(entry.size(), sizeof(data));

  PW_TEST_EXPECT_EQ(entry.front(), data[0]);
  PW_TEST_EXPECT_EQ(entry.back(), data[sizeof(data) - 1]);
  for (size_t i = 0; i < sizeof(data); ++i) {
    PW_TEST_EXPECT_EQ(entry[i], data[i]);
    PW_TEST_EXPECT_EQ(entry.at(i), data[i]);
  }

  auto [span1, span2] = entry.contiguous_data();
  PW_TEST_EXPECT_EQ(span1.size(), sizeof(data));
  PW_TEST_EXPECT_TRUE(span2.empty());
  for (size_t i = 0; i < sizeof(data); ++i) {
    PW_TEST_EXPECT_EQ(span1[i], data[i]);
  }

  std::byte copy_buffer[sizeof(data)] = {};
  PW_TEST_EXPECT_EQ(entry.copy(copy_buffer, sizeof(copy_buffer)), sizeof(data));
  for (size_t i = 0; i < sizeof(copy_buffer); ++i) {
    PW_TEST_EXPECT_EQ(copy_buffer[i], data[i]);
  }

  std::byte small_copy_buffer[sizeof(data) - 2] = {};
  PW_TEST_EXPECT_EQ(entry.copy(small_copy_buffer, sizeof(small_copy_buffer)),
                    sizeof(data));
  for (size_t i = 0; i < sizeof(small_copy_buffer); ++i) {
    PW_TEST_EXPECT_EQ(small_copy_buffer[i], data[i]);
  }
});

PW_CONSTEXPR_TEST_IF_CLANG(VarLenEntry, Discontiguous, {
  std::byte buffer[8] = {};
  pw::VarLenEntryQueue queue(buffer);

  auto overwritten = pw::bytes::String("12");
  queue.push(overwritten);

  auto data = pw::bytes::String("ABCDE");
  queue.push_overwrite(data);

  VarLenEntry<const std::byte> entry = queue.front();

  PW_TEST_EXPECT_FALSE(entry.empty());
  PW_TEST_EXPECT_EQ(entry.size(), sizeof(data));

  PW_TEST_EXPECT_EQ(entry.front(), data[0]);
  PW_TEST_EXPECT_EQ(entry.back(), data[sizeof(data) - 1]);
  for (size_t i = 0; i < sizeof(data); ++i) {
    PW_TEST_EXPECT_EQ(entry[i], data[i]);
    PW_TEST_EXPECT_EQ(entry.at(i), data[i]);
  }

  auto [span1, span2] = entry.contiguous_data();
  PW_TEST_EXPECT_FALSE(span1.empty());
  PW_TEST_EXPECT_FALSE(span2.empty());
  PW_TEST_EXPECT_EQ(span1.size() + span2.size(), sizeof(data));

  std::byte copy_buffer[sizeof(data)] = {};
  PW_TEST_EXPECT_EQ(entry.copy(copy_buffer, sizeof(copy_buffer)), sizeof(data));
  for (size_t i = 0; i < sizeof(copy_buffer); ++i) {
    PW_TEST_EXPECT_EQ(copy_buffer[i], data[i]);
  }

  std::byte small_copy_buffer[sizeof(data) - 2] = {};
  PW_TEST_EXPECT_EQ(entry.copy(small_copy_buffer, sizeof(small_copy_buffer)),
                    sizeof(data));
  for (size_t i = 0; i < sizeof(small_copy_buffer); ++i) {
    PW_TEST_EXPECT_EQ(small_copy_buffer[i], data[i]);
  }
});

PW_CONSTEXPR_TEST_IF_CLANG(VarLenEntry, ConstConversion, {
  std::byte buffer[10] = {};
  pw::VarLenEntryQueue queue(buffer);

  auto data = pw::bytes::String("ABC");
  queue.push(data);

  VarLenEntry<std::byte> entry = queue.front();
  VarLenEntry<const std::byte> const_entry = entry;

  PW_TEST_EXPECT_EQ(const_entry.size(), sizeof(data));
  for (size_t i = 0; i < sizeof(data); ++i) {
    PW_TEST_EXPECT_EQ(const_entry[i], data[i]);
    PW_TEST_EXPECT_EQ(const_entry.at(i), data[i]);
  }
});

PW_CONSTEXPR_TEST_IF_CLANG(VarLenEntry, Equality, {
  std::byte buffer[10] = {};
  pw::VarLenEntryQueue queue(buffer);

  auto data = pw::bytes::String("ABC");
  queue.push(data);
  queue.push(data);

  VarLenEntry<const std::byte> entry1 = queue.front();
  queue.pop();
  VarLenEntry<const std::byte> entry2 = queue.front();

  // These entries have the same content but are at different locations.
  PW_TEST_EXPECT_NE(entry1, entry2);

  // Create a copy of the iterator to get an equal entry.
  auto it = queue.begin();
  VarLenEntry<const std::byte> entry3 = *it;
  PW_TEST_EXPECT_EQ(entry2, entry3);
});

PW_CONSTEXPR_TEST_IF_CLANG(VarLenEntry, Mutable, {
  std::byte buffer[8] = {};
  pw::VarLenEntryQueue queue(buffer);

  auto data = pw::bytes::String("ABCDE");
  queue.push(data);

  VarLenEntry<std::byte> entry = queue.front();
  entry[0] = std::byte{'Z'};
  entry.at(1) = std::byte{'Y'};
  entry.back() = std::byte{'X'};

  std::byte copy_buffer[sizeof(data)] = {};
  entry.copy(copy_buffer, sizeof(copy_buffer));
  PW_TEST_EXPECT_EQ(copy_buffer[0], std::byte('Z'));
  PW_TEST_EXPECT_EQ(copy_buffer[1], std::byte('Y'));
  PW_TEST_EXPECT_EQ(copy_buffer[2], std::byte('C'));
  PW_TEST_EXPECT_EQ(copy_buffer[3], std::byte('D'));
  PW_TEST_EXPECT_EQ(copy_buffer[4], std::byte('X'));
});

}  // namespace
