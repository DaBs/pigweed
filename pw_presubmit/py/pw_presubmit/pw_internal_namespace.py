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
"""Check for use of Pigweed-internal stuff outside of Pigweed."""

from pathlib import Path
import re

import pw_cli.env

from pw_presubmit import presubmit

from . import presubmit_context

_PW_INTERNAL = re.compile(
    r'\bpw\s*::\s*(?:[\w_]+\s*::\s*)*internal\b(?:\s*::\s*[\w_]+)*'
)


def check_file(ctx: presubmit_context.PresubmitContext, path: Path) -> None:
    """Check one file for non-inclusive language.

    Args:
        ctx: Presubmit context.
        path: File to check.
    """
    if path.is_symlink() or path.is_dir():
        return

    try:
        contents = path.read_text()
    except UnicodeDecodeError:
        # File is not text, like a gif.
        return

    if any(part.startswith('pw_') for part in path.relative_to(ctx.root).parts):
        # This is in a directory that looks like a Pigweed module. Even if we're
        # not in Pigweed itself, this is allowed, because this code looks like
        # it's planned to be upstreamed to Pigweed.
        return

    match = _PW_INTERNAL.search(contents)
    if not match:
        return

    prefix = contents[: match.start()]
    line_num = prefix.count('\n') + 1
    ctx.fail(
        'Found internal-only Pigweed reference outside of Pigweed: '
        f'{match.group(0)}',
        path=path,
        line=line_num,
    )


@presubmit.Check
def pw_internal_namespace(ctx: presubmit_context.PresubmitContext) -> None:
    """Presubmit check for Pigweed-internal references."""

    # No subprocesses are run for pw_internal_namespace so don't perform this
    # check if dry_run is on.
    if ctx.dry_run:
        return

    # Don't run this check on Pigweed itself, only on downstream projects using
    # Pigweed.
    pw_root = pw_cli.env.pigweed_environment().PW_ROOT
    pw_project_root = pw_cli.env.pigweed_environment().PW_PROJECT_ROOT
    if pw_root == pw_project_root:
        return

    ctx.paths = presubmit_context.apply_exclusions(ctx)

    for path in ctx.paths:
        check_file(ctx, path)
