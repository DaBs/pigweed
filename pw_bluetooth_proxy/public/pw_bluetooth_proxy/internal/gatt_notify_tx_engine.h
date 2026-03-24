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

#include "pw_bluetooth_proxy/internal/tx_engine.h"

namespace pw::bluetooth::proxy::internal {

/// A TxEngine that sends ATT notification packets in a B-Frame.
/// This class is not thread-safe and external synchronization is required.
class GattNotifyTxEngine final : public TxEngine {
 public:
  GattNotifyTxEngine(uint16_t connection_handle,
                     uint16_t remote_cid,
                     uint16_t attribute_handle,
                     Delegate& delegate)
      : delegate_(delegate),
        connection_handle_(connection_handle),
        remote_cid_(remote_cid),
        attribute_handle_(attribute_handle) {}

  ~GattNotifyTxEngine() override = default;

  // TxEngine overrides:

  Result<H4PacketWithH4> GenerateNextPacket(
      const multibuf::MultiBuf& attribute_value, bool& keep_payload) override;
  Status CheckWriteParameter(const multibuf::MultiBuf& payload) override;
  Result<bool> AddCredits(uint16_t) override {
    // ATT does not use credits.
    return false;
  }
  HandlePduFromHostReturnValue HandlePduFromHost(pw::span<uint8_t>) override {
    // Forward all packets to controller.
    return {.forward_to_controller = true, .send_to_client = std::nullopt};
  }

 private:
  Delegate& delegate_;
  const uint16_t connection_handle_;
  const uint16_t remote_cid_;
  const uint16_t attribute_handle_;
};
}  // namespace pw::bluetooth::proxy::internal
