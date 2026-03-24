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

#include "pw_bluetooth_proxy/h4_packet.h"
#include "pw_multibuf/multibuf.h"
#include "pw_result/result.h"
#include "pw_span/span.h"
#include "pw_status/status.h"

namespace pw::bluetooth::proxy::internal {

class TxEngine {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual std::optional<uint16_t> MaxL2capPayloadSize() = 0;
    virtual pw::Result<H4PacketWithH4> AllocateH4(uint16_t length) = 0;
  };

  struct HandlePduFromHostReturnValue {
    bool forward_to_controller;
    std::optional<pw::span<uint8_t>> send_to_client = std::nullopt;
  };

  TxEngine() = default;
  TxEngine(TxEngine&&) = default;
  TxEngine& operator=(TxEngine&&) = default;

  virtual ~TxEngine() = default;

  // Returns UNAVAILABLE if there is nothing to send.
  virtual Result<H4PacketWithH4> GenerateNextPacket(
      const multibuf::MultiBuf& payload, bool& keep_payload) = 0;

  // Returns FAILED_PRECONDITION if the maximum size is not yet known.
  // Returns INVALID_ARGUMENT if the payload is too large.
  virtual Status CheckWriteParameter(const multibuf::MultiBuf& payload) = 0;

  // Add send credits. No-op if the TxEngine is creditless.
  // Returns true if the previous number of credits was 0 and the channel needs
  // to be drained.
  // Returns INVALID_ARGUMENT if the number of credits is invalid.
  virtual Result<bool> AddCredits(uint16_t credits) = 0;

  virtual HandlePduFromHostReturnValue HandlePduFromHost(
      pw::span<uint8_t> frame) = 0;
};

}  // namespace pw::bluetooth::proxy::internal
