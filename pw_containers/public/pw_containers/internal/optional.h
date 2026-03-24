// Copyright 2026 The Pigweed Authors
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

#include <new>
#include <type_traits>
#include <utility>

#ifdef __cpp_concepts
#include <concepts>
#endif  // __cpp_concepts

#include "pw_assert/assert.h"
#include "pw_containers/internal/optional_data.h"

namespace pw::containers::internal {

/// @submodule{pw_containers,utilities}

#ifdef __cpp_concepts
template <typename T>
concept OptionalState = std::regular<T>;
#define PW_OPTIONAL_STATE OptionalState
#else
#define PW_OPTIONAL_STATE
#endif  // __cpp_concepts

/// `Optional` is similar to `std::optional` and `pw::Result`, but uses a
/// user-specified state type for object presence. One state value is reserved
/// to indicate that the object is present.
///
/// `Optional` is essentially a customizable `pw::Result`, supporting custom
/// enums instead of only `pw::Status`. `pw::Result` cannot easily wrap a
/// `pw::Status`, so is not suitable as a general `T` wrapper. Construction or
/// assignment from a `Status` is ambiguous, since it is unclear if the `Status`
/// is a value or an error. These issues apply to `absl::StatusOr` as well (see
/// b/280392796). `Optional` resolves this by prohibiting conversions between
/// the value and state type and with a simpler construction/assignment API.
///
/// To simplify its use in templates, `Optional` is specialized for `void`.
///
/// @tparam T type of the optional value
/// @tparam kHasValueState State value that represents
template <typename T,
          PW_OPTIONAL_STATE auto kHasValueState,
          bool IsTriviallyDestructible = std::is_trivially_destructible_v<T>>
class Optional final : private OptionalData<T,
                                            decltype(kHasValueState),
                                            kHasValueState,
                                            IsTriviallyDestructible>,
                       private CopyCtorBase<T>,
                       private MoveCtorBase<T>,
                       private CopyAssignBase<T>,
                       private MoveAssignBase<T> {
 private:
  template <typename, PW_OPTIONAL_STATE auto, bool>
  friend class Optional;

  using Base = OptionalData<T,
                            decltype(kHasValueState),
                            kHasValueState,
                            IsTriviallyDestructible>;

 public:
  using state_type = decltype(kHasValueState);
  using value_type = T;

  Optional() = delete;

  /// Constructs an optional without a value using the provided state.
  explicit constexpr Optional(state_type state) : Base(state) {}

  /// Constructs an optional containing a value.
  constexpr Optional(const value_type& value) : Base(value) {}
  constexpr Optional(value_type&& value) : Base(std::move(value)) {}

  /// Constructs an optional in-place containing a value.
  template <typename... Args>
  explicit constexpr Optional(std::in_place_t, Args&&... args)
      : Base(std::in_place, std::forward<Args>(args)...) {}

  constexpr Optional(const Optional&) = default;
  constexpr Optional& operator=(const Optional&) = default;
  constexpr Optional(Optional&&) = default;
  constexpr Optional& operator=(Optional&&) = default;

  /// Constructs an optional from another optional with a compatible value type.
  template <
      typename U,
      std::enable_if_t<std::conjunction_v<std::negation<std::is_same<T, U>>,
                                          std::is_constructible<T, const U&>,
                                          std::is_convertible<const U&, T>>,
                       int> = 0>
  constexpr Optional(const Optional<U, kHasValueState>& other) : Base(other) {}

  template <
      typename U,
      std::enable_if_t<
          std::conjunction_v<std::negation<std::is_same<T, U>>,
                             std::is_constructible<T, const U&>,
                             std::negation<std::is_convertible<const U&, T>>>,
          int> = 0>
  explicit constexpr Optional(const Optional<U, kHasValueState>& other)
      : Base(other) {}

  template <
      typename U,
      std::enable_if_t<std::conjunction_v<std::negation<std::is_same<T, U>>,
                                          std::is_constructible<T, U&&>,
                                          std::is_convertible<U&&, T>>,
                       int> = 0>
  constexpr Optional(Optional<U, kHasValueState>&& other)
      : Base(std::move(other)) {}

  template <typename U,
            std::enable_if_t<
                std::conjunction_v<std::negation<std::is_same<T, U>>,
                                   std::is_constructible<T, U&&>,
                                   std::negation<std::is_convertible<U&&, T>>>,
                int> = 0>
  explicit constexpr Optional(Optional<U, kHasValueState>&& other)
      : Base(std::move(other)) {}

  /// Assigns a new value to the optional.
  constexpr Optional& operator=(const value_type& value) {
    this->Assign(value);
    return *this;
  }

  /// Move assigns a new value to the optional.
  constexpr Optional& operator=(value_type&& value) {
    this->Assign(std::move(value));
    return *this;
  }

  /// Constructs a new value in-place.
  template <typename... Args>
  constexpr void emplace(Args&&... args) {
    this->Clear();
    this->MakeValue(std::forward<Args>(args)...);
    this->state_ = kHasValueState;
  }

  /// Destroys the contained value, if any, and sets the state. The state must
  /// not be `kHasValueState`. To avoid a runtime check, use the
  /// `reset<kState>()` function template whenever possible.
  constexpr void reset(state_type state) { this->AssignState(state); }

  /// @copydoc reset
  template <state_type kState>
  constexpr void reset() {
    this->template AssignState<kState>();
  }

  /// Returns true if the optional contains a value.
  [[nodiscard]] constexpr bool has_value() const {
    return this->state_ == kHasValueState;
  }

  /// Accesses the contained value. Does NOT check if the value is present!
  ///
  /// @pre `has_value()` MUST be `true`.
  constexpr T& operator*() & { return this->data_; }
  /// @copydoc operator*
  constexpr const T& operator*() const& { return this->data_; }

  /// @copydoc operator*
  constexpr T&& operator*() && { return std::move(this->data_); }
  /// @copydoc operator*
  constexpr const T&& operator*() const&& { return std::move(this->data_); }

  /// @copydoc operator*
  constexpr T* operator->() { return &this->data_; }
  /// @copydoc operator*
  constexpr const T* operator->() const { return &this->data_; }

  /// Returns the contained value. Asserts if the value is not present.
  constexpr T& value() & {
    PW_ASSERT(has_value());
    return this->data_;
  }
  /// @copydoc value
  constexpr const T& value() const& {
    PW_ASSERT(has_value());
    return this->data_;
  }

  /// @copydoc value
  constexpr T&& value() && {
    PW_ASSERT(has_value());
    return std::move(this->data_);
  }
  /// @copydoc value
  constexpr const T&& value() const&& {
    PW_ASSERT(has_value());
    return std::move(this->data_);
  }

  /// Returns the state. Always valid, whether `has_value` is `true` or `false`.
  constexpr state_type state() const { return this->state_; }

  friend constexpr bool operator==(const Optional& lhs, const Optional& rhs) {
    if (lhs.has_value() && rhs.has_value()) {
      return *lhs == *rhs;
    }
    return lhs.state() == rhs.state();
  }
  friend constexpr bool operator!=(const Optional& lhs, const Optional& rhs) {
    return !(lhs == rhs);
  }

  friend constexpr bool operator==(const Optional& lhs, const T& rhs) {
    return lhs.has_value() && *lhs == rhs;
  }
  friend constexpr bool operator==(const T& lhs, const Optional& rhs) {
    return rhs == lhs;
  }
  friend constexpr bool operator!=(const Optional& lhs, const T& rhs) {
    return !(lhs == rhs);
  }
  friend constexpr bool operator!=(const T& lhs, const Optional& rhs) {
    return !(rhs == lhs);
  }

  static_assert(!std::is_convertible_v<T, state_type>,
                "To avoid ambiguity, T cannot be convertible to the state "
                "type. Consider using `enum class` to restrict conversions.");
  static_assert(!std::is_convertible_v<state_type, T>,
                "To avoid ambiguity, the state cannot be convertible to T "
                "type. Consider using `enum class` to restrict conversions.");
};

/// The `void` specialization of `Optional` is provided to simplify template
/// code. `Optional<void>` just the state type.
template <PW_OPTIONAL_STATE auto kHasValueState, bool IsTriviallyDestructible>
class Optional<void, kHasValueState, IsTriviallyDestructible> final {
 public:
  using state_type = decltype(kHasValueState);
  using value_type = void;

