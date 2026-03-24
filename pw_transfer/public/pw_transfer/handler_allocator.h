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

#include <limits>

#include "pw_allocator/allocator.h"
#include "pw_assert/assert.h"
#include "pw_containers/dynamic_deque.h"
#include "pw_result/result.h"
#include "pw_status/status.h"
#include "pw_stream/stream.h"
#include "pw_sync/lock_annotations.h"
#include "pw_sync/mutex.h"
#include "pw_transfer/handler.h"
#include "pw_transfer/transfer.h"

namespace pw::transfer {

class TransferHandlerAllocator;

/// A RAII handle for a registered transfer handler. The handler is closed and
/// deallocated when this object goes out of scope.
class TransferResource {
 public:
  /// Creates an invalid handle.
  TransferResource() = default;

  TransferResource(TransferHandlerAllocator& allocator, uint32_t resource_id)
      : allocator_(&allocator), resource_id_(resource_id) {}

  TransferResource(TransferResource&& other) noexcept {
    *this = std::move(other);
  }

  TransferResource& operator=(TransferResource&& other) noexcept {
    Close();
    allocator_ = other.allocator_;
    resource_id_ = other.resource_id_;
    other.allocator_ = nullptr;
    other.resource_id_ = 0;
    return *this;
  }

  ~TransferResource() { Close(); }

  /// Returns the resource ID associated with this handle. Returns 0 if invalid.
  uint32_t resource_id() const { return resource_id_; }

  /// Returns true if this handle is valid.
  explicit operator bool() const { return resource_id_ != 0; }

  /// Closes the handler if this is a valid handle.
  void Close();

 private:
  TransferHandlerAllocator* allocator_ = nullptr;
  uint32_t resource_id_ = 0;
};

/// Manages a list of active pw::transfer::Handler instances.
///
/// The resource_id is dynamically assigned and registered with the
/// TransferService on creation and wrapped in a TransferResource. It's released
/// when TransferResource is destroyed.
///
/// A range of resource id's can be specified to allocate from. The range must
/// be between `1` and `std::numeric_limits<uint32_t>::max() - 1` inclusive.
///
/// Takes in a pw::Allocator and allocates memory for the Handler instances and
/// list to store them.
///
/// When allocating a handle it takes a pw::Stream for either reading, writing,
/// or both and create the right kind of Handler based on that.
class TransferHandlerAllocator {
 public:
  /// Use to size allocator passed to constructor.
  static constexpr size_t GetAllocatorSize(size_t max_handlers,
                                           size_t allocator_block_overhead) {
    constexpr auto handler_entry_layout = allocator::Layout::Of<HandlerEntry>();
    static_assert(sizeof(Handler) == sizeof(ReadWriteHandler));
    static_assert(sizeof(Handler) == sizeof(ReadOnlyHandler));
    static_assert(sizeof(Handler) == sizeof(WriteOnlyHandler));
    constexpr auto handler_layout = allocator::Layout::Of<Handler>();
    return max_handlers *
               (handler_entry_layout.size() + handler_entry_layout.alignment() +
                handler_layout.size() + handler_layout.alignment() +
                allocator_block_overhead) +
           // Max expected overhead of active_handlers_ allocation.
           2 * allocator_block_overhead;
  }

  TransferHandlerAllocator(
      TransferService& transfer_service,
      Allocator& allocator,
      uint32_t min_resource_id = 1,
      uint32_t max_resource_id = std::numeric_limits<uint32_t>::max() - 1)
      : transfer_service_(transfer_service),
        allocator_(allocator),
        active_handlers_(allocator),
        min_resource_id_(min_resource_id),
        max_resource_id_(max_resource_id),
        last_resource_id_(min_resource_id - 1) {
    PW_ASSERT(min_resource_id_ != 0);
    PW_ASSERT(max_resource_id_ >= min_resource_id_);
    PW_ASSERT(max_resource_id_ < std::numeric_limits<uint32_t>::max());
  }

  /// Allocates and registers a ReadHandler for the given reader.
  Result<TransferResource> AllocateReader(stream::Reader& reader);

  /// Allocates and registers a WriteHandler for the given writer.
  Result<TransferResource> AllocateWriter(stream::Writer& writer);

  /// Allocates and registers a ReadWriteHandler for the given reader_writer.
  Result<TransferResource> AllocateReadWriter(
      stream::ReaderWriter& reader_writer);

 private:
  friend class TransferResource;

  struct HandlerEntry {
    uint32_t resource_id;
    UniquePtr<Handler> handler;
  };

  // Closes and deallocates the handler associated with the given resource_id.
  void Close(uint32_t resource_id);

  bool IsIdInUse(uint32_t resource_id) PW_EXCLUSIVE_LOCKS_REQUIRED(lock_);

  template <typename HandlerType, typename... Args>
  Result<TransferResource> AllocateHandler(Args&... args);

  TransferService& transfer_service_;
  Allocator& allocator_ PW_GUARDED_BY(lock_);
  DynamicDeque<HandlerEntry> active_handlers_ PW_GUARDED_BY(lock_);
  const uint32_t min_resource_id_;
  const uint32_t max_resource_id_;
  uint32_t last_resource_id_ PW_GUARDED_BY(lock_);
  sync::Mutex lock_;
};

}  // namespace pw::transfer
