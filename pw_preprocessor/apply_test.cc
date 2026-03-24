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

#include "pw_preprocessor/apply.h"

#include "pw_unit_test/constexpr.h"
#include "pw_unit_test/framework.h"

namespace {

#define TEST_MACRO(index, forwarded_arg, arg) forwarded_arg #index ":" #arg
#define TEST_SEP(index, forwarded_arg) ";" forwarded_arg #index ";"

PW_CONSTEXPR_TEST(Apply, ZeroArgs, {
  // PW_APPLY with 0 args should expand to nothing.
  PW_TEST_EXPECT_STREQ("", "" PW_APPLY(TEST_MACRO, TEST_SEP, "prefix_"));
});

PW_CONSTEXPR_TEST(Apply, OneArg, {
  // PW_APPLY(MACRO, SEP, ARG, a1) -> MACRO(0, ARG, a1)
  PW_TEST_EXPECT_STREQ("prefix_0:a1",
                       PW_APPLY(TEST_MACRO, TEST_SEP, "prefix_", a1));
});

PW_CONSTEXPR_TEST(Apply, TwoArgs, {
  // PW_APPLY(MACRO, SEP, ARG, a1, a2) -> MACRO(0, ARG, a1) SEP(0, ARG) MACRO(1,
  // ARG, a2)
  PW_TEST_EXPECT_STREQ("prefix_0:a1;prefix_0;prefix_1:a2",
                       PW_APPLY(TEST_MACRO, TEST_SEP, "prefix_", a1, a2));
});

PW_CONSTEXPR_TEST(Apply, ThreeArgs, {
  PW_TEST_EXPECT_STREQ("prefix_0:a1;prefix_0;prefix_1:a2;prefix_1;prefix_2:a3",
                       PW_APPLY(TEST_MACRO, TEST_SEP, "prefix_", a1, a2, a3));
});

#define SIMPLE_SEP(index, forwarded_arg) ","
#define STRINGIFY(index, forwarded_arg, arg) #arg

PW_CONSTEXPR_TEST(Apply, FiveArgs, {
  PW_TEST_EXPECT_STREQ("a1,a2,a3,a4,a5",
                       PW_APPLY(STRINGIFY, SIMPLE_SEP, , a1, a2, a3, a4, a5));
});

PW_CONSTEXPR_TEST(Apply, 16Args, {
  PW_TEST_EXPECT_STREQ("a0,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15",
                       PW_APPLY(STRINGIFY,
                                SIMPLE_SEP,
                                ,
                                a0,
                                a1,
                                a2,
                                a3,
                                a4,
                                a5,
                                a6,
                                a7,
                                a8,
                                a9,
                                a10,
                                a11,
                                a12,
                                a13,
                                a14,
                                a15));
});

#define INDEX_ONLY_MACRO(index, forwarded_arg, arg) #index
#define COMMA_SEP(index, forwarded_arg) ","

PW_CONSTEXPR_TEST(Apply, Indices65Args, {
  // clang-format off
  PW_TEST_EXPECT_STREQ(
      "0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,"
      "27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,"
      "50,51,52,53,54,55,56,57,58,59,60,61,62,63,64",
      PW_APPLY(INDEX_ONLY_MACRO, COMMA_SEP, ,
                a0,  a1,  a2,  a3,  a4,  a5,  a6,  a7,
                a8,  a9, a10, a11, a12, a13, a14, a15,
               a16, a17, a18, a19, a20, a21, a22, a23,
               a24, a25, a26, a27, a28, a29, a30, a31,
               a32, a33, a34, a35, a36, a37, a38, a39,
               a40, a41, a42, a43, a44, a45, a46, a47,
               a48, a49, a50, a51, a52, a53, a54, a55,
               a56, a57, a58, a59, a60, a61, a62, a63,
               a64));
  // clang-format on
});

#define ARG_AND_INDEX(index, forwarded_arg, arg) #arg #index

PW_CONSTEXPR_TEST(Apply, 256Args, {
  // clang-format off
  PW_TEST_EXPECT_STREQ(
      "A0,B1,C2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42,a43,a44,a45,a46,a47,a48,a49,a50,a51,a52,a53,a54,a55,a56,a57,a58,a59,a60,a61,a62,a63,a64,a65,a66,a67,a68,a69,a70,a71,a72,a73,a74,a75,a76,a77,a78,a79,a80,a81,a82,a83,a84,a85,a86,a87,a88,a89,a90,a91,a92,a93,a94,a95,a96,a97,a98,a99,a100,a101,a102,a103,a104,a105,a106,a107,a108,a109,a110,a111,a112,a113,a114,a115,a116,a117,a118,a119,a120,a121,a122,a123,a124,a125,a126,a127,a128,a129,a130,a131,a132,a133,a134,a135,a136,a137,a138,a139,a140,a141,a142,a143,a144,a145,a146,a147,a148,a149,a150,a151,a152,a153,a154,a155,a156,a157,a158,a159,a160,a161,a162,a163,a164,a165,a166,a167,a168,a169,a170,a171,a172,a173,a174,a175,a176,a177,a178,a179,a180,a181,a182,a183,a184,a185,a186,a187,a188,a189,a190,a191,a192,a193,a194,a195,a196,a197,a198,a199,a200,a201,a202,a203,a204,a205,a206,a207,a208,a209,a210,a211,a212,a213,a214,a215,a216,a217,a218,a219,a220,a221,a222,a223,a224,a225,a226,a227,a228,a229,a230,a231,a232,a233,a234,a235,a236,a237,a238,a239,a240,a241,a242,a243,a244,a245,a246,a247,a248,a249,a250,a251,a252,X253,Y254,Z255",
      PW_APPLY(ARG_AND_INDEX, SIMPLE_SEP, ,
               A, B, C, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, X, Y, Z));
  // clang-format on
});

#define SEMICOLON(...) ;
#define TO_STRING_CASE(index, name, arg) \
  case arg:                              \
    return #arg

constexpr const char* ValueToStr(int value) {
  switch (value) {
    PW_APPLY(TO_STRING_CASE, SEMICOLON, , 100, 200, 300, 400, 500);
  }
  return "Unknown value";
}

PW_CONSTEXPR_TEST(Apply, SwitchValue, {
  constexpr const char* test = ValueToStr(300);
  PW_TEST_EXPECT_STREQ("300", test);
  PW_TEST_EXPECT_STREQ("500", ValueToStr(500));
});

PW_CONSTEXPR_TEST(Apply, UnknownSwitchValue, {
  constexpr const char* test = ValueToStr(600);
  PW_TEST_EXPECT_STREQ("Unknown value", test);
});

}  // namespace
