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

#include "pw_bluetooth_proxy/config.h"

#if PW_BLUETOOTH_PROXY_ASYNC == 0

namespace pw::bluetooth::proxy {

class ProxyHost;

namespace internal {

/// Implementation detail class for ProxyHost.
///
/// This type is used when `PW_BLUETOOTH_PROXY_ASYNC` is false; that is, when
/// the proxy is using a purely synchronous execution model using one or more
/// threads and using mutexes to protect shared state from concurrent
/// modification.
///
/// In this case, no extra fields are needed, so this is simply an empty struct.
class ProxyHostImpl {
 public:
  ProxyHostImpl(ProxyHost&) {}
};

}  // namespace internal
}  // namespace pw::bluetooth::proxy

#endif  // PW_BLUETOOTH_PROXY_ASYNC
