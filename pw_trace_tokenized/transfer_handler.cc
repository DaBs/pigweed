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

#include "pw_trace_tokenized/transfer_handler.h"

#include <climits>

#include "pw_assert/check.h"
#include "pw_containers/algorithm.h"
#include "pw_containers/inline_var_len_entry_queue.h"
#include "pw_containers/var_len_entry_queue.h"
#include "pw_log/log.h"
#include "pw_trace_tokenized/trace_buffer.h"

namespace pw::trace {
namespace {

TraceBufferReader trace_buffer_reader;

}  // namespace

TraceBufferReader& GetTraceBufferReader() { return trace_buffer_reader; }

size_t TraceBufferReader::MoveFromBlockCache(ByteSpan dest) {
  size_t len = std::min(dest.size(), block_cache_.size());
  if (len != 0) {
    pw::copy(block_cache_.begin(),
             block_cache_.begin() +
                 static_cast<span<std::byte>::difference_type>(len),
             dest.begin());
    block_cache_ = block_cache_.subspan(len);
  }
  return len;
}

size_t TraceBufferReader::MoveFromTraceBuffer(ByteSpan dest) {
  if (dest.empty()) {
    return 0;
  }
  InlineVarLenEntryQueue<>& trace_buffer_queue = trace::GetBuffer()->queue();
  VarLenEntryQueue dest_queue(dest);
  MoveVarLenEntries(trace_buffer_queue, dest_queue);
  return dest_queue.encoded_size_bytes();
}

StatusWithSize TraceBufferReader::DoRead(ByteSpan dest) {
  PW_CHECK(!dest.empty());

  size_t total = 0;
  size_t moved = MoveFromBlockCache(dest);
  dest = dest.subspan(moved);
  total += moved;

  moved = MoveFromTraceBuffer(dest);
  dest = dest.subspan(moved);
  total += moved;

  if (!dest.empty()) {
    moved = MoveFromTraceBuffer(block_buffer_);
    block_cache_ = ByteSpan(block_buffer_, moved);
    total += MoveFromBlockCache(dest);
  }

  return total == 0 ? StatusWithSize::OutOfRange() : StatusWithSize(total);
}

}  // namespace pw::trace
