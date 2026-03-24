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

#include "pw_tokenizer/detokenize_from_this_program.h"

#include "pw_tokenizer/detokenize.h"

namespace pw::tokenizer {

extern "C" {
// These symbols are provided by add_detokenize_from_this_program_sections.ld.
extern const std::byte __pw_tokenizer_entries_start[];
extern const std::byte __pw_tokenizer_entries_end[];
}

Result<Detokenizer> GetDetokenizerFromThisProgram() {
  return Detokenizer::FromElfSection({
      __pw_tokenizer_entries_start,
      __pw_tokenizer_entries_end,
  });
}

}  // namespace pw::tokenizer
