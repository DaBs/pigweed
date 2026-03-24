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

#include "pw_bluetooth_proxy/internal/rx_engine.h"

namespace pw::bluetooth::proxy::internal {

/// An RxEngine for a GattNotifyChannel that forwards all ATT packets to the
/// host.
/// This class is stateless and therefore thread-safe.
class GattNotifyRxEngine final : public RxEngine {
 public:
  GattNotifyRxEngine() = default;

  GattNotifyRxEngine(GattNotifyRxEngine&& other) = default;
  GattNotifyRxEngine& operator=(GattNotifyRxEngine&& other) = default;

  HandlePduFromControllerReturnValue HandlePduFromController(
      pw::span<uint8_t>) override {
    // Forward all packets to host.
    // No GATT Notify client has an RX callback, we we can just return an empty
    // span to forward the packet.
    return pw::span<uint8_t>();
  }

  Status AddRxCredits(uint16_t) override { return Status::Unimplemented(); }
};

}  // namespace pw::bluetooth::proxy::internal
