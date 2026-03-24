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

/// TxEngine that implements L2CAP Basic mode (B-Frame headers).
/// This class is not thread-safe and requires external synchronization.
class BasicModeTxEngine final : public TxEngine {
 public:
  BasicModeTxEngine(uint16_t connection_handle,
                    uint16_t remote_cid,
                    Delegate& delegate)
      : delegate_(delegate),
        connection_handle_(connection_handle),
        remote_cid_(remote_cid) {}

  BasicModeTxEngine(BasicModeTxEngine&& other) = delete;
  BasicModeTxEngine& operator=(BasicModeTxEngine&& other) = delete;

  ~BasicModeTxEngine() override = default;

  // TxEngine overrides:

  Result<H4PacketWithH4> GenerateNextPacket(const multibuf::MultiBuf& payload,
                                            bool& keep_payload) override;
  Status CheckWriteParameter(const multibuf::MultiBuf& payload) override;
  Result<bool> AddCredits(uint16_t) override {
    // Basic mode does not use credits.
    return false;
  }
  HandlePduFromHostReturnValue HandlePduFromHost(
      pw::span<uint8_t> frame) override;

 private:
  Delegate& delegate_;
  const uint16_t connection_handle_;
  const uint16_t remote_cid_;
};
}  // namespace pw::bluetooth::proxy::internal
