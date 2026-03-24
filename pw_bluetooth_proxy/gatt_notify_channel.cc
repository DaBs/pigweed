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

#include "pw_bluetooth_proxy/gatt_notify_channel.h"

#include <mutex>

#include "pw_bluetooth/att.emb.h"
#include "pw_bluetooth/emboss_util.h"
#include "pw_bluetooth/l2cap_frames.emb.h"
#include "pw_log/log.h"
#include "pw_status/status.h"

namespace pw::bluetooth::proxy {

GattNotifyChannel::GattNotifyChannel(L2capChannel& channel,
                                     uint16_t attribute_handle)
    : internal::GenericL2capChannel(channel),
      attribute_handle_(attribute_handle) {
  std::optional<uint16_t> max_l2cap_payload_size =
      channel.MaxL2capPayloadSize();
  if (!max_l2cap_payload_size.has_value()) {
    PW_LOG_ERROR("Maximum L2CAP payload size is not set.");
  } else if (*max_l2cap_payload_size <=
             emboss::AttHandleValueNtf::MinSizeInBytes()) {
    PW_LOG_ERROR(
        "Maximum L2CAP payload size is smaller than minimum attribute size.");
  } else {
    max_attribute_size_ =
        *max_l2cap_payload_size - emboss::AttHandleValueNtf::MinSizeInBytes();
  }
}

Status GattNotifyChannel::DoCheckWriteParameter(
    const multibuf::MultiBuf& payload) {
  if (max_attribute_size_ == 0) {
    PW_LOG_WARN("Channel created before LE_Read_Buffer_Size processed.");
    return Status::FailedPrecondition();
  }
  if (payload.size() > max_attribute_size_) {
    PW_LOG_WARN("Attribute too large (%zu > %d). So will not process.",
                payload.size(),
                max_attribute_size_);
    return Status::InvalidArgument();
  }
  return OkStatus();
}

}  // namespace pw::bluetooth::proxy
