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
"""List of all default Pigweed Formatters."""
import importlib.resources

from dataclasses import dataclass
from pw_build.runfiles_manager import RunfilesManager

from pw_presubmit.format.core import FileFormatter
from pw_presubmit.format.bazel import BuildifierFormatter
from pw_presubmit.format.cpp import ClangFormatFormatter
from pw_presubmit.format.gn import GnFormatter
from pw_presubmit.format.go import GofmtFormatter
from pw_presubmit.format.java import JavaFormatter
from pw_presubmit.format.json import JsonFormatter
from pw_presubmit.format.owners import OwnersFormatter
from pw_presubmit.format.protobuf import ProtobufFormatter
from pw_presubmit.format.javascript import JavaScriptFormatter
from pw_presubmit.format.python import BlackFormatter
from pw_presubmit.format.rst import RstFormatter
from pw_presubmit.format.rust import RustfmtFormatter
from pw_presubmit.format.starlark import StarlarkFormatter
from pw_presubmit.format.typescript import TypeScriptFormatter
from pw_presubmit.format.cmake import CmakeFormatter
from pw_presubmit.format.css import CssFormatter
from pw_presubmit.format.markdown import MarkdownFormatter

_PACKAGE_DATA_DIR = importlib.resources.files('pigweed_format')
_DISABLED_FORMATTERS = _PACKAGE_DATA_DIR / 'disabled_formatters.txt'


@dataclass
class _FormatterSetup:
    """Setup for a formatter.

    Attributes:
    formatter: The core formatter object responsible for the formatting
        logic.
    binary: Name of the format tool binary.
    bazel_import_path: Import path of the ``pw_py_importable_runfile``
        rule that provides the desired file.
    """

    formatter: FileFormatter
    binary: None | str
    bazel_import_path: None | str


def _default_pigweed_formatters(
    runfiles: RunfilesManager,
) -> list[_FormatterSetup]:
    """List of default pigweed formatters.

    Args:
        runfiles: Runtime file resource helper to simpilify running
            binaries in bootstrap and Bazel build systems.

    Returns:
        List of default pigweed formatters represented via
        _FormatterSetup objects.
    """
    return [
        _FormatterSetup(
            formatter=BlackFormatter(
                tool_runner=runfiles,
            ),
            binary='black',
            bazel_import_path='pw_presubmit.py.black_runfiles',
        ),
        _FormatterSetup(
            formatter=BuildifierFormatter(
                tool_runner=runfiles,
            ),
            binary='buildifier',
            bazel_import_path='pw_presubmit.py.buildifier_runfiles',
        ),
        _FormatterSetup(
            formatter=ClangFormatFormatter(
                tool_runner=runfiles,
            ),
            binary='clang-format',
            bazel_import_path='llvm_toolchain.clang_format',
        ),
        _FormatterSetup(
            formatter=CmakeFormatter(
                tool_runner=runfiles,
            ),
            binary=None,
            bazel_import_path=None,
        ),
        _FormatterSetup(
            formatter=CssFormatter(
                tool_runner=runfiles,
            ),
            binary='prettier',
            bazel_import_path='pw_presubmit.py.prettier_runfiles',
        ),
        _FormatterSetup(
            formatter=GnFormatter(
                tool_runner=runfiles,
            ),
            binary='gn',
            bazel_import_path='pw_presubmit.py.gn_runfiles',
        ),
        _FormatterSetup(
            formatter=GofmtFormatter(
                tool_runner=runfiles,
            ),
            binary='gofmt',
            bazel_import_path='pw_presubmit.py.gofmt_runfiles',
        ),
        _FormatterSetup(
            formatter=JavaScriptFormatter(
                tool_runner=runfiles,
            ),
            binary='prettier',
            bazel_import_path='pw_presubmit.py.prettier_runfiles',
        ),
        _FormatterSetup(
            formatter=JavaFormatter(
                tool_runner=runfiles,
            ),
            binary='clang-format',
            bazel_import_path='llvm_toolchain.clang_format',
        ),
        _FormatterSetup(
            formatter=JsonFormatter(
                tool_runner=runfiles,
            ),
            binary=None,
            bazel_import_path=None,
        ),
        _FormatterSetup(
            formatter=MarkdownFormatter(
                tool_runner=runfiles,
            ),
            binary=None,
            bazel_import_path=None,
        ),
        _FormatterSetup(
            formatter=OwnersFormatter(
                tool_runner=runfiles,
            ),
            binary=None,
            bazel_import_path=None,
        ),
        _FormatterSetup(
            formatter=ProtobufFormatter(
                tool_runner=runfiles,
            ),
            binary='clang-format',
            bazel_import_path='llvm_toolchain.clang_format',
        ),
        _FormatterSetup(
            formatter=RstFormatter(
                tool_runner=runfiles,
            ),
            binary=None,
            bazel_import_path=None,
        ),
        _FormatterSetup(
            formatter=RustfmtFormatter(
                tool_runner=runfiles,
            ),
            binary='rustfmt',
            bazel_import_path='pw_presubmit.py.rustfmt_runfiles',
        ),
        _FormatterSetup(
            formatter=StarlarkFormatter(
                tool_runner=runfiles,
            ),
            binary='buildifier',
            bazel_import_path='pw_presubmit.py.buildifier_runfiles',
        ),
        _FormatterSetup(
            formatter=TypeScriptFormatter(
                tool_runner=runfiles,
            ),
            binary='prettier',
            bazel_import_path='pw_presubmit.py.prettier_runfiles',
        ),
    ]


def pigweed_formatters(runfiles: RunfilesManager) -> list[FileFormatter]:
    """Initializes pigweed formatters.

    Takes the default pigweed formatters, filter out the disabled ones,
    if any, and also sets up their runfiles locations.

    Args:
        runfiles: Runtime file resource helper to simpilify running
            binaries in bootstrap and Bazel build systems.

    Returns:
        List of enabled FileFormatters.
    """
    if _DISABLED_FORMATTERS.is_file():
        disabled_formatters = set(_DISABLED_FORMATTERS.read_text().splitlines())
    else:
        disabled_formatters = set()

    # If JavaScript is disabled, also disable CSS since they share a tool.
    if 'JavaScript' in disabled_formatters:
        disabled_formatters.add('CSS')

    all_formatters = _default_pigweed_formatters(runfiles)

    enabled_formatters = []
    for fmt in all_formatters:
        if fmt.formatter.mnemonic not in disabled_formatters:
            enabled_formatters.append(fmt)
        else:
            disabled_formatters.remove(fmt.formatter.mnemonic)

    assert (
        not disabled_formatters
    ), f'Attempted to disable unknown formatters: {disabled_formatters}'

    # Setup runfiles
    for formatter in enabled_formatters:
        if formatter.binary is not None:
            if formatter.binary in runfiles:
                continue
            runfiles.add_bootstrapped_tool(
                formatter.binary, formatter.binary, from_shell_path=True
            )
            if formatter.bazel_import_path is not None:
                runfiles.add_bazel_tool(
                    formatter.binary, formatter.bazel_import_path
                )

    return [fmt.formatter for fmt in enabled_formatters]
