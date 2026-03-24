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

#include "pw_bytes/span.h"
#include "pw_tokenizer/tokenize.h"
#include "pw_unit_test/framework.h"

namespace pw::tokenizer {
namespace {

TEST(DetokenizeFromThisProgram, DetokenizesSuccessfully) {
  uint32_t token1 = PW_TOKENIZE_STRING("Token one");
  uint32_t token2 = PW_TOKENIZE_STRING("Token two");

  pw::Result<Detokenizer> detok = GetDetokenizerFromThisProgram();
  PW_TEST_ASSERT_OK(detok);

  EXPECT_EQ(detok->Detokenize(ObjectAsBytes(token1)).BestString(), "Token one");
  EXPECT_EQ(detok->Detokenize(ObjectAsBytes(token2)).BestString(), "Token two");
}

}  // namespace
}  // namespace pw::tokenizer
