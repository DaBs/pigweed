# Copyright 2025 The Pigweed Authors
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.
"""Example of how to generate and run tests with pw_build.generated_tests."""

from pw_build import generated_tests

# Define test cases as a sequence of (input, expected_output) tuples.
# Use strings as titles for a group of test cases.
TEST_CASES = (
    'Simple strings',
    ('hello', 5),
    ('world', 5),
    'Empty and single-character',
    ('', 0),
    ('a', 1),
)


# Define a Python test generator function.
def _define_py_test(ctx: generated_tests.Context):
    input_str, expected = ctx.test_case
    # Return a function that will be used as a unittest.TestCase method.
    return lambda self: self.assertEqual(len(input_str), expected)


# Define a C++ test generator function.
def _define_cc_test(ctx: generated_tests.Context):
    input_str, expected = ctx.test_case
    # Yield C++ code for the test case.
    yield f'TEST(ReverseString, {ctx.cc_name()}) {{'
    yield f'  EXPECT_EQ(std::string_view("{input_str}").size(), {expected}u);'
    yield '}'


CC_HEADER = """\
#include <string_view>

#include "pw_unit_test/framework.h"

namespace {
"""

# Create a TestGenerator instance.
TESTS = generated_tests.TestGenerator(
    TEST_CASES,
    cc_test=(_define_cc_test, CC_HEADER, '}  // namespace'),
)

# Create the Python test class.
ReverseStringTest = TESTS.python_tests('ReverseStringTest', _define_py_test)

# Run the Python tests or generate other tests, depending on the command line
# arguments.
if __name__ == '__main__':
    generated_tests.main(TESTS)
