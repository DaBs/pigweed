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

#define PW_TRACE_MODULE_NAME "TST"

#include "pw_trace_tokenized/transfer_handler.h"

#include "pw_bytes/span.h"
#include "pw_result/result.h"
#include "pw_string/string_builder.h"
#include "pw_tokenizer/detokenize.h"
#include "pw_trace/trace.h"
#include "pw_trace_tokenized/decoder.h"
#include "pw_trace_tokenized/trace_buffer.h"
#include "pw_unit_test/framework.h"

namespace {

using pw::tokenizer::Detokenizer;
using pw::trace::DecodedEvent;
using pw::trace::EventType;
using pw::trace::TokenizedDecoder;
using pw::trace::TraceBufferReader;
using pw::trace::TraceTransferHandler;

#define PW_TRACE_TEST_ADD_TO_DECODER(label, builder)            \
  do {                                                          \
    uint32_t _token = PW_TRACE_REF(PW_TRACE_EVENT_TYPE_INSTANT, \
                                   PW_TRACE_MODULE_NAME,        \
                                   label,                       \
                                   PW_TRACE_FLAGS_DEFAULT,      \
                                   "");                         \
    builder.Add(_token, "PW_TRACE_EVENT_TYPE_INSTANT", label);  \
  } while (0)

// Helper class that can build a token database as a CSV string, and return a
// decoder that uses it.
class TokenizedDecoderBuilder {
 public:
  static constexpr uint64_t kTicksPerSec = 1000;  // 1 kHz

  // Appends a CSV entry to the internal buffer.
  void Add(uint32_t token, const char* event_type, const char* label) {
    pw::StringBuffer<64> sbuf;
    sbuf << event_type << "|0|" << PW_TRACE_MODULE_NAME << "||" << label;
    database_["trace"][token].emplace_back(
        sbuf.c_str(), pw::tokenizer::TokenDatabase::kDateRemovedNever);
  }

  // Constructs the detokenizer and uses it to construct a decoder.
  //
  // The returned decoder contains a reference to this object's detokenizer. As
  // a result, this object must outlive the returned decoder.
  TokenizedDecoder Finalize() {
    PW_ASSERT(!detokenizer_.has_value());
    detokenizer_ = Detokenizer(std::move(database_));
    return TokenizedDecoder(detokenizer_.value(), kTicksPerSec);
  }

 private:
  pw::tokenizer::DomainTokenEntriesMap database_;
  std::optional<Detokenizer> detokenizer_;
};

// Unit tests.

TEST(TraceTransferHandler, ReadOnly) {
  TraceBufferReader reader;
  TraceTransferHandler handler(123, reader);
  EXPECT_EQ(handler.PrepareRead(), pw::OkStatus());
  EXPECT_EQ(handler.PrepareWrite(), pw::Status::PermissionDenied());
}

TEST(TraceBufferReader, ReadEmpty) {
  PW_TRACE_SET_ENABLED(true);
  pw::trace::ClearBuffer();

  TraceBufferReader reader;
  std::byte buffer[128];
  pw::Result<pw::ByteSpan> result = reader.Read(buffer);
  EXPECT_EQ(result.status(), pw::Status::OutOfRange());
}

TEST(TraceBufferReader, ReadData) {
  TokenizedDecoderBuilder decoder_builder;
  PW_TRACE_TEST_ADD_TO_DECODER("Test", decoder_builder);
  TokenizedDecoder decoder = decoder_builder.Finalize();

  PW_TRACE_SET_ENABLED(true);
  pw::trace::ClearBuffer();

  PW_TRACE_INSTANT("Test");

  // Check that contents match.
  TraceBufferReader reader;
  pw::Result<DecodedEvent> event = decoder.ReadSizePrefixed(reader);
  ASSERT_EQ(event.status(), pw::OkStatus());

  EXPECT_EQ(event->type, EventType::PW_TRACE_EVENT_TYPE_INSTANT);
  EXPECT_STREQ(event->flags_str.c_str(), "0");
  EXPECT_STREQ(event->module.c_str(), PW_TRACE_MODULE_NAME);
  EXPECT_STREQ(event->group.c_str(), "");
  EXPECT_STREQ(event->label.c_str(), "Test");

  // Second read should be OutOfRange as the buffer is now empty.
  std::byte buffer[128];
  pw::Result<pw::ByteSpan> encoded = reader.Read(buffer);
  EXPECT_EQ(encoded.status(), pw::Status::OutOfRange());
}

TEST(TraceBufferReader, ReadPartial) {
  TokenizedDecoderBuilder decoder_builder;
  PW_TRACE_TEST_ADD_TO_DECODER("Test", decoder_builder);
  PW_TRACE_TEST_ADD_TO_DECODER("Test2", decoder_builder);
  TokenizedDecoder decoder = decoder_builder.Finalize();

  pw::trace::ClearBuffer();
  PW_TRACE_SET_ENABLED(true);
  PW_TRACE_INSTANT("Test");
  PW_TRACE_INSTANT("Test2");

  // Read a single byte first. Reading should succeed if any data is available.
  TraceBufferReader reader;
  std::byte buffer[128];
  pw::ByteSpan bytes(buffer);
  pw::Result<pw::ByteSpan> result = reader.Read(bytes.subspan(0, 1));
  ASSERT_EQ(result.status(), pw::OkStatus());
  size_t total_size = result->size();

  // Read the rest.
  result = reader.Read(bytes.subspan(1));
  ASSERT_EQ(result.status(), pw::OkStatus());
  total_size += result->size();
  bytes = bytes.subspan(0, total_size);

  // Third read should be OutOfRange.
  result = reader.Read(bytes);
  EXPECT_EQ(result.status(), pw::Status::OutOfRange());

  // Check that contents match. We don't have an easy way to know how many bytes
  // correspond to each object, so try incrementally. In practice, callers will
  // use `ReadSizePrefixed` as in the previous unit test.
  pw::Result<DecodedEvent> event = pw::Status::OutOfRange();
  for (size_t i = 1; i < bytes.size(); ++i) {
    size_t offset = pw::varint::EncodedSize(i);
    if (i <= offset) {
      continue;
    }
    event = decoder.Decode(bytes.subspan(offset, i - offset));
    if (event.ok()) {
      bytes = bytes.subspan(i);
      break;
    }
  }
  ASSERT_EQ(event.status(), pw::OkStatus());

  EXPECT_EQ(event->type, EventType::PW_TRACE_EVENT_TYPE_INSTANT);
  EXPECT_STREQ(event->flags_str.c_str(), "0");
  EXPECT_STREQ(event->module.c_str(), PW_TRACE_MODULE_NAME);
  EXPECT_STREQ(event->group.c_str(), "");
  EXPECT_STREQ(event->label.c_str(), "Test");

  size_t offset = pw::varint::EncodedSize(bytes.size());
  event = decoder.Decode(bytes.subspan(offset));
  ASSERT_EQ(event.status(), pw::OkStatus());

  EXPECT_EQ(event->type, EventType::PW_TRACE_EVENT_TYPE_INSTANT);
  EXPECT_STREQ(event->flags_str.c_str(), "0");
  EXPECT_STREQ(event->module.c_str(), PW_TRACE_MODULE_NAME);
  EXPECT_STREQ(event->group.c_str(), "");
  EXPECT_STREQ(event->label.c_str(), "Test2");
}

#undef PW_TRACE_TEST_ADD_TO_DECODER
#undef _PW_TRACE_TEST_ADD_TO_DECODER

}  // namespace
