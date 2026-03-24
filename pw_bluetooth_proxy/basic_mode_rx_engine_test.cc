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

#include "pw_bluetooth_proxy/internal/basic_mode_rx_engine.h"

#include "pw_unit_test/framework.h"

namespace pw::bluetooth::proxy::internal {

namespace {

constexpr uint16_t kLocalCid = 0x50;

TEST(BasicModeRxEngineTest, InvalidBFrame) {
  BasicModeRxEngine engine(kLocalCid);
  std::array<uint8_t, 1> frame = {};
  RxEngine::HandlePduFromControllerReturnValue result =
      engine.HandlePduFromController(frame);
  EXPECT_TRUE(std::holds_alternative<std::monostate>(result));
}

TEST(BasicModeRxEngineTest, UseSpans) {
  BasicModeRxEngine engine(kLocalCid);
  std::array<uint8_t, 7> frame = {// L2cap B-Frame:
                                  0x03,
                                  0x00,  // PDU length
                                  0x50,
                                  0x00,  // Local Channel ID
                                         // Payload:
                                  0x03,
                                  0x04,
                                  0x05};
  const std::array<uint8_t, 3> kExpectedPayload = {0x03, 0x04, 0x05};
  RxEngine::HandlePduFromControllerReturnValue result =
      engine.HandlePduFromController(frame);
  ASSERT_TRUE(std::holds_alternative<pw::span<uint8_t>>(result));
  pw::span<uint8_t> actual_payload = std::get<pw::span<uint8_t>>(result);
  ASSERT_EQ(actual_payload.size(), kExpectedPayload.size());
  EXPECT_TRUE(std::equal(actual_payload.begin(),
                         actual_payload.end(),
                         kExpectedPayload.begin(),
                         kExpectedPayload.end()));
}

}  // namespace

}  // namespace pw::bluetooth::proxy::internal
