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
"""Tests for the reST formatter."""

import importlib.resources
import os
from pathlib import Path
import tempfile
import unittest

from format_testing_utils import CapturingToolRunner
from pw_build.runfiles_manager import RunfilesManager
from pw_presubmit.format.rst import RstFormatter

# Setup paths to test data
_TEST_DATA_DIR = importlib.resources.files('pw_presubmit.format.test_data')
_RST_TEST_FILE = _TEST_DATA_DIR / 'rst_test_data.rst'
_RST_GOLDEN_FILE = _TEST_DATA_DIR / 'rst_test_data_golden.rst'


FORMATTED_CPP = """\
.. code-block:: cpp

   // clang-format off
   constexpr int kMyMatrix[] = {
       100,  23,   0,
         0, 542,  38,
         1,   2, 201,
   };
   // clang-format on

   int SomeFunction(int x) {
     PW_ASSERT(x < 9);
     return kMyMatrix[x];
   }
"""


class RstFormatterTest(unittest.TestCase):
    """Tests for the RstFormatter."""

    maxDiff = None

    def setUp(self):
        self.runfiles = RunfilesManager()
        self.runfiles.add_bazel_tool(
            'clang-format',
            'llvm_toolchain.clang_format',
            exclusive=True,
        )
        self.runfiles.add_bootstrapped_tool(
            'clang-format',
            'clang-format',
            from_shell_path=True,
        )
        self.tool_runner = CapturingToolRunner(
            {'clang-format': self.runfiles['clang-format']}
        )
        self.tempdir = tempfile.TemporaryDirectory()
        self.test_file = Path(self.tempdir.name) / 'test.rst'

    def tearDown(self):
        self.tempdir.cleanup()

    def _run_formatter(self, content: str) -> str:
        formatter = RstFormatter(tool_runner=self.tool_runner)
        self.test_file.write_text(content)
        formatter.format_file(self.test_file)
        return self.test_file.read_text()

    def test_golden_file(self):
        """Test formatting a file against a golden file."""
        # TODO: https://pwbug.dev/434683529 - Fix CRLF-related discrepancies.
        if os.name == 'nt':
            return

        formatter = RstFormatter(tool_runner=self.tool_runner)
        result = formatter.format_file_in_memory(
            _RST_TEST_FILE, _RST_TEST_FILE.read_bytes()
        )
        self.assertTrue(result.ok)
        self.assertIsNone(result.error_message)
        self.assertMultiLineEqual(
            result.formatted_file_contents.decode(),
            _RST_GOLDEN_FILE.read_text(),
        )

    def test_simple_formatting(self):
        original = 'Hello world\t  \n'
        expected = 'Hello world\n'
        self.assertEqual(expected, self._run_formatter(original))

    def test_code_directive_replacement(self):
        original = '.. code:: cpp\n'
        expected = '.. code-block:: cpp\n'
        self.assertEqual(expected, self._run_formatter(original))

    def test_code_block_reindent(self):
        original = '\n'.join(
            (
                '.. code-block:: cpp',
                '  int main() {',
                '    int x = 0;',
                '    return x;',
                '  }',
            )
        )
        expected = '\n'.join(
            (
                '.. code-block:: cpp',
                '',
                '   int main() {',
                '     int x = 0;',
                '     return x;',
                '   }',
                '',
            )
        )
        self.assertEqual(expected, self._run_formatter(original))

    def test_code_block_reformat_cpp(self):
        self.assertEqual(FORMATTED_CPP, self._run_formatter(FORMATTED_CPP))

    def test_code_block_reformat_cpp_repeatedly(self):
        original = '\n'.join(
            (
                '.. code-block:: cpp',
                '',
                '   int main() {',
                '     if (true) {',
                '       return 0;',
                '     }',
                '',
                '     return 1;',
                '   }',
                '',
            )
        )
        self.assertEqual(original, self._run_formatter(original))

    def test_code_block_with_options(self):
        original = '\n'.join(
            (
                '.. code-block:: cpp',
                '   :caption: My Caption',
                '',
                '  int main() {',
                '    return 0;',
                '  }',
                '',
            )
        )
        expected = '\n'.join(
            (
                '.. code-block:: cpp',
                '   :caption: My Caption',
                '',
                '   int main() { return 0; }',
                '',
            )
        )
        self.assertEqual(expected, self._run_formatter(original))

    def test_go_code_block_keeps_tabs(self):
        original = '\n'.join(
            (
                '.. code-block:: go',
                '',
                'func main() {',
                '\tfmt.Println(\'Hello\')',
                '}',
                '',
            )
        )
        expected = '\n'.join(
            (
                '.. code-block:: go',
                '',
                '   func main() {',
                '   \tfmt.Println(\'Hello\')',
                '   }',
                '',
            )
        )
        self.assertEqual(expected, self._run_formatter(original))

    def test_none_code_block_keeps_tabs(self):
        original = '\n'.join(
            (
                '.. code-block:: none',
                '',
                'some',
                '\ttext',
                '',
            )
        )
        expected = '\n'.join(
            (
                '.. code-block:: none',
                '',
                '   some',
                '   \ttext',
                '',
            )
        )
        self.assertEqual(expected, self._run_formatter(original))

    def test_trailing_newlines_removed(self):
        original = 'Hello\n\n\n'
        expected = 'Hello\n'
        self.assertEqual(expected, self._run_formatter(original))

    def test_trailing_newline_added(self):
        original = 'Hello'
        expected = 'Hello\n'
        self.assertEqual(expected, self._run_formatter(original))


if __name__ == '__main__':
    unittest.main()
