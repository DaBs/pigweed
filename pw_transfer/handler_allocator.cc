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

#include "pw_transfer/handler_allocator.h"

#include <mutex>

namespace pw::transfer {

bool TransferHandlerAllocator::IsIdInUse(uint32_t resource_id) {
  for (const auto& entry : active_handlers_) {
    if (entry.resource_id == resource_id) {
      return true;
    }
  }
  return false;
}

template <typename HandlerType, typename... Args>
Result<TransferResource> TransferHandlerAllocator::AllocateHandler(
    Args&... args) {
  std::lock_guard lock(lock_);
  std::optional<uint32_t> resource_id;

  // Search for an available resource ID. This is not optimized, and expects
  // that the number of handlers is small.
  uint32_t search_start = last_resource_id_ + 1;
  if (search_start > max_resource_id_ || search_start < min_resource_id_) {
    search_start = min_resource_id_;
  }

  // Loop from search_start to max_resource_id_.
  for (uint32_t id = search_start; id <= max_resource_id_; ++id) {
    if (id == 0) {
      continue;
    }
    if (!IsIdInUse(id)) {
      resource_id = id;
      break;
    }
  }

  if (!resource_id.has_value()) {
    // Loop from min_resource_id_ to search_start.
    for (uint32_t id = min_resource_id_; id < search_start; ++id) {
      if (id == 0) {
        continue;
      }
      if (!IsIdInUse(id)) {
        resource_id = id;
        break;
      }
    }
  }

  if (!resource_id.has_value()) {
    return Status::ResourceExhausted();
  }

  auto handler = allocator_.MakeUnique<HandlerType>(*resource_id, args...);
  if (handler == nullptr) {
    return Status::ResourceExhausted();
  }

  if (!active_handlers_.try_emplace_back(HandlerEntry{
          .resource_id = *resource_id,
          .handler = std::move(handler),
      })) {
    return Status::ResourceExhausted();
  }

  Handler& handler_ref = *active_handlers_.back().handler.get();

  if (!transfer_service_.RegisterHandler(handler_ref)) {
    active_handlers_.pop_back();
    return Status::FailedPrecondition();
  }

  last_resource_id_ = *resource_id;

  return TransferResource(*this, *resource_id);
}

Result<TransferResource> TransferHandlerAllocator::AllocateReader(
    stream::Reader& reader) {
  return AllocateHandler<ReadOnlyHandler>(reader);
}

Result<TransferResource> TransferHandlerAllocator::AllocateWriter(
    stream::Writer& writer) {
  return AllocateHandler<WriteOnlyHandler>(writer);
}

Result<TransferResource> TransferHandlerAllocator::AllocateReadWriter(
    stream::ReaderWriter& reader_writer) {
  return AllocateHandler<ReadWriteHandler>(reader_writer);
}

void TransferHandlerAllocator::Close(uint32_t resource_id) {
  std::lock_guard lock(lock_);
  auto to_remove = std::find_if(active_handlers_.begin(),
                                active_handlers_.end(),
                                [resource_id](const HandlerEntry& entry) {
                                  return entry.resource_id == resource_id;
                                });

  if (to_remove == active_handlers_.end()) {
    return;
  }
  transfer_service_.UnregisterHandler(*to_remove->handler.get());
  active_handlers_.erase(to_remove);
}

void TransferResource::Close() {
  if (allocator_ != nullptr && resource_id_ != 0) {
    allocator_->Close(resource_id_);
  }
  resource_id_ = 0;
}

}  // namespace pw::transfer
