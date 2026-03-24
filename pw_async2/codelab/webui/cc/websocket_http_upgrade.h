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

#include <cstddef>
#include <cstdlib>
#include <optional>
#include <string_view>

#include "pw_bytes/span.h"
#include "pw_result/result.h"
#include "pw_span/span.h"

namespace pw::experimental::websocket::http_upgrade {

inline constexpr size_t kBase64EncodedNonceKeySize = 24;
inline constexpr size_t kBase64EncodedReplyKeySize = 28;

Result<span<const std::byte, kBase64EncodedReplyKeySize>> GenerateReplyKey(
    span<const std::byte, kBase64EncodedNonceKeySize> key_in,
    span<std::byte, kBase64EncodedReplyKeySize> key_out);

Result<span<const std::byte>> GenerateWebsocketUpgrade400BadRequest(
    span<std::byte> response_buffer);

Result<span<const std::byte>> GenerateWebsocketUpgrade400BadRequestWithVersion(
    span<std::byte> response_buffer);

Result<span<const std::byte>> GenerateWebsocketUpgradeSuccess(
    span<const std::byte, kBase64EncodedReplyKeySize> reply_key,
    span<std::byte> response_buffer);

bool IsHttpRequestHeaderOnly(span<const std::byte> request_bytes);

bool HasUniqueHttpHeader(span<const std::byte> request_bytes,
                         span<const std::byte> header_bytes);

Result<span<const std::byte>> FindUniqueHttpHeaderPrefixAndReturnRemaining(
    span<const std::byte> request_bytes, span<const std::byte> prefix_bytes);

Result<span<const std::byte, kBase64EncodedNonceKeySize>>
CheckWebsocketUpgradeRequestAndGetKey(span<const std::byte> request);

Result<span<const std::byte>> ProcessHttpWebsocketUpgradeRequest(
    span<const std::byte> request,
    span<std::byte> response_buffer,
    bool& out_valid_upgrade_request);

}  // namespace pw::experimental::websocket::http_upgrade
