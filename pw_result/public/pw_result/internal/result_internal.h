// Copyright 2022 The Pigweed Authors
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

#include <type_traits>
#include <utility>

#include "pw_assert/assert.h"
#include "pw_status/status.h"

namespace pw {

template <typename T>
class [[nodiscard]] Result;

namespace internal_result {

// Detects whether `U` has conversion operator to `Result<T>`, i.e. `operator
// Result<T>()`.
template <typename T, typename U, typename = void>
struct HasConversionOperatorToResult : std::false_type {};

template <typename T, typename U>
void test(char (*)[sizeof(std::declval<U>().operator Result<T>())]);

template <typename T, typename U>
struct HasConversionOperatorToResult<T, U, decltype(test<T, U>(0))>
    : std::true_type {};

// Detects whether `T` is constructible or convertible from `Result<U>`.
template <typename T, typename U>
using IsConstructibleOrConvertibleFromResult =
    std::disjunction<std::is_constructible<T, Result<U>&>,
                     std::is_constructible<T, const Result<U>&>,
                     std::is_constructible<T, Result<U>&&>,
                     std::is_constructible<T, const Result<U>&&>,
                     std::is_convertible<Result<U>&, T>,
                     std::is_convertible<const Result<U>&, T>,
                     std::is_convertible<Result<U>&&, T>,
                     std::is_convertible<const Result<U>&&, T>>;

// Detects whether `T` is constructible or convertible or assignable from
// `Result<U>`.
template <typename T, typename U>
using IsConstructibleOrConvertibleOrAssignableFromResult =
    std::disjunction<IsConstructibleOrConvertibleFromResult<T, U>,
                     std::is_assignable<T&, Result<U>&>,
                     std::is_assignable<T&, const Result<U>&>,
                     std::is_assignable<T&, Result<U>&&>,
                     std::is_assignable<T&, const Result<U>&&>>;

// Detects whether direct initializing `Result<T>` from `U` is ambiguous, i.e.
// when `U` is `Result<V>` and `T` is constructible or convertible from `V`.
template <typename T, typename U>
struct IsDirectInitializationAmbiguous
    : public std::conditional_t<
          std::is_same<std::remove_cv_t<std::remove_reference_t<U>>, U>::value,
          std::false_type,
          IsDirectInitializationAmbiguous<
              T,
              std::remove_cv_t<std::remove_reference_t<U>>>> {};

template <typename T, typename V>
struct IsDirectInitializationAmbiguous<T, Result<V>>
    : public IsConstructibleOrConvertibleFromResult<T, V> {};

// Checks against the constraints of the direction initialization, i.e. when
// `Result<T>::Result(U&&)` should participate in overload resolution.
template <typename T, typename U>
using IsDirectInitializationValid = std::disjunction<
    // Short circuits if T is basically U.
    std::is_same<T, std::remove_cv_t<std::remove_reference_t<U>>>,
    std::negation<std::disjunction<
        std::is_same<Result<T>, std::remove_cv_t<std::remove_reference_t<U>>>,
        std::is_same<Status, std::remove_cv_t<std::remove_reference_t<U>>>,
        std::is_same<std::in_place_t,
                     std::remove_cv_t<std::remove_reference_t<U>>>,
        IsDirectInitializationAmbiguous<T, U>>>>;

// This trait detects whether `Result<T>::operator=(U&&)` is ambiguous, which
// is equivalent to whether all the following conditions are met:
// 1. `U` is `Result<V>`.
// 2. `T` is constructible and assignable from `V`.
// 3. `T` is constructible and assignable from `U` (i.e. `Result<V>`).
// For example, the following code is considered ambiguous:
// (`T` is `bool`, `U` is `Result<bool>`, `V` is `bool`)
//   Result<bool> s1 = true;  // s1.ok() && s1.ValueOrDie() == true
//   Result<bool> s2 = false;  // s2.ok() && s2.ValueOrDie() == false
//   s1 = s2;  // ambiguous, `s1 = s2.ValueOrDie()` or `s1 = bool(s2)`?
template <typename T, typename U>
struct IsForwardingAssignmentAmbiguous
    : public std::conditional_t<
          std::is_same<std::remove_cv_t<std::remove_reference_t<U>>, U>::value,
          std::false_type,
          IsForwardingAssignmentAmbiguous<
              T,
              std::remove_cv_t<std::remove_reference_t<U>>>> {};

template <typename T, typename U>
struct IsForwardingAssignmentAmbiguous<T, Result<U>>
    : public IsConstructibleOrConvertibleOrAssignableFromResult<T, U> {};

// Checks against the constraints of the forwarding assignment, i.e. whether
// `Result<T>::operator(U&&)` should participate in overload resolution.
template <typename T, typename U>
using IsForwardingAssignmentValid = std::disjunction<
    // Short circuits if T is basically U.
    std::is_same<T, std::remove_cv_t<std::remove_reference_t<U>>>,
    std::negation<std::disjunction<
        std::is_same<Result<T>, std::remove_cv_t<std::remove_reference_t<U>>>,
        std::is_same<Status, std::remove_cv_t<std::remove_reference_t<U>>>,
        std::is_same<std::in_place_t,
                     std::remove_cv_t<std::remove_reference_t<U>>>,
        IsForwardingAssignmentAmbiguous<T, U>>>>;

// This trait is for determining if a given type is a Result.
template <typename T>
constexpr bool IsResult = false;
template <typename T>
constexpr bool IsResult<Result<T>> = true;

// This trait determines the return type of a given function without const,
// volatile or reference qualifiers.
template <typename Fn, typename T>
using InvokeResultType =
    std::remove_cv_t<std::remove_reference_t<std::invoke_result_t<Fn, T>>>;

}  // namespace internal_result
}  // namespace pw
