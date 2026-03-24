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

#include "pw_bluetooth_proxy/internal/channel_proxy_impl.h"

#include "pw_log/log.h"

namespace pw::bluetooth::proxy::internal {

ChannelProxyImpl::ChannelProxyImpl(uint16_t max_l2cap_payload_size,
                                   L2capChannel& channel)
    : GenericL2capChannel(channel),
      max_l2cap_payload_size_(max_l2cap_payload_size) {}

StatusWithMultiBuf ChannelProxyImpl::DoWrite(multibuf::MultiBuf&& payload) {
  return GenericL2capChannel::Write(std::move(payload));
}

Status ChannelProxyImpl::DoIsWriteAvailable() {
  return GenericL2capChannel::IsWriteAvailable();
}

Status ChannelProxyImpl::DoSendAdditionalRxCredits(
    uint16_t additional_rx_credits) {
  return GenericL2capChannel::SendAdditionalRxCredits(additional_rx_credits);
}

Status ChannelProxyImpl::DoCheckWriteParameter(
    const multibuf::MultiBuf& payload) {
  if (max_l2cap_payload_size_ == 0) {
    return Status::FailedPrecondition();
  }
  if (payload.size() > max_l2cap_payload_size_) {
    PW_LOG_WARN("Payload (%zu bytes) is too large. So will not process.",
                payload.size());
    return Status::InvalidArgument();
  }
  return OkStatus();
}

}  // namespace pw::bluetooth::proxy::internal
