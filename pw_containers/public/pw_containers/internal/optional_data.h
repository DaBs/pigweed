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

#include "pw_assert/assert.h"
#include "pw_preprocessor/compiler.h"

namespace pw::containers::internal {

// Internal implementation helpers for Optional and Result. Closely based on
// absl::StatusOr.

// Helper base class to hold the data and all operations. We move all this to a
// base class to allow mixing with the appropriate TraitsBase specialization.
//
// Pigweed addition: Specialize StatusOrData for trivially destructible types.
// This makes a Result usable in a constexpr statement.
//
// Note: in C++20, this entire file can be greatly simplfied with the requires
// statement.
template <typename T,
          typename State,
          State kHasValueState,
          bool = std::is_trivially_destructible<T>::value>
class OptionalData;

#define PW_OPTIONAL_DATA_IMPL                                              \
  template <typename U, typename S, S k, bool b>                           \
  friend class OptionalData;                                               \
                                                                           \
 public:                                                                   \
  OptionalData() = delete;                                                 \
                                                                           \
  PW_MODIFY_DIAGNOSTICS_PUSH();                                            \
  PW_MODIFY_DIAGNOSTIC_GCC(ignored, "-Wmaybe-uninitialized");              \
                                                                           \
  constexpr OptionalData(const OptionalData& other)                        \
      : empty_(), state_(other.state_) {                                   \
    if (other.state_ == kHasValueState) {                                  \
      MakeValue(other.data_);                                              \
    }                                                                      \
  }                                                                        \
                                                                           \
  constexpr OptionalData(OptionalData&& other) noexcept                    \
      : empty_(), state_(other.state_) {                                   \
    if (other.state_ == kHasValueState) {                                  \
      MakeValue(std::move(other.data_));                                   \
    }                                                                      \
  }                                                                        \
                                                                           \
  template <typename U, bool b>                                            \
  explicit constexpr OptionalData(                                         \
      const OptionalData<U, State, kHasValueState, b>& other)              \
      : empty_() {                                                         \
    if (other.state_ == kHasValueState) {                                  \
      MakeValue(other.data_);                                              \
      state_ = kHasValueState;                                             \
    } else {                                                               \
      state_ = other.state_;                                               \
    }                                                                      \
  }                                                                        \
                                                                           \
  template <typename U, bool b>                                            \
  explicit constexpr OptionalData(                                         \
      OptionalData<U, State, kHasValueState, b>&& other)                   \
      : empty_() {                                                         \
    if (other.state_ == kHasValueState) {                                  \
      MakeValue(std::move(other.data_));                                   \
      state_ = kHasValueState;                                             \
    } else {                                                               \
      state_ = other.state_;                                               \
    }                                                                      \
  }                                                                        \
                                                                           \
  template <typename... Args>                                              \
  explicit constexpr OptionalData(std::in_place_t, Args&&... args)         \
      : data_(std::forward<Args>(args)...), state_(kHasValueState) {}      \
                                                                           \
  explicit constexpr OptionalData(const T& value)                          \
      : data_(value), state_(kHasValueState) {}                            \
  explicit constexpr OptionalData(T&& value)                               \
      : data_(std::move(value)), state_(kHasValueState) {}                 \
                                                                           \
  explicit constexpr OptionalData(State no_value_state)                    \
      : empty_(), state_(no_value_state) {                                 \
    PW_ASSERT(no_value_state != kHasValueState);                           \
  }                                                                        \
                                                                           \
  constexpr OptionalData& operator=(const OptionalData& other) {           \
    if (this == &other) {                                                  \
      return *this;                                                        \
    }                                                                      \
    if (other.state_ == kHasValueState) {                                  \
      Assign(other.data_);                                                 \
    } else {                                                               \
      AssignState(other.state_);                                           \
    }                                                                      \
    return *this;                                                          \
  }                                                                        \
                                                                           \
  constexpr OptionalData& operator=(OptionalData&& other) noexcept(        \
      std::is_nothrow_move_assignable<T>::value &&                         \
      std::is_nothrow_move_constructible<T>::value) {                      \
    if (this == &other) {                                                  \
      return *this;                                                        \
    }                                                                      \
    if (other.state_ == kHasValueState) {                                  \
      Assign(std::move(other.data_));                                      \
    } else {                                                               \
      AssignState(other.state_);                                           \
    }                                                                      \
    return *this;                                                          \
  }                                                                        \
                                                                           \
  template <typename U>                                                    \
  constexpr void Assign(U&& value) {                                       \
    if (state_ == kHasValueState) {                                        \
      data_ = std::forward<U>(value);                                      \
    } else {                                                               \
      MakeValue(std::forward<U>(value));                                   \
      state_ = kHasValueState;                                             \
    }                                                                      \
  }                                                                        \
                                                                           \
  constexpr void AssignState(State no_value_state) {                       \
    Clear();                                                               \
    state_ = no_value_state;                                               \
    PW_ASSERT(no_value_state != kHasValueState);                           \
  }                                                                        \
                                                                           \
  template <State kState>                                                  \
  constexpr void AssignState() {                                           \
    Clear();                                                               \
    state_ = kState;                                                       \
    static_assert(                                                         \
        kState != kHasValueState,                                          \
        "Cannot set state to the has-value state; set the value instead"); \
  }                                                                        \
                                                                           \
 protected:                                                                \
  struct Empty {};                                                         \
  union {                                                                  \
    Empty empty_;                                                          \
    std::remove_const_t<T> data_;                                          \
  };                                                                       \
  State state_;                                                            \
                                                                           \
  constexpr void Clear() {                                                 \
    if constexpr (!std::is_trivially_destructible_v<T>) {                  \
      if (state_ == kHasValueState) {                                      \
        data_.~T();                                                        \
      }                                                                    \
    }                                                                      \
  }                                                                        \
  template <typename... Arg>                                               \
  constexpr void MakeValue(Arg&&... arg) {                                 \
    new (&data_) T(std::forward<Arg>(arg)...);                             \
  }                                                                        \
  PW_MODIFY_DIAGNOSTICS_POP();                                             \
  static_assert(true, "Macros must be terminated with a semicolon")

template <typename T, typename State, State kHasValueState>
class OptionalData<T, State, kHasValueState, true> {
  PW_OPTIONAL_DATA_IMPL;
};

template <typename T, typename State, State kHasValueState>
class OptionalData<T, State, kHasValueState, false> {
  PW_OPTIONAL_DATA_IMPL;

