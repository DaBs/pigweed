// Copyright 2024 The Pigweed Authors
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

#include <cstdint>

#include "lib/stdcompat/utility.h"
#include "pw_bluetooth/hci_h4.emb.h"
#include "pw_function/function.h"
#include "pw_span/span.h"

namespace pw::bluetooth::proxy {

/// @module{pw_bluetooth_proxy}

/// H4PacketInterface is an abstract interface for an H4 HCI packet.
///
/// Concrete subclasses are used directly in code so their functions will be
/// properly inlined. This abstract superclass just ensures a common interface
/// across the concrete subclasses.
class H4PacketInterface {
 public:
  using ReleaseFn = Function<void(const uint8_t*)>;

  constexpr H4PacketInterface() = default;
  H4PacketInterface(span<uint8_t> buffer,
                    size_t hci_offset,
                    ReleaseFn&& release_fn)
      : buffer_(buffer),
        hci_offset_(hci_offset),
        release_fn_(std::move(release_fn)) {}

  H4PacketInterface(const H4PacketInterface& other) = delete;
  H4PacketInterface& operator=(const H4PacketInterface& other) = delete;

  H4PacketInterface(H4PacketInterface&& other) { *this = std::move(other); }
  H4PacketInterface& operator=(H4PacketInterface&& other) {
    if (this != &other) {
      buffer_ = other.buffer_;
      hci_offset_ = other.hci_offset_;
      release_fn_ = std::move(other.release_fn_);
      other.Reset();
    }
    return *this;
  }

  virtual ~H4PacketInterface() {
    if (!buffer_.empty() && HasReleaseFn()) {
      release_fn_(buffer_.data());
    }
  }

  /// Returns HCI packet type indicator as defined in BT Core Spec Version 5.4 |
  /// Vol 4, Part A, Section 2.
  emboss::H4PacketType GetH4Type() const { return DoGetH4Type(); }

  /// Sets HCI packet type indicator.
  void SetH4Type(emboss::H4PacketType type) { DoSetH4Type(type); }

  /// Returns pw::span of HCI packet as defined in BT Core Spec Version 5.4 |
  /// Vol 4, Part E, Section 5.4.
  constexpr span<uint8_t> GetHciSpan() {
    return buffer_.empty() ? buffer_ : buffer_.subspan(hci_offset_);
  }
  constexpr span<const uint8_t> GetHciSpan() const {
    return buffer_.empty() ? buffer_ : buffer_.subspan(hci_offset_);
  }

  bool HasReleaseFn() { return bool{release_fn_}; }

  // Returns the release function (which could be empty) and resets the packet.
  // The caller takes ownership of the buffer and should already have stored it.
  ReleaseFn ResetAndReturnReleaseFn() {
    ReleaseFn fn = std::move(release_fn_);
    Reset();
    return fn;
  }

 protected:
  static constexpr std::uint8_t kH4PacketIndicatorSize = 1;

  constexpr span<uint8_t> buffer() { return buffer_; }
  constexpr span<const uint8_t> buffer() const { return buffer_; }

 private:
  virtual emboss::H4PacketType DoGetH4Type() const = 0;
  virtual void DoSetH4Type(emboss::H4PacketType) = 0;

  void Reset() {
    buffer_ = span<uint8_t>();
    hci_offset_ = 0;
    release_fn_ = ReleaseFn{};
  }

  span<uint8_t> buffer_;
  size_t hci_offset_ = 0;
  ReleaseFn release_fn_{};
};

/// H4PacketWithHci is an H4Packet backed by an HCI buffer.
class H4PacketWithHci final : public H4PacketInterface {
 public:
  constexpr H4PacketWithHci() = default;

  H4PacketWithHci(emboss::H4PacketType h4_type,
                  span<uint8_t> hci_span,
                  ReleaseFn&& release_fn = nullptr)
      : H4PacketInterface(hci_span, 0, std::move(release_fn)),
        h4_type_(h4_type) {}

  H4PacketWithHci(span<uint8_t> h4_span, ReleaseFn&& release_fn = nullptr)
      : H4PacketInterface(
            h4_span, kH4PacketIndicatorSize, std::move(release_fn)),
        h4_type_(emboss::H4PacketType{h4_span[0]}) {}

  H4PacketWithHci(H4PacketWithHci&& other) = default;
  H4PacketWithHci& operator=(H4PacketWithHci&& other) = default;

 private:
  emboss::H4PacketType DoGetH4Type() const override { return h4_type_; }
  void DoSetH4Type(emboss::H4PacketType h4_type) override {
    h4_type_ = h4_type;
  }

  emboss::H4PacketType h4_type_ = emboss::H4PacketType::UNKNOWN;
};

/// H4PacketWithH4 is an H4Packet backed by an H4 buffer.
class H4PacketWithH4 final : public H4PacketInterface {
 public:
  constexpr H4PacketWithH4() = default;

  H4PacketWithH4(span<uint8_t> h4_span, ReleaseFn&& release_fn = nullptr)
      : H4PacketInterface(
            h4_span, kH4PacketIndicatorSize, std::move(release_fn)) {}

  H4PacketWithH4(emboss::H4PacketType h4_type,
                 span<uint8_t> h4_span,
                 ReleaseFn&& release_fn = nullptr)
      : H4PacketWithH4(h4_span, std::move(release_fn)) {
    SetH4Type(h4_type);
  }

  constexpr span<uint8_t> GetH4Span() { return buffer(); }
  constexpr span<const uint8_t> GetH4Span() const { return buffer(); }

 private:
  emboss::H4PacketType DoGetH4Type() const override {
    span<const uint8_t> h4_span = GetH4Span();
    return !h4_span.empty() ? emboss::H4PacketType(h4_span[0])
                            : emboss::H4PacketType::UNKNOWN;
  }

  void DoSetH4Type(emboss::H4PacketType h4_type) override {
    span<uint8_t> h4_span = GetH4Span();
    if (!h4_span.empty()) {
      h4_span.data()[0] = cpp23::to_underlying(h4_type);
    }
  }
};

}  // namespace pw::bluetooth::proxy
