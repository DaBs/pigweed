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

#include "pw_bluetooth_proxy/internal/generic_l2cap_channel.h"

#include "pw_assert/check.h"

namespace pw::bluetooth::proxy::internal {

GenericL2capChannel::GenericL2capChannel(L2capChannel& channel)
    : connection_handle_(channel.connection_handle()),
      transport_(channel.transport()),
      local_cid_(channel.local_cid()),
      remote_cid_(channel.remote_cid()),
      impl_(channel) {}

StatusWithMultiBuf GenericL2capChannel::Write(multibuf::MultiBuf&& payload) {
  if (auto status = DoCheckWriteParameter(payload); !status.ok()) {
    return {status, std::move(payload)};
  }
  return impl_.Write(std::move(payload));
}

}  // namespace pw::bluetooth::proxy::internal
