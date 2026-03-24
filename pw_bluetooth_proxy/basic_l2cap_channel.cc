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

#include "pw_bluetooth_proxy/basic_l2cap_channel.h"

#include "pw_log/log.h"
#include "pw_status/status.h"

namespace pw::bluetooth::proxy {

BasicL2capChannel::BasicL2capChannel(L2capChannel& channel)
    : internal::GenericL2capChannel(channel) {
  std::optional<uint16_t> max_l2cap_payload_size =
      channel.MaxL2capPayloadSize();
  if (!max_l2cap_payload_size.has_value()) {
    PW_LOG_ERROR("Maximum L2CAP payload size is not set.");
  } else {
    max_l2cap_payload_size_ = *max_l2cap_payload_size;
  }
}

Status BasicL2capChannel::DoCheckWriteParameter(
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

}  // namespace pw::bluetooth::proxy
