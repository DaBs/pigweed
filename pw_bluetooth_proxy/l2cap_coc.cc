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

#include "pw_bluetooth_proxy/l2cap_coc.h"

#include <mutex>

#include "pw_bluetooth/l2cap_frames.emb.h"
#include "pw_log/log.h"

namespace pw::bluetooth::proxy {

constexpr uint8_t kSduLengthFieldSize =
    emboss::FirstKFrame::MinSizeInBytes() -
    emboss::BasicL2capHeader::IntrinsicSizeInBytes();

L2capCoc::L2capCoc(L2capChannel& channel, uint16_t tx_mtu)
    : internal::GenericL2capChannel(channel), tx_mtu_(tx_mtu) {
  std::optional<uint16_t> max_l2cap_payload_size =
      channel.MaxL2capPayloadSize();
  if (!max_l2cap_payload_size.has_value()) {
    PW_LOG_ERROR("Maximum L2CAP payload size is not set.");
  } else if (*max_l2cap_payload_size <= kSduLengthFieldSize) {
    PW_LOG_ERROR(
        "Maximum L2CAP payload size is smaller than minimum SDU size.");
  }
}

Status L2capCoc::DoCheckWriteParameter(const multibuf::MultiBuf& payload) {
  if (payload.size() > tx_mtu_) {
    PW_LOG_ERROR(
        "Payload (%zu bytes) exceeds MTU (%d bytes). So will not process. "
        "local_cid: %#x, remote_cid: %#x",
        payload.size(),
        tx_mtu_,
        local_cid(),
        remote_cid());
    return Status::InvalidArgument();
  }
  return pw::OkStatus();
}

}  // namespace pw::bluetooth::proxy