 public:
  ~OptionalData() { Clear(); }
};

#undef PW_OPTIONAL_DATA_IMPL

// Helper base classes to allow implicitly deleted constructors and assignment
// operators in `Optional`.
template <typename T, bool = std::is_copy_constructible<T>::value>
struct CopyCtorBase {
  CopyCtorBase() = default;
  CopyCtorBase(const CopyCtorBase&) = default;
  CopyCtorBase(CopyCtorBase&&) = default;
  CopyCtorBase& operator=(const CopyCtorBase&) = default;
  CopyCtorBase& operator=(CopyCtorBase&&) = default;
};

template <typename T>
struct CopyCtorBase<T, false> {
  CopyCtorBase() = default;
  CopyCtorBase(const CopyCtorBase&) = delete;
  CopyCtorBase(CopyCtorBase&&) = default;
  CopyCtorBase& operator=(const CopyCtorBase&) = default;
  CopyCtorBase& operator=(CopyCtorBase&&) = default;
};

template <typename T, bool = std::is_move_constructible<T>::value>
struct MoveCtorBase {
  MoveCtorBase() = default;
  MoveCtorBase(const MoveCtorBase&) = default;
  MoveCtorBase(MoveCtorBase&&) = default;
  MoveCtorBase& operator=(const MoveCtorBase&) = default;
  MoveCtorBase& operator=(MoveCtorBase&&) = default;
};

template <typename T>
struct MoveCtorBase<T, false> {
  MoveCtorBase() = default;
  MoveCtorBase(const MoveCtorBase&) = default;
  MoveCtorBase(MoveCtorBase&&) = delete;
  MoveCtorBase& operator=(const MoveCtorBase&) = default;
  MoveCtorBase& operator=(MoveCtorBase&&) = default;
};

template <typename T,
          bool = std::is_copy_constructible<T>::value &&
                 std::is_copy_assignable<T>::value>
struct CopyAssignBase {
  CopyAssignBase() = default;
  CopyAssignBase(const CopyAssignBase&) = default;
  CopyAssignBase(CopyAssignBase&&) = default;
  CopyAssignBase& operator=(const CopyAssignBase&) = default;
  CopyAssignBase& operator=(CopyAssignBase&&) = default;
};

template <typename T>
struct CopyAssignBase<T, false> {
  CopyAssignBase() = default;
  CopyAssignBase(const CopyAssignBase&) = default;
  CopyAssignBase(CopyAssignBase&&) = default;
  CopyAssignBase& operator=(const CopyAssignBase&) = delete;
  CopyAssignBase& operator=(CopyAssignBase&&) = default;
};

template <typename T,
          bool = std::is_move_constructible<T>::value &&
                 std::is_move_assignable<T>::value>
struct MoveAssignBase {
  MoveAssignBase() = default;
  MoveAssignBase(const MoveAssignBase&) = default;
  MoveAssignBase(MoveAssignBase&&) = default;
  MoveAssignBase& operator=(const MoveAssignBase&) = default;
  MoveAssignBase& operator=(MoveAssignBase&&) = default;
};

template <typename T>
struct MoveAssignBase<T, false> {
  MoveAssignBase() = default;
  MoveAssignBase(const MoveAssignBase&) = default;
  MoveAssignBase(MoveAssignBase&&) = default;
  MoveAssignBase& operator=(const MoveAssignBase&) = default;
  MoveAssignBase& operator=(MoveAssignBase&&) = delete;
};

}  // namespace pw::containers::internal
