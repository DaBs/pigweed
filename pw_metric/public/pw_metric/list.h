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

#include "pw_containers/intrusive_forward_list.h"

namespace pw::metric {

class Metric;
class Group;
class ResumableMetricWalker;
class MetricWalker;
class MetricService;

namespace internal {

/// Wraps a IntrusiveForwardList to limit the API for use with metrics.
template <typename T>
class ListWrapper {
 public:
  using iterator = typename IntrusiveForwardList<T>::iterator;
  using const_iterator = typename IntrusiveForwardList<T>::const_iterator;
  using element_type = typename IntrusiveForwardList<T>::element_type;
  using value_type = typename IntrusiveForwardList<T>::value_type;
  using size_type = typename IntrusiveForwardList<T>::size_type;
  using difference_type = typename IntrusiveForwardList<T>::difference_type;
  using reference = typename IntrusiveForwardList<T>::reference;
  using const_reference = typename IntrusiveForwardList<T>::const_reference;
  using pointer = typename IntrusiveForwardList<T>::pointer;
  using const_pointer = typename IntrusiveForwardList<T>::const_pointer;

  constexpr ListWrapper() = default;

  ~ListWrapper() { list_.clear(); }

  // Temporarily provide for backwards compatibility. Use push_front() since
  // order is not significant.
  void push_back(reference value) { list_.push_front(value); }
  void push_front(reference value) { list_.push_front(value); }

  // Expose specific methods required for metrics.
  [[nodiscard]] bool empty() const { return list_.empty(); }

  [[nodiscard]] size_t size() const {
    return static_cast<size_t>(std::distance(list_.begin(), list_.end()));
  }

  /// Iterates over the list and calls the functor `f` with a reference to each
  /// item.
  template <typename F>
  void ForEach(F&& f) {
    for (auto& item : list_) {
      f(item);
    }
  }

  /// Iterates over the list and calls the functor `f` with a const reference to
  /// each item.
  template <typename F>
  void ForEach(F&& f) const {
    for (const auto& item : list_) {
      f(item);
    }
  }

  [[deprecated(
      "For thread safety reasons, metrics lists no longer support direct "
      "iteration. Instead, call ForEach to iterate over items in the list.")]]
  auto begin() const {
    return list_.begin();
  }

  [[deprecated(
      "For thread safety reasons, metrics lists no longer support direct "
      "iteration. Instead, call ForEach to iterate over items in the list.")]]
  auto end() const {
    return list_.end();
  }

 private:
  // Allow pw_metric classes to modify and directly access the list.
  friend Metric;
  friend Group;

  friend ResumableMetricWalker;
  friend MetricWalker;
  friend MetricService;

  using Item = typename IntrusiveForwardList<T>::Item;

  IntrusiveForwardList<T>& list() { return list_; }
  const IntrusiveForwardList<T>& list() const { return list_; }

  IntrusiveForwardList<T> list_;
};

}  // namespace internal

/// A list of metrics.
using MetricList = internal::ListWrapper<Metric>;

/// A list of metrics groups.
using GroupList = internal::ListWrapper<Group>;

}  // namespace pw::metric