  constexpr Optional() = delete;

  constexpr Optional(const Optional&) = default;
  constexpr Optional(Optional&&) = default;

  constexpr Optional& operator=(const Optional&) = default;
  constexpr Optional& operator=(Optional&&) = default;

  /// Constructs an optional without a value using the provided state.
  explicit constexpr Optional(state_type state) : state_(state) {
    PW_ASSERT(state != kHasValueState);
  }

  /// Constructs an optional containing a value.
  explicit constexpr Optional(std::in_place_t) : state_(kHasValueState) {}

  /// Sets the state to indicate a value is present.
  constexpr void emplace() { state_ = kHasValueState; }

  /// Changes the state of the optional. The state must not be `kHasValueState`.
  /// To avoid a runtime check, use the `reset<kState>()` function template
  /// whenever possible.
  constexpr void reset(state_type state) {
    PW_ASSERT(state != kHasValueState);
    state_ = state;
  }

  /// @copydoc reset
  template <state_type kState>
  constexpr void reset() {
    static_assert(
        kState != kHasValueState,
        "Cannot set state to the has-value state; set the value instead");
    state_ = kState;
  }

  /// True if the optional contains a value.
  [[nodiscard]] constexpr bool has_value() const {
    return state_ == kHasValueState;
  }

  /// Asserts that `has_value()` is `true`.
  constexpr void value() const { PW_ASSERT(has_value()); }

  /// Returns the underlying state value.
  constexpr state_type state() const { return state_; }

  friend constexpr bool operator==(const Optional& lhs, const Optional& rhs) {
    return lhs.state() == rhs.state();
  }
  friend constexpr bool operator!=(const Optional& lhs, const Optional& rhs) {
    return lhs.state() != rhs.state();
  }

 private:
  state_type state_;
};

/// @endsubmodule

#undef PW_OPTIONAL_STATE

}  // namespace pw::containers::internal
