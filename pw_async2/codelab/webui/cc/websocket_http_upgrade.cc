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

#include "websocket_http_upgrade.h"

#include <array>
#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <string_view>

#include "mbedtls/sha1.h"
#include "pw_base64/base64.h"
#include "pw_result/result.h"
#include "pw_span/span.h"
#include "pw_status/status.h"
#include "pw_string/string_builder.h"

namespace pw::experimental::websocket::http_upgrade {
namespace {

constexpr size_t kNonceKeySize = 16;
constexpr size_t kSha1DigestSize = 20;

static_assert(kBase64EncodedNonceKeySize == base64::EncodedSize(kNonceKeySize));
static_assert(kBase64EncodedReplyKeySize ==
              base64::EncodedSize(kSha1DigestSize));

using namespace std::string_view_literals;

constexpr auto kHttpTextLineEnd = "\xd\xa"sv;

constexpr auto kBadRequestStatus = "HTTP/1.1 400 Bad Request"sv;

// Latest version defined by RFC 6455 (December 2011)
constexpr auto kSupportedWebsocketVersionHeader = "Sec-WebSocket-Version: 13"sv;

// Defined by RFC 6455 (December 2011)
constexpr auto kWebsocketUpgradeGUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"sv;

[[nodiscard]] std::string_view BytesAsStringView(span<const std::byte> value) {
  return {reinterpret_cast<const char*>(value.data()), value.size()};
}

}  // namespace

Result<span<const std::byte, kBase64EncodedReplyKeySize>> GenerateReplyKey(
    span<const std::byte, kBase64EncodedNonceKeySize> key_in,
    span<std::byte, kBase64EncodedReplyKeySize> key_out) {
  if (key_in.size() != kBase64EncodedNonceKeySize) {
    return Status::InvalidArgument();
  }
  if (key_out.size() < kBase64EncodedReplyKeySize) {
    return Status::InvalidArgument();
  }

  std::array<unsigned char, kSha1DigestSize> sha1_digest{};

  mbedtls_sha1_context sha1_context{};

  mbedtls_sha1_init(&sha1_context);
  if (mbedtls_sha1_starts(&sha1_context) != 0) {
    return Status::Internal();
  }
  if (mbedtls_sha1_update(&sha1_context,
                          reinterpret_cast<const unsigned char*>(key_in.data()),
                          key_in.size()) != 0) {
    return Status::Internal();
  }
  if (mbedtls_sha1_update(
          &sha1_context,
          reinterpret_cast<const unsigned char*>(kWebsocketUpgradeGUID.data()),
          kWebsocketUpgradeGUID.size()) != 0) {
    return Status::Internal();
  }

  if (mbedtls_sha1_finish(&sha1_context, sha1_digest.data()) != 0) {
    return Status::Internal();
  }

  mbedtls_sha1_free(&sha1_context);

  base64::Encode(as_bytes(span(sha1_digest)),
                 reinterpret_cast<char*>(key_out.data()));

  return key_out;
}

Result<span<const std::byte>> GenerateWebsocketUpgrade400BadRequest(
    span<std::byte> response_buffer) {
  pw::StringBuilder response(response_buffer);
  response.append(kBadRequestStatus);
  response.append(kHttpTextLineEnd);
  response.append(kHttpTextLineEnd);

  if (!response.status().ok()) {
    return response.status();
  }
  return response.as_bytes();
}

Result<span<const std::byte>> GenerateWebsocketUpgrade400BadRequestWithVersion(
    span<std::byte> response_buffer) {
  pw::StringBuilder response(response_buffer);
  response.append(kBadRequestStatus);
  response.append(kHttpTextLineEnd);
  response.append(kSupportedWebsocketVersionHeader);
  response.append(kHttpTextLineEnd);
  response.append(kHttpTextLineEnd);

  if (!response.status().ok()) {
    return response.status();
  }
  return response.as_bytes();
}

Result<span<const std::byte>> GenerateWebsocketUpgradeSuccess(
    span<const std::byte, kBase64EncodedReplyKeySize> reply_key,
    span<std::byte> response_buffer) {
  constexpr std::string_view kUpgradeSuccessPrefix =
      "HTTP/1.1 101 Switching Protocols\xd\xa"
      "Upgrade: websocket\xd\xa"
      "Connection: Upgrade\xd\xa"
      "Sec-WebSocket-Accept: "sv;

  pw::StringBuilder response(response_buffer);
  response.append(kUpgradeSuccessPrefix);
  response.append(BytesAsStringView(reply_key));
  response.append(kHttpTextLineEnd);
  response.append(kHttpTextLineEnd);

  if (!response.status().ok()) {
    return response.status();
  }
  return response.as_bytes();
}

bool IsHttpRequestHeaderOnly(span<const std::byte> request_bytes) {
  const auto request = BytesAsStringView(request_bytes);
  return request.size() >= 4 &&
         request.find("\r\n\r\n"sv) == request.size() - 4;
}

Result<span<const std::byte>> FindUniqueHttpHeaderPrefixAndReturnRemaining(
    span<const std::byte> request_bytes, span<const std::byte> prefix_bytes) {
  const auto request = BytesAsStringView(request_bytes);
  const auto prefix = BytesAsStringView(prefix_bytes);

  const auto prefix_start = request.find(prefix);
  if (prefix_start == std::string_view::npos) {
    return Status::NotFound();
  }
  if (prefix_start < 2) {
    return Status::FailedPrecondition();
  }
  if (request.substr(prefix_start - 2, 2) != kHttpTextLineEnd) {
    return Status::FailedPrecondition();
  }

  const auto prefix_end = prefix_start + prefix.size();

  if (request.find(prefix, prefix_end) != std::string_view::npos) {
    return Status::FailedPrecondition();
  }

  const auto remaining_end = request.find(kHttpTextLineEnd, prefix_end);
  if (remaining_end == std::string_view::npos) {
    return Status::FailedPrecondition();
  }

  return as_bytes(span(request.substr(prefix_end, remaining_end - prefix_end)));
}

bool HasUniqueHttpHeader(span<const std::byte> request_bytes,
                         span<const std::byte> header_bytes) {
  auto result =
      FindUniqueHttpHeaderPrefixAndReturnRemaining(request_bytes, header_bytes);
  return (result.ok() && result->empty());
}

Result<span<const std::byte, kBase64EncodedNonceKeySize>>
CheckWebsocketUpgradeRequestAndGetKey(span<const std::byte> request_bytes) {
  if (!IsHttpRequestHeaderOnly(request_bytes)) {
    return Status::FailedPrecondition();
  }

  constexpr auto kConnectionUpgradeHeader = "Connection: Upgrade"sv;
  if (!HasUniqueHttpHeader(request_bytes,
                           as_bytes(span(kConnectionUpgradeHeader)))) {
    return Status::FailedPrecondition();
  }

  constexpr auto kUpgradeWebsocketHeader = "Upgrade: websocket"sv;
  if (!HasUniqueHttpHeader(request_bytes,
                           as_bytes(span(kUpgradeWebsocketHeader)))) {
    return Status::FailedPrecondition();
  }

  if (!HasUniqueHttpHeader(request_bytes,
                           as_bytes(span(kSupportedWebsocketVersionHeader)))) {
    return Status::Unavailable();
  }

  constexpr auto kSecWebSocketKeyHeader = "Sec-WebSocket-Key: "sv;
  const auto found_security_key = FindUniqueHttpHeaderPrefixAndReturnRemaining(
      request_bytes, as_bytes(span(kSecWebSocketKeyHeader)));
  if (!found_security_key.ok()) {
    return Status::FailedPrecondition();
  }

  const auto security_key_bytes = as_bytes(*found_security_key);
  if (security_key_bytes.size() != kBase64EncodedNonceKeySize) {
    return Status::FailedPrecondition();
  }

  return security_key_bytes.subspan<0, kBase64EncodedNonceKeySize>();
}

Result<span<const std::byte>> ProcessHttpWebsocketUpgradeRequest(
    span<const std::byte> request,
    span<std::byte> response_buffer,
    bool& out_valid_upgrade_request) {
  out_valid_upgrade_request = false;
  Result<span<const std::byte, kBase64EncodedNonceKeySize>> request_key_result =
      CheckWebsocketUpgradeRequestAndGetKey(request);
  if (!request_key_result.ok()) {
    if (request_key_result.status().IsUnavailable()) {
      return GenerateWebsocketUpgrade400BadRequestWithVersion(response_buffer);
    }
    return GenerateWebsocketUpgrade400BadRequest(response_buffer);
  }

  std::array<std::byte, kBase64EncodedReplyKeySize> response_key_buffer{};
  auto response_key_result =
      GenerateReplyKey(*request_key_result, response_key_buffer);
  if (!response_key_result.ok()) {
    return GenerateWebsocketUpgrade400BadRequest(response_buffer);
  }

  out_valid_upgrade_request = true;
  return GenerateWebsocketUpgradeSuccess(*response_key_result, response_buffer);
}

}  // namespace pw::experimental::websocket::http_upgrade
