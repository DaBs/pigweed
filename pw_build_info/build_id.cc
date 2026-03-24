// Copyright 2021 The Pigweed Authors
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

#include "pw_build_info/build_id.h"

#include <cstdint>
#include <cstring>

#include "pw_preprocessor/compiler.h"
#include "pw_span/span.h"

namespace pw::build_info {
namespace {

PW_PACKED(struct) ElfNoteInfo {
  uint32_t name_size;
  uint32_t descriptor_size;
  uint32_t type;
};

// Symbol that points to the start of the .note.gnu.build-id ELF note section.
// The section layout is:
//   - ElfNoteInfo header
//   - 'name' field (name_size bytes from header)
//   - 'descriptor' field (descriptor_size bytes, the actual build identifier)
// This symbol only references the header.
extern "C" const ElfNoteInfo gnu_build_id_begin;

}  // namespace

span<const std::byte> BuildId() {
  // Read the sizes at the beginning of the note section.
  ElfNoteInfo build_id_note_sizes;
  std::memcpy(
      &build_id_note_sizes, &gnu_build_id_begin, sizeof(build_id_note_sizes));
  // Skip the "name" entry of the note section, and return a span to the
  // descriptor.
  const std::byte* descriptor_ptr =
      reinterpret_cast<const std::byte*>(&gnu_build_id_begin) +
      sizeof(build_id_note_sizes) + build_id_note_sizes.name_size;
  return span(descriptor_ptr, build_id_note_sizes.descriptor_size);
}

}  // namespace pw::build_info
