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

#include "webui/webui_server.h"

#include <array>
#include <cstddef>
#include <cstdlib>
#include <mutex>
#include <optional>
#include <string_view>
#include <thread>
#include <utility>

#include "pw_function/function.h"
#include "pw_log/log.h"
#include "pw_preprocessor/compiler.h"
#include "pw_result/result.h"
#include "pw_span/span.h"
#include "pw_status/status.h"
#include "pw_stream/socket_stream.h"
#include "pw_string/string_builder.h"
#include "pw_sync/interrupt_spin_lock.h"
#include "pw_sync/lock_annotations.h"
#include "websocket_frame_protocol.h"
#include "websocket_http_upgrade.h"
#include "webui/resources.h"

namespace codelab::webui {
namespace {

using namespace std::string_view_literals;

constexpr size_t kMaxResponseHeaderSize = 128;
constexpr size_t kRequestBufferSize = 2048;
constexpr size_t kResponseBufferSize = 16384;

static_assert(kMaxResponseHeaderSize + resources::main_js.size() <
              kResponseBufferSize);
static_assert(kMaxResponseHeaderSize + resources::main_css.size() <
              kResponseBufferSize);
static_assert(kMaxResponseHeaderSize + resources::index_html.size() <
              kResponseBufferSize);

[[nodiscard]] std::string_view BytesAsStringView(
    pw::span<const std::byte> bytes) {
  return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

pw::Result<pw::span<const std::byte>> StringBuilderAsBytes(
    pw::StringBuilder&& response) {
  if (!response.status().ok()) {
    return response.status();
  }
  return response.as_bytes();
}

pw::Result<pw::span<const std::byte>> Generate404ContentResponse(
    pw::span<std::byte> response_buffer) {
  auto response = pw::StringBuilder(response_buffer);

  response.append(
      "HTTP/1.1 404 Not Found\r\n"
      "Content-Type: text/plain\r\n"
      "Content-Length: 5\r\n"
      "\r\n"
      "404\r\n"sv);

  return StringBuilderAsBytes(std::move(response));
}

pw::Result<pw::span<const std::byte>> Generate200ContentResponse(
    pw::span<std::byte> response_buffer,
    std::string_view mime_type,
    pw::span<const std::byte> content) {
  auto response = pw::StringBuilder(response_buffer);

  response.Format(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: %.*s\r\n"
      "Content-Length: %zu\r\n"
      "\r\n",
      static_cast<int>(mime_type.size()),
      mime_type.data(),
      content.size());
  response.append(BytesAsStringView(content));

  return StringBuilderAsBytes(std::move(response));
}

pw::Result<pw::span<const std::byte>> ProcessHttpRequest(
    pw::span<const std::byte> request_bytes,
    pw::span<std::byte> response_buffer,
    bool& out_upgraded_to_websocket_protocol) {
  out_upgraded_to_websocket_protocol = false;

  auto request = BytesAsStringView(request_bytes);

  constexpr std::string_view get_main_js = "GET /main.js HTTP/1.1\r\n"sv;
  if (request.substr(0, get_main_js.size()) == get_main_js) {
    return Generate200ContentResponse(
        response_buffer,
        "text/javascript"sv,
        pw::span{resources::main_js.data(), resources::main_js.size()});
  }

  constexpr std::string_view get_main_css = "GET /main.css HTTP/1.1\r\n"sv;
  if (request.substr(0, get_main_css.size()) == get_main_css) {
    return Generate200ContentResponse(
        response_buffer,
        "text/css"sv,
        pw::span{resources::main_css.data(), resources::main_css.size()});
  }

  constexpr std::string_view get_index_html = "GET / HTTP/1.1\r\n"sv;
  if (request.substr(0, get_index_html.size()) == get_index_html) {
    return Generate200ContentResponse(
        response_buffer,
        "text/html"sv,
        pw::span{resources::index_html.data(), resources::index_html.size()});
  }

  constexpr std::string_view get_websocket_path =
      "GET /codelab/webui HTTP/1.1\r\n"sv;
  if (request.substr(0, get_websocket_path.size()) == get_websocket_path) {
    return pw::experimental::websocket::http_upgrade::
        ProcessHttpWebsocketUpgradeRequest(pw::as_bytes(pw::span(request)),
                                           response_buffer,
                                           out_upgraded_to_websocket_protocol);
  }

  return Generate404ContentResponse(response_buffer);
}

class ClientState {
 public:
  explicit ClientState(
      pw::stream::SocketStream&& connection,
      pw::Function<pw::Status(std::string_view)>& do_received_text)
      : connection_(std::move(connection)),
        do_received_text_(&do_received_text) {}

  pw::Result<pw::span<const std::byte>> HandleHttpRequest(
      pw::span<const std::byte> request_bytes) {
    auto result = ProcessHttpRequest(
        request_bytes, response_buffer_, using_websocket_protocol_);
    return result;
  }

  pw::Result<pw::span<const std::byte>> HandleWebsocketRequest(
      pw::span<std::byte> request_bytes) {
    pw::span<std::byte> response = response_buffer_;
    size_t used = 0;

    pw::span<std::byte> remaining_bytes = request_bytes;
    while (!remaining_bytes.empty()) {
      using namespace pw::experimental::websocket::frame_protocol;

      auto frame_result = DecodeFrame(remaining_bytes);
      if (!frame_result.ok()) {
        return frame_result.status();
      }

      auto received = *frame_result;
      if (!received.final_fragment) {
        PW_LOG_CRITICAL("Fragmented messages are not supported.");
        return pw::Status::FailedPrecondition();
      }
      if (!received.mask.has_value()) {
        PW_LOG_CRITICAL("Client frames must be masked.");
        return pw::Status::FailedPrecondition();
      }

      switch (received.opcode) {
        case Frame::kClose: {
          auto encode_result = EncodeFrame(Frame{.final_fragment = true,
                                                 .opcode = Frame::kClose,
                                                 .payload = received.payload},
                                           response);
          if (!encode_result.ok()) {
            return encode_result.status();
          }
          used += encode_result->size();
          break;
        }

        case Frame::kPing: {
          auto encode_result = EncodeFrame(Frame{.final_fragment = true,
                                                 .opcode = Frame::kPong,
                                                 .payload = received.payload},
                                           response);
          if (!encode_result.ok()) {
            return encode_result.status();
          }
          used += encode_result->size();
          break;
        }

        case Frame::kPong:
          PW_LOG_CRITICAL("Unexpected Pong");
          return pw::Status::InvalidArgument();
          break;

        case Frame::kText: {
          auto status = (*do_received_text_)(std::string_view(
              reinterpret_cast<const char*>(received.payload.data()),
              received.payload.size()));
          if (!status.ok()) {
            return status;
          }
          break;
        }

        case Frame::kContinuation:
        case Frame::kBinary:
        default:
          PW_LOG_CRITICAL("Unsupported opcode: %d",
                          static_cast<int>(received.opcode));
          return pw::Status::InvalidArgument();
      }
    }

    return response.subspan(0, used);
  }

  void HandleClientLoop() {
    while (!done_with_client_) {
      pw::Result<pw::span<std::byte>> request_result =
          connection_.Read(request_buffer_.data(), request_buffer_.size());
      if (!request_result.ok()) {
        PW_LOG_CRITICAL("read failed: %s", request_result.status().str());
        return;
      }

      pw::Result<pw::span<const std::byte>> response_result;
      bool was_using_websocket_protocol = using_websocket_protocol_;
      if (!using_websocket_protocol_) {
        response_result = HandleHttpRequest(*request_result);
      } else {
        response_result = HandleWebsocketRequest(*request_result);
      }

      if (response_result.ok()) {
        const auto response = *response_result;
        const auto write_status =
            connection_.Write(response.data(), response.size());
        if (!write_status.ok()) {
          PW_LOG_CRITICAL("write failed: %s", write_status.str());
          return;
        }
      } else {
        PW_LOG_CRITICAL("handling request failed: %s",
                        response_result.status().str());
      }

      if (!response_result.ok() || !using_websocket_protocol_) {
        done_with_client_ = true;
      }

      if (!was_using_websocket_protocol && using_websocket_protocol_) {
        PW_LOG_INFO("Websocket connection established");
        websocket_ready_ = true;
        Send(pw::as_bytes(pw::span("rset"sv)));
      }
    }
  }

  void Send(pw::span<const std::byte> data) {
    if (!websocket_ready_) {
      return;
    }

    using namespace pw::experimental::websocket::frame_protocol;

    constexpr size_t kEncodedBufferSize = 128;
    std::array<std::byte, kEncodedBufferSize> encoded_buffer;
    auto result = EncodeFrame(
        Frame{.final_fragment = true, .opcode = Frame::kText, .payload = data},
        encoded_buffer);

    if (!result.ok()) {
      PW_LOG_CRITICAL("Encoding frame failed: %s", result.status().str());
      return;
    }

    const auto encoded = *result;
    const auto write_status = connection_.Write(encoded.data(), encoded.size());
    if (!write_status.ok()) {
      PW_LOG_CRITICAL("write failed: %s", write_status.str());
    }
  }

 private:
  bool websocket_ready_ = false;
  bool done_with_client_ = false;
  bool using_websocket_protocol_ = false;
  pw::stream::SocketStream connection_;
  pw::Function<pw::Status(std::string_view)>* do_received_text_;
  std::array<std::byte, kRequestBufferSize> request_buffer_{};
  std::array<std::byte, kResponseBufferSize> response_buffer_{};
};

pw::sync::InterruptSpinLock client_lock;
std::optional<ClientState> client_state PW_GUARDED_BY(client_lock);

void AcceptConnectionsLoop(
    pw::Function<pw::Status(std::string_view)> do_received_text) {
  std::atexit([]() { std::_Exit(0); });

  constexpr int kListenPort = 8081;
  pw::stream::ServerSocket server;
  if (const pw::Status status = server.Listen(kListenPort); !status.ok()) {
    PW_LOG_CRITICAL(
        "Failed to listen on port %d: %s", kListenPort, status.str());
    return;
  }

  PW_LOG_INFO(
      "Webui server ready. Open http://localhost:%d/ to control your vending "
      "machine!",
      server.port());

  while (true) {
    PW_LOG_INFO("Waiting for client connection...");
    pw::Result<pw::stream::SocketStream> connection = server.Accept();
    if (!connection.ok()) {
      PW_LOG_CRITICAL("Failed to accept connection: %s",
                      connection.status().str());
      return;
    }

    ClientState* state = nullptr;

    {
      std::lock_guard lock(client_lock);
      client_state.emplace(std::move(*connection), do_received_text);
      state = &client_state.value();
    }

    PW_LOG_INFO("Client connected.");
    state->HandleClientLoop();
    PW_LOG_INFO("Client disconnected.");

    {
      std::lock_guard lock(client_lock);
      client_state.reset();
    }
  }

  PW_UNREACHABLE;
}

}  // namespace

void StartWebUIServer(
    pw::Function<pw::Status(std::string_view)> do_received_text) {
  std::thread server_thread(AcceptConnectionsLoop, std::move(do_received_text));
  server_thread.detach();
}

void SetDisplay([[maybe_unused]] std::string_view text) {
  std::lock_guard lock(client_lock);
  if (!client_state) {
    return;
  }

  constexpr size_t kBufferSize = 120;
  std::array<std::byte, kBufferSize> response_buffer{};
  auto response = pw::StringBuilder(response_buffer);
  response.append("msg:");
  response.append(text);
  client_state->Send(response.as_bytes());
}

void SetDispenserMotorState(int state) {
  std::lock_guard lock(client_lock);
  if (!client_state) {
    return;
  }

  constexpr size_t kBufferSize = 128;
  std::array<std::byte, kBufferSize> response_buffer{};
  auto response = pw::StringBuilder(response_buffer);
  response.Format("%+d", state);
  client_state->Send(response.as_bytes());
}

}  // namespace codelab::webui
