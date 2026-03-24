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

#include "pw_containers/internal/optional.h"

#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

#include "pw_containers/internal/test_helpers.h"
#include "pw_unit_test/constexpr.h"
#include "pw_unit_test/framework.h"

namespace {

using ::pw::containers::internal::Optional;
using ::pw::containers::test::Counter;
using ::pw::containers::test::MoveOnly;
using ::pw::containers::test::TrivialMoveOnly;

enum class HasValue : uint8_t {
  kNo = 0,
  kYes = 1,
  kDefinitelyNot = 3,
};

template <typename T>
using TestOpt = Optional<T, HasValue::kYes>;

// Helper to create empty/value optionals abstracting the differences
// between std::optional and pw::containers::internal::TestOpt.
template <typename OptT>
struct OptionalTraits;

template <typename T>
struct OptionalTraits<std::optional<T>> {
  static std::optional<T> MakeEmpty() { return std::nullopt; }
  static std::optional<T> MakeValue(T v) {
    return std::optional<T>(std::move(v));
  }
};

template <typename T>
struct OptionalTraits<TestOpt<T>> {
  static constexpr TestOpt<T> MakeEmpty() { return TestOpt<T>(HasValue::kNo); }
  static constexpr TestOpt<T> MakeValue(T v) {
    return TestOpt<T>(std::move(v));
  }
};

struct Copyable {
  constexpr Copyable(int v) : value(v) {}
  constexpr Copyable(const Copyable&) = default;
  constexpr Copyable(Copyable&&) = default;
  constexpr Copyable& operator=(const Copyable&) = default;
  constexpr Copyable& operator=(Copyable&&) = default;
  int value;
  constexpr bool operator==(const Copyable& other) const {
    return value == other.value;
  }
};

struct CopyCtorNotAssign {
  constexpr CopyCtorNotAssign(int v) : value(v) {}
  constexpr CopyCtorNotAssign(const CopyCtorNotAssign&) = default;
  constexpr CopyCtorNotAssign(CopyCtorNotAssign&&) = default;
  constexpr CopyCtorNotAssign& operator=(const CopyCtorNotAssign&) = delete;
  constexpr CopyCtorNotAssign& operator=(CopyCtorNotAssign&&) = delete;
  int value;
  constexpr bool operator==(const CopyCtorNotAssign& other) const {
    return value == other.value;
  }
};

struct MoveCtorNotAssign {
  constexpr MoveCtorNotAssign(int v) : value(v) {}
  constexpr MoveCtorNotAssign(const MoveCtorNotAssign&) = delete;
  constexpr MoveCtorNotAssign(MoveCtorNotAssign&&) = default;
  constexpr MoveCtorNotAssign& operator=(const MoveCtorNotAssign&) = delete;
  constexpr MoveCtorNotAssign& operator=(MoveCtorNotAssign&&) = delete;
  int value;
  constexpr bool operator==(const MoveCtorNotAssign& other) const {
    return value == other.value;
  }
};

struct CopyAssignNotCtor {
  constexpr CopyAssignNotCtor(int v) : value(v) {}
  constexpr CopyAssignNotCtor(const CopyAssignNotCtor&) = delete;
  constexpr CopyAssignNotCtor(CopyAssignNotCtor&&) = default;
  constexpr CopyAssignNotCtor& operator=(const CopyAssignNotCtor&) = default;
  constexpr CopyAssignNotCtor& operator=(CopyAssignNotCtor&&) = default;
  int value;
  constexpr bool operator==(const CopyAssignNotCtor& other) const {
    return value == other.value;
  }
};

struct MoveAssignNotCtor {
  constexpr MoveAssignNotCtor(int v) : value(v) {}
  constexpr MoveAssignNotCtor(const MoveAssignNotCtor&) = delete;
  constexpr MoveAssignNotCtor(MoveAssignNotCtor&&) = delete;
  constexpr MoveAssignNotCtor& operator=(const MoveAssignNotCtor&) = delete;
  constexpr MoveAssignNotCtor& operator=(MoveAssignNotCtor&&) = default;
  int value;
  constexpr bool operator==(const MoveAssignNotCtor& other) const {
    return value == other.value;
  }
};

struct ConvertibleFromInt {
  constexpr ConvertibleFromInt(int v) : value(v) {}
  int value;
  constexpr bool operator==(const ConvertibleFromInt& other) const {
    return value == other.value;
  }
};

struct ExplicitConstructibleFromInt {
  explicit constexpr ExplicitConstructibleFromInt(int v) : value(v) {}
  int value;
  constexpr bool operator==(const ExplicitConstructibleFromInt& other) const {
    return value == other.value;
  }
};

// Common tests for both std::optional and TestOpt.
template <typename OptT>
void TestCommonOptional() {
  using T = typename OptT::value_type;
  using Traits = OptionalTraits<OptT>;

  // Default/Empty construction
  OptT empty = Traits::MakeEmpty();
  EXPECT_FALSE(empty.has_value());

  // Value construction
  OptT val = Traits::MakeValue(T(42));
  EXPECT_TRUE(val.has_value());
  EXPECT_EQ(val.value().value, 42);
  EXPECT_EQ((*val).value, 42);
  EXPECT_EQ(val->value, 42);

  // Copy construction (if copyable)
  if constexpr (std::is_copy_constructible_v<T>) {
    OptT copy = val;
    EXPECT_TRUE(copy.has_value());
    EXPECT_EQ(copy->value, 42);
  }

  // Move construction
  OptT moved = std::move(val);
  EXPECT_TRUE(moved.has_value());
  EXPECT_EQ(moved->value, 42);

  // Emplace
  OptT emp = Traits::MakeEmpty();
  emp.emplace(100);
  EXPECT_TRUE(emp.has_value());
  EXPECT_EQ(emp->value, 100);
}

TEST(OptionalTest, Common_StdOptional_Copyable) {
  TestCommonOptional<std::optional<Copyable>>();
}

TEST(OptionalTest, Common_PwOptional_Copyable) {
  TestCommonOptional<TestOpt<Copyable>>();
}

TEST(OptionalTest, Common_StdOptional_MoveOnly) {
  TestCommonOptional<std::optional<MoveOnly>>();
}

TEST(OptionalTest, Common_PwOptional_MoveOnly) {
  TestCommonOptional<TestOpt<MoveOnly>>();
}

TEST(OptionalTest, Common_StdOptional_CopyCtorNotAssign) {
  TestCommonOptional<std::optional<CopyCtorNotAssign>>();
}

TEST(OptionalTest, Common_PwOptional_CopyCtorNotAssign) {
  TestCommonOptional<TestOpt<CopyCtorNotAssign>>();
}

TEST(OptionalTest, Common_StdOptional_MoveCtorNotAssign) {
  TestCommonOptional<std::optional<MoveCtorNotAssign>>();
}

TEST(OptionalTest, Common_PwOptional_MoveCtorNotAssign) {
  TestCommonOptional<TestOpt<MoveCtorNotAssign>>();
}

TEST(OptionalTest, Traits_CopyCtorNotAssign) {
  using Opt = TestOpt<CopyCtorNotAssign>;
  static_assert(std::is_copy_constructible_v<Opt>);
  static_assert(std::is_move_constructible_v<Opt>);
  static_assert(!std::is_copy_assignable_v<Opt>);
  static_assert(!std::is_move_assignable_v<Opt>);

  using StdOpt = std::optional<CopyCtorNotAssign>;
  static_assert(std::is_copy_constructible_v<StdOpt>);
  static_assert(std::is_move_constructible_v<StdOpt>);
  static_assert(!std::is_copy_assignable_v<StdOpt>);
  static_assert(!std::is_move_assignable_v<StdOpt>);
}

TEST(OptionalTest, Traits_MoveCtorNotAssign) {
  using Opt = TestOpt<MoveCtorNotAssign>;
  static_assert(!std::is_copy_constructible_v<Opt>);
  static_assert(std::is_move_constructible_v<Opt>);
  static_assert(!std::is_copy_assignable_v<Opt>);
  static_assert(!std::is_move_assignable_v<Opt>);

  using StdOpt = std::optional<MoveCtorNotAssign>;
  static_assert(!std::is_copy_constructible_v<StdOpt>);
  static_assert(std::is_move_constructible_v<StdOpt>);
  static_assert(!std::is_copy_assignable_v<StdOpt>);
  static_assert(!std::is_move_assignable_v<StdOpt>);
}

TEST(OptionalTest, Traits_CopyAssignNotCtor) {
  using Opt = TestOpt<CopyAssignNotCtor>;
  static_assert(!std::is_copy_constructible_v<Opt>);
  static_assert(std::is_move_constructible_v<Opt>);
  static_assert(!std::is_copy_assignable_v<Opt>);
  static_assert(std::is_move_assignable_v<Opt>);

  using StdOpt = std::optional<CopyAssignNotCtor>;
  static_assert(!std::is_copy_constructible_v<StdOpt>);
  static_assert(std::is_move_constructible_v<StdOpt>);
  static_assert(!std::is_copy_assignable_v<StdOpt>);
  static_assert(std::is_move_assignable_v<StdOpt>);
}

TEST(OptionalTest, Traits_MoveAssignNotCtor) {
  using Opt = TestOpt<MoveAssignNotCtor>;
  static_assert(!std::is_copy_constructible_v<Opt>);
  static_assert(!std::is_move_constructible_v<Opt>);
  static_assert(!std::is_copy_assignable_v<Opt>);
  static_assert(!std::is_move_assignable_v<Opt>);

  using StdOpt = std::optional<MoveAssignNotCtor>;
  static_assert(!std::is_copy_constructible_v<StdOpt>);
  static_assert(!std::is_move_constructible_v<StdOpt>);
  static_assert(!std::is_copy_assignable_v<StdOpt>);
  static_assert(!std::is_move_assignable_v<StdOpt>);
}

PW_CONSTEXPR_TEST(OptionalTest, ImplicitConversion, {
  TestOpt<ConvertibleFromInt> opt = ConvertibleFromInt(123);
  PW_TEST_EXPECT_TRUE(opt.has_value());
  PW_TEST_EXPECT_EQ(opt->value, 123);
});

PW_CONSTEXPR_TEST(OptionalTest, ExplicitConstruction, {
  TestOpt<ExplicitConstructibleFromInt> opt(std::in_place, 123);
  PW_TEST_EXPECT_TRUE(opt.has_value());
  PW_TEST_EXPECT_EQ(opt->value, 123);
});

TEST(OptionalTest, Reset) {
  TestOpt<Counter> opt(std::in_place, 5);
  EXPECT_TRUE(opt.has_value());
  opt.reset(HasValue::kDefinitelyNot);
  EXPECT_FALSE(opt.has_value());
}

TEST(OptionalTest, StateAccess) {
  TestOpt<Counter> opt(HasValue::kNo);
  EXPECT_EQ(opt.state(), HasValue::kNo);
  opt.reset(HasValue::kDefinitelyNot);
  EXPECT_EQ(opt.state(), HasValue::kDefinitelyNot);
  opt = Counter(3);
  EXPECT_EQ(opt.state(), HasValue::kYes);
}

PW_CONSTEXPR_TEST(OptionalTest, CopyAssignment, {
  TestOpt<Copyable> opt1 = Copyable(1);
  TestOpt<Copyable> opt2 = Copyable(2);
  opt1 = opt2;
  PW_TEST_EXPECT_EQ(opt1->value, 2);
});

PW_CONSTEXPR_TEST(OptionalTest, MoveAssignment, {
  TestOpt<TrivialMoveOnly> mv1(std::in_place, 1);
  TestOpt<TrivialMoveOnly> mv2(std::in_place, 2);
  mv1 = std::move(mv2);
  PW_TEST_EXPECT_EQ(mv1->value, 2);
});

TEST(OptionalTest, ValueCopyAssignment) {
  TestOpt<Copyable> opt(HasValue::kNo);
  Copyable val(123);
  opt = val;
  EXPECT_EQ(opt->value, 123);
}

TEST(OptionalTest, ValueMoveAssignment) {
  TestOpt<MoveOnly> mv(HasValue::kNo);
  mv = MoveOnly(456);
  EXPECT_EQ(mv->value, 456);
}

TEST(OptionalTest, Destructor_CalledOnReset) {
  Counter::Reset();

  TestOpt<Counter> opt(std::in_place);
  opt.reset(HasValue::kNo);
  EXPECT_EQ(Counter::created, 1);
  EXPECT_EQ(Counter::destroyed, 1);
}

TEST(OptionalTest, Destructor_CalledOnScopeExit) {
  Counter::Reset();
  {
    TestOpt<Counter> opt(std::in_place);
  }
  EXPECT_EQ(Counter::created, 1);
  EXPECT_EQ(Counter::destroyed, 1);
}

TEST(OptionalTest, Destructor_CalledOnAssignState) {
  Counter::Reset();
  {
    TestOpt<Counter> opt(std::in_place);
    opt = TestOpt<Counter>(HasValue::kNo);
  }
  EXPECT_EQ(Counter::created, 1);
  EXPECT_EQ(Counter::destroyed, 1);
}

PW_CONSTEXPR_TEST(OptionalTest, ConvertingConstructor_Implicit, {
  TestOpt<long> opt = 42;  // int to long
  PW_TEST_EXPECT_TRUE(opt.has_value());
  PW_TEST_EXPECT_EQ(*opt, 42);
});

TEST(OptionalTest, ConvertingConstructor_FromOptional) {
  TestOpt<int> source(42);
  TestOpt<long> dest = source;
  EXPECT_TRUE(dest.has_value());
  EXPECT_EQ(*dest, 42);
}

TEST(OptionalTest, ConvertingConstructor_FromMoveOptional) {
  TestOpt<int> source(42);
  TestOpt<long> dest = std::move(source);
  EXPECT_TRUE(dest.has_value());
  EXPECT_EQ(*dest, 42);
}

struct Base {
  virtual ~Base() = default;
  int x = 1;
};
struct Derived : Base {
  Derived() { x = 2; }
};

TEST(OptionalTest, ConvertingConstructor_BaseDerived) {
  TestOpt<Derived> derived(std::in_place);
  TestOpt<Base> base = derived;
  EXPECT_TRUE(base.has_value());
  EXPECT_EQ(base->x, 2);
}

PW_CONSTEXPR_TEST(OptionalTest, RefQualifiedAccessors, {
  class RefTester {
   public:
    constexpr int get() & { return 1; }
    constexpr int get() const& { return 2; }

    constexpr int get() && { return -1; }
    constexpr int get() const&& { return -2; }
  };

  TestOpt<RefTester> opt(std::in_place);

  // &
  PW_TEST_EXPECT_EQ(opt.value().get(), 1);
  PW_TEST_EXPECT_EQ((*opt).get(), 1);

  // const &
  const auto& c_opt = opt;
  PW_TEST_EXPECT_EQ(c_opt.value().get(), 2);
  PW_TEST_EXPECT_EQ((*c_opt).get(), 2);

  // &&
  PW_TEST_EXPECT_EQ(std::move(opt).value().get(), -1);
  PW_TEST_EXPECT_EQ((*TestOpt<RefTester>(std::in_place)).get(), -1);

  // const &&
  const auto& c_opt_ref1 = c_opt;
  PW_TEST_EXPECT_EQ(std::move(c_opt_ref1).value().get(), -2);
  const auto& c_opt_ref2 = c_opt;
  PW_TEST_EXPECT_EQ((*std::move(c_opt_ref2)).get(), -2);
});

PW_CONSTEXPR_TEST(OptionalTest, RefQualifiedAccessors_MoveOnly, {
  TestOpt<TrivialMoveOnly> opt(std::in_place, 123);

  // &&
  TrivialMoveOnly m = std::move(opt).value();
  PW_TEST_EXPECT_EQ(m.value, 123);

  // opt is now moved-from, state is valid but value is moved-from.
  // We can't easily check moved-from state of MoveOnly without adding a flag to
  // it, but we verified the value was moved out.
});

TEST(OptionalTest, PlacementNew_EmplaceArgs) {
  struct MultiArg {
    MultiArg(int a, int b) : sum(a + b) {}
    int sum;
  };
  TestOpt<MultiArg> opt(HasValue::kNo);
  opt.emplace(10, 20);
  EXPECT_TRUE(opt.has_value());
  EXPECT_EQ(opt->sum, 30);
}

PW_CONSTEXPR_TEST(OptionalTest, Void_DefaultConstruction, {
  TestOpt<void> opt(HasValue::kNo);
  PW_TEST_EXPECT_FALSE(opt.has_value());
  PW_TEST_EXPECT_EQ(opt.state(), HasValue::kNo);
});

PW_CONSTEXPR_TEST(OptionalTest, Void_InPlaceConstruction, {
  TestOpt<void> opt(std::in_place);
  PW_TEST_EXPECT_TRUE(opt.has_value());
  PW_TEST_EXPECT_EQ(opt.state(), HasValue::kYes);
});

PW_CONSTEXPR_TEST(OptionalTest, Void_Emplace, {
  TestOpt<void> opt(HasValue::kNo);
  opt.emplace();
  PW_TEST_EXPECT_TRUE(opt.has_value());
});

PW_CONSTEXPR_TEST(OptionalTest, Void_Reset, {
  TestOpt<void> opt(std::in_place);
  opt.reset(HasValue::kNo);
  PW_TEST_EXPECT_FALSE(opt.has_value());
});

PW_CONSTEXPR_TEST(OptionalTest, Void_Value, {
  TestOpt<void> opt(std::in_place);
  opt.value();  // Should not crash
});

PW_CONSTEXPR_TEST(OptionalTest, Void_CopyAssignment, {
  TestOpt<void> opt1(std::in_place);
  TestOpt<void> opt2(HasValue::kNo);
  opt2 = opt1;
  PW_TEST_EXPECT_TRUE(opt2.has_value());
});

PW_CONSTEXPR_TEST(OptionalTest, Void_MoveAssignment, {
  TestOpt<void> opt1(std::in_place);
  TestOpt<void> opt2(HasValue::kNo);
  opt2 = std::move(opt1);
  PW_TEST_EXPECT_TRUE(opt2.has_value());
});

PW_CONSTEXPR_TEST(OptionalTest, Equality_OptionalOptionalVoid, {
  PW_TEST_EXPECT_EQ(TestOpt<void>(HasValue::kNo), TestOpt<void>(HasValue::kNo));
  PW_TEST_EXPECT_NE(TestOpt<void>(HasValue::kNo),
                    TestOpt<void>(HasValue::kDefinitelyNot));
  PW_TEST_EXPECT_EQ(TestOpt<void>(std::in_place), TestOpt<void>(std::in_place));
  PW_TEST_EXPECT_NE(TestOpt<void>(std::in_place), TestOpt<void>(HasValue::kNo));
});

PW_CONSTEXPR_TEST(OptionalTest, Equality_OptionalOptional, {
  PW_TEST_EXPECT_EQ(TestOpt<int>(HasValue::kNo), TestOpt<int>(HasValue::kNo));
  PW_TEST_EXPECT_NE(TestOpt<int>(HasValue::kNo),
                    TestOpt<int>(HasValue::kDefinitelyNot));
  PW_TEST_EXPECT_NE(TestOpt<int>(0), TestOpt<int>(HasValue::kNo));
  PW_TEST_EXPECT_NE(TestOpt<int>(HasValue::kNo), TestOpt<int>(0));

  PW_TEST_EXPECT_EQ(TestOpt<int>(1), TestOpt<int>(1));
  PW_TEST_EXPECT_NE(TestOpt<int>(2), TestOpt<int>(1));
});

PW_CONSTEXPR_TEST(OptionalTest, Equality_OptionalValue, {
  PW_TEST_EXPECT_NE(TestOpt<int>(HasValue::kNo), TestOpt<int>(1));
  PW_TEST_EXPECT_NE(TestOpt<int>(1), TestOpt<int>(HasValue::kNo));

  PW_TEST_EXPECT_EQ(TestOpt<int>(1), 1);
  PW_TEST_EXPECT_NE(TestOpt<int>(1), 2);

  PW_TEST_EXPECT_EQ(2, TestOpt<int>(2));
  PW_TEST_EXPECT_NE(2, TestOpt<int>(1));
});

TEST(OptionalTest, HasValueIsDefault) {
  enum class State { kHas, kHasNot };

  using Opt = Optional<int, State::kHas>;
  Opt present(5);
  PW_TEST_EXPECT_TRUE(present.has_value());
  PW_TEST_EXPECT_EQ(present.state(), State::kHas);
  present = Opt(State::kHasNot);
  PW_TEST_ASSERT_FALSE(present.has_value());

  Opt not_present(State::kHasNot);
  PW_TEST_ASSERT_FALSE(not_present.has_value());
}

}  // namespace
