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

#include "pw_bluetooth_proxy/channel_proxy.h"
#include "pw_bluetooth_proxy/internal/generic_l2cap_channel.h"

namespace pw::bluetooth::proxy::internal {
class ChannelProxyImpl final : public ChannelProxy,
                               public internal::GenericL2capChannel {
 public:
  ChannelProxyImpl(uint16_t max_l2cap_payload_size, L2capChannel& channel);

  ChannelProxyImpl(ChannelProxyImpl&& other) = default;
  ChannelProxyImpl& operator=(ChannelProxyImpl&& other) = default;

 private:
  // ChannelProxy overrides:
  StatusWithMultiBuf DoWrite(multibuf::MultiBuf&& payload) override;
  Status DoIsWriteAvailable() override;
  Status DoSendAdditionalRxCredits(uint16_t additional_rx_credits) override;

  // GenericL2capChannel overrides:
  Status DoCheckWriteParameter(const multibuf::MultiBuf& payload) override;

  uint16_t max_l2cap_payload_size_ = 0;
};

}  // namespace pw::bluetooth::proxy::internal
