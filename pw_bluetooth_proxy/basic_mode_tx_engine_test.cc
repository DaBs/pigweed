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

#include "pw_bluetooth_proxy/internal/basic_mode_tx_engine.h"

#include "pw_allocator/testing.h"
#include "pw_assert/check.h"
#include "pw_bluetooth_proxy_private/test_utils.h"
#include "pw_containers/inline_queue.h"
#include "pw_unit_test/framework.h"

namespace pw::bluetooth::proxy::internal {

namespace {

constexpr uint16_t kConnectionHandle = 0x40;
constexpr uint16_t kRemoteCid = 0x50;
constexpr std::array<std::byte, 3> kPayload0 = {
    std::byte{0x00}, std::byte{0x01}, std::byte{0x02}};
constexpr std::array<std::byte, 3> kPayload1 = {
    std::byte{0x03}, std::byte{0x04}, std::byte{0x05}};

class BasicModeTxEngineTest : public ::testing::Test,
                              public TxEngine::Delegate {
 public:
  void SetUp() override {
    // Queue 2 packets
    std::optional<multibuf::MultiBuf> buffer_0 =
        multibuf_allocator_.AllocateContiguous(kPayload0.size());
    PW_CHECK(buffer_0->CopyFrom(kPayload0).ok());
    payload_queue_.emplace(std::move(*buffer_0));

    std::optional<multibuf::MultiBuf> buffer_1 =
        multibuf_allocator_.AllocateContiguous(kPayload1.size());
    PW_CHECK(buffer_1->CopyFrom(kPayload1).ok());
    payload_queue_.emplace(std::move(*buffer_1));

    max_payload_size_ = 20;
    engine_.emplace(kConnectionHandle, kRemoteCid, *this);
  }

  void TearDown() override { engine_.reset(); }

  BasicModeTxEngine& engine() { return engine_.value(); }

  multibuf::MultiBufAllocator& multibuf_allocator() {
    return multibuf_allocator_;
  }

  void set_max_payload_size(std::optional<uint16_t> max_payload_size) {
    max_payload_size_ = max_payload_size;
  }

  // TxEngine::Delegate overrides:

  std::optional<uint16_t> MaxL2capPayloadSize() override {
    return max_payload_size_;
  }

  const multibuf::MultiBuf& FrontPayload() {
    PW_CHECK(!payload_queue_.empty());
    return payload_queue_.front();
  }

  void PopFrontPayload() {
    ASSERT_FALSE(payload_queue_.empty());
    payload_queue_.pop();
  }

  pw::Result<H4PacketWithH4> AllocateH4(uint16_t length) override {
    void* allocation =
        allocator_.Allocate(allocator::Layout(length, alignof(uint8_t)));
    PW_ASSERT(allocation);
    return H4PacketWithH4(
        pw::span<uint8_t>(static_cast<uint8_t*>(allocation), length),
        [this](const uint8_t* buffer) {
          allocator_.Deallocate(const_cast<uint8_t*>(buffer));
        });
  }

 private:
  pw::allocator::test::AllocatorForTest<500> allocator_;

  // Use libc allocator so msan can detect use after frees.
  std::array<std::byte, 500> packet_buffer_{};
  pw::multibuf::SimpleAllocator multibuf_allocator_{
      /*data_area=*/packet_buffer_,
      /*metadata_alloc=*/allocator::GetLibCAllocator()};

  std::optional<BasicModeTxEngine> engine_;
  std::optional<uint16_t> max_payload_size_;
  InlineQueue<multibuf::MultiBuf, 2> payload_queue_;
};

TEST_F(BasicModeTxEngineTest, GenerateNextPacket) {
  bool keep_payload = false;
  Result<H4PacketWithH4> packet_0 =
      engine().GenerateNextPacket(FrontPayload(), keep_payload);
  PW_TEST_ASSERT_OK(packet_0);
  EXPECT_FALSE(keep_payload);
  PopFrontPayload();
  pw::span<uint8_t> packet_0_span = packet_0->GetH4Span();
  EXPECT_EQ(packet_0_span.size(), 12u);
  const std::array<uint8_t, 12> kExpectedH4_0 = {0x02,  // H4 type: ACL
                                                        // ACL header:
                                                 0x40,
                                                 0x00,  // Handle
                                                 0x07,
                                                 0x00,  // Data Total Length
                                                        // L2cap B-Frame:
                                                 0x03,
                                                 0x00,  // PDU length
                                                 0x50,
                                                 0x00,  // Remote Channel ID
                                                        // Payload:
                                                 0x00,
                                                 0x01,
                                                 0x02};
  EXPECT_TRUE(std::equal(packet_0_span.begin(),
                         packet_0_span.end(),
                         kExpectedH4_0.begin(),
                         kExpectedH4_0.end()));

  Result<H4PacketWithH4> packet_1 =
      engine().GenerateNextPacket(FrontPayload(), keep_payload);
  PW_TEST_ASSERT_OK(packet_1);
  EXPECT_FALSE(keep_payload);
  PopFrontPayload();
  pw::span<uint8_t> packet_1_span = packet_1->GetH4Span();
  EXPECT_EQ(packet_1_span.size(), 12u);
  const std::array<uint8_t, 12> kExpectedH4_1 = {0x02,  // H4 type: ACL
                                                        // ACL header:
                                                 0x40,
                                                 0x00,  // Handle
                                                 0x07,
                                                 0x00,  // Data Total Length
                                                        // L2cap B-Frame:
                                                 0x03,
                                                 0x00,  // PDU length
                                                 0x50,
                                                 0x00,  // Remote Channel ID
                                                        // Payload:
                                                 0x03,
                                                 0x04,
                                                 0x05};
  EXPECT_TRUE(std::equal(packet_1_span.begin(),
                         packet_1_span.end(),
                         kExpectedH4_1.begin(),
                         kExpectedH4_1.end()));
}

TEST_F(BasicModeTxEngineTest, AddCredits) {
  Result<bool> result = engine().AddCredits(1);
  PW_TEST_ASSERT_OK(result);
  EXPECT_FALSE(result.value());
}

TEST_F(BasicModeTxEngineTest, CheckWriteParameterNoSizeYet) {
  std::optional<multibuf::MultiBuf> buffer =
      multibuf_allocator().AllocateContiguous(1);
  ASSERT_TRUE(buffer.has_value());

  set_max_payload_size(std::nullopt);
  Status status = engine().CheckWriteParameter(*buffer);
  EXPECT_EQ(status, Status::FailedPrecondition());
}

TEST_F(BasicModeTxEngineTest, CheckWriteParameterPayloadTooLarge) {
  std::optional<multibuf::MultiBuf> buffer =
      multibuf_allocator().AllocateContiguous(20);
  ASSERT_TRUE(buffer.has_value());

  set_max_payload_size(19);
  Status status = engine().CheckWriteParameter(*buffer);
  EXPECT_EQ(status, Status::InvalidArgument());
}

TEST_F(BasicModeTxEngineTest, CheckWriteParameterOk) {
  std::optional<multibuf::MultiBuf> buffer =
      multibuf_allocator().AllocateContiguous(20);
  ASSERT_TRUE(buffer.has_value());

  set_max_payload_size(20);
  Status status = engine().CheckWriteParameter(*buffer);
  PW_TEST_ASSERT_OK(status);
}

TEST_F(BasicModeTxEngineTest, HandlePduFromHostInvalidFrame) {
  std::array<uint8_t, 1> frame = {};
  TxEngine::HandlePduFromHostReturnValue result =
      engine().HandlePduFromHost(frame);
  EXPECT_FALSE(result.forward_to_controller);
  EXPECT_FALSE(result.send_to_client.has_value());
}

TEST_F(BasicModeTxEngineTest, HandlePduFromHostValidFrame) {
  std::array<uint8_t, 7> frame = {// L2cap B-Frame:
                                  0x03,
                                  0x00,  // PDU length
                                  0x50,
                                  0x00,  // Remote Channel ID
                                         // Payload:
                                  0x03,
                                  0x04,
                                  0x05};
  std::array<uint8_t, 3> payload = {0x03, 0x04, 0x05};
  TxEngine::HandlePduFromHostReturnValue result =
      engine().HandlePduFromHost(frame);
  EXPECT_TRUE(result.forward_to_controller);
  ASSERT_TRUE(result.send_to_client.has_value());
  ASSERT_EQ(result.send_to_client->size(), 3u);
  EXPECT_TRUE(std::equal(result.send_to_client->begin(),
                         result.send_to_client->end(),
                         payload.begin(),
                         payload.end()));
}

}  // namespace

}  // namespace pw::bluetooth::proxy::internal
