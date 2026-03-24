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

#include "pw_transfer/handler_allocator.h"

#include <array>
#include <cstddef>

#include "pw_allocator/best_fit.h"
#include "pw_allocator/block/small_block.h"
#include "pw_stream/null_stream.h"
#include "pw_thread/test_thread_context.h"
#include "pw_thread/thread.h"
#include "pw_transfer/transfer.h"
#include "pw_unit_test/framework.h"

namespace pw::transfer {
namespace {

class TransferHandlerAllocatorTestBase : public ::testing::Test {
 protected:
  TransferHandlerAllocatorTestBase()
      : thread_(chunk_buffer_, encode_buffer_),
        system_thread_(system_thread_context_.options(), thread_),
        transfer_service_(thread_, 1024U),
        allocator_(memory_pool_) {}

  static constexpr size_t kMaxHandlers = 4;

  ~TransferHandlerAllocatorTestBase() override {
    thread_.Terminate();
    system_thread_.join();
  }

  std::array<std::byte, 1> chunk_buffer_;
  std::array<std::byte, 1> encode_buffer_;
  transfer::Thread<1, 1> thread_;
  pw::thread::test::TestThreadContext system_thread_context_;
  pw::Thread system_thread_;
  TransferService transfer_service_;
  std::array<std::byte,
             TransferHandlerAllocator::GetAllocatorSize(
                 kMaxHandlers, allocator::SmallBlock::kBlockOverhead)>
      memory_pool_;
  allocator::BestFitAllocator<allocator::SmallBlock> allocator_;
};

class TransferHandlerAllocatorTest : public TransferHandlerAllocatorTestBase {
 protected:
  TransferHandlerAllocatorTest()
      : handler_allocator_(transfer_service_, allocator_) {}

  TransferHandlerAllocator handler_allocator_;
};

TEST_F(TransferHandlerAllocatorTest, AllocateReader) {
  stream::NullStream reader;
  auto resource_id = handler_allocator_.AllocateReader(reader);
  PW_TEST_EXPECT_OK(resource_id);
}

TEST_F(TransferHandlerAllocatorTest, AllocateWriter) {
  stream::NullStream writer;
  auto resource_id = handler_allocator_.AllocateWriter(writer);
  PW_TEST_EXPECT_OK(resource_id);
}

TEST_F(TransferHandlerAllocatorTest, AllocateReadWriter) {
  stream::NullStream reader_writer;
  auto resource_id = handler_allocator_.AllocateReadWriter(reader_writer);
  PW_TEST_EXPECT_OK(resource_id);
}

TEST_F(TransferHandlerAllocatorTest, CloseInvalidId) {
  TransferResource invalid(handler_allocator_, 123);
}

TEST_F(TransferHandlerAllocatorTest, MultipleAllocationsAndCloses) {
  stream::NullStream reader1;
  stream::NullStream writer1;
  stream::NullStream reader_writer1;
  stream::NullStream reader2;

  auto id1 = handler_allocator_.AllocateReader(reader1);
  auto id2 = handler_allocator_.AllocateWriter(writer1);
  auto id3 = handler_allocator_.AllocateReadWriter(reader_writer1);
  auto id4 = handler_allocator_.AllocateReader(reader2);

  PW_TEST_ASSERT_OK(id1);
  PW_TEST_ASSERT_OK(id2);
  PW_TEST_ASSERT_OK(id3);
  PW_TEST_ASSERT_OK(id4);

  // Past kMaxHandlers should fail
  EXPECT_EQ(handler_allocator_.AllocateReader(reader1).status(),
            Status::ResourceExhausted());

  *id1 = {};

  // After clearing one, we can allocate one again.
  PW_TEST_ASSERT_OK(handler_allocator_.AllocateReader(reader1));
}

using RangedTransferHandlerAllocatorTest = TransferHandlerAllocatorTestBase;

TEST_F(RangedTransferHandlerAllocatorTest, AllocateInRange) {
  TransferHandlerAllocator handler_allocator(
      transfer_service_, allocator_, 10, 12);
  stream::NullStream reader;
  auto resource1 = handler_allocator.AllocateReader(reader);
  PW_TEST_ASSERT_OK(resource1);
  EXPECT_EQ(resource1->resource_id(), 10U);
  auto resource2 = handler_allocator.AllocateReader(reader);
  PW_TEST_ASSERT_OK(resource2);
  EXPECT_EQ(resource2->resource_id(), 11U);
  auto resource3 = handler_allocator.AllocateReader(reader);
  PW_TEST_ASSERT_OK(resource3);
  EXPECT_EQ(resource3->resource_id(), 12U);
  EXPECT_EQ(handler_allocator.AllocateReader(reader).status(),
            Status::ResourceExhausted());
}

TEST_F(RangedTransferHandlerAllocatorTest, ReuseIds) {
  TransferHandlerAllocator handler_allocator(
      transfer_service_, allocator_, 10, 11);
  stream::NullStream reader;
  auto resource1 = handler_allocator.AllocateReader(reader);
  PW_TEST_ASSERT_OK(resource1);
  EXPECT_EQ(resource1->resource_id(), 10U);

  auto resource2 = handler_allocator.AllocateReader(reader);
  PW_TEST_ASSERT_OK(resource2);
  EXPECT_EQ(resource2->resource_id(), 11U);
  EXPECT_EQ(handler_allocator.AllocateReader(reader).status(),
            Status::ResourceExhausted());

  resource1->Close();
  auto resource3 = handler_allocator.AllocateReader(reader);
  PW_TEST_ASSERT_OK(resource3);
  EXPECT_EQ(resource3->resource_id(), 10U);
}

}  // namespace
}  // namespace pw::transfer
