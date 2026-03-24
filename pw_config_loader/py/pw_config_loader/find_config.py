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
"""Helpers for finding config files."""

from collections import defaultdict
from collections.abc import Sequence
from pathlib import Path
from typing import Iterable, Iterator, List, Mapping


class ConflictingConfigsError(Exception):
    """An exception raised when multiple configs live in the same directory.

    This error is raised when a configuration file search has multiple candidate
    patterns (e.g. foo.toml or foo.yaml), and both candidates reside in the same
    directory.
    """


def configs_in_parents(
    config_file_name: str | Sequence[str], path: Path
) -> Iterator[Path]:
    """Finds all config files in a file's parent directories.

    Given the following file system:

    .. code-block:: py

       / (root)
         home/
           gregory/
             foo.conf
             .foo.yaml
             pigweed/
               foo.conf
               pw_cli/
                 baz.txt

    Calling this with ``config_file_name="foo.conf"`` and
    ``path=/home/gregory/pigweed/pw_cli/baz.txt``, the following config
    files will be yielded in order:

    - ``/home/gregory/pigweed/foo.conf``
    - ``/home/gregory/foo.conf``

    Multiple names may be provided to support alternative names, or multiple
    formats. In cases where multiple matches are found in the same directory,
    a ``ConflictingConfigsError`` is raised.

    Args:
        config_file_name: The basename of the config file of interest, or a
            sequence of basenames.
        path: The path to search for config files from.

    Yields:
        The paths to all config files that match the provided
        ``config_file_name`` patterns, ordered from nearest to ``path`` to
        farthest.
    """
    path = path.resolve()
    if path.is_file():
        path = path.parent

    choices: Sequence[str] = (
        [config_file_name]
        if isinstance(config_file_name, str)
        else config_file_name
    )

    while True:
        configs_found_in_dir = []
        for choice in choices:
            maybe_config = path / choice

            if maybe_config.is_file():
                configs_found_in_dir.append(maybe_config)

        if len(configs_found_in_dir) > 1:
            raise ConflictingConfigsError(
                'Found multiple potentially conflicting in the same '
                f'directory: {configs_found_in_dir}'
            )

        yield from configs_found_in_dir

        if str(path) == path.anchor:
            break
        path = path.parent


def paths_by_nearest_config(
    config_file_name: str | Sequence[str], paths: Iterable[Path]
) -> Mapping[Path | None, List[Path]]:
    """Groups a series of paths by their nearest config file.

    For each path in ``paths``, a lookup of the nearest matching config file
    is performed. Each identified config file is inserted as a key to the
    dictionary, and the value of each entry is a list containing every input
    file path that will use said config file. This is well suited for batching
    calls to tools that require a config file as a passed argument.

    Example:

    .. code-block:: py

       paths_by_config = paths_by_nearest_config(
           'settings.ini',
           paths,
       )
       for conf, grouped_paths:
           subprocess.run(
               (
                   'format_files',
                   '--config',
                   conf,
                   *grouped_paths,
               )
           )

    Args:
        config_file_name: The basename of the config file of interest, or a
            sequence of basenames.
        paths: The paths that should be mapped to their nearest config.

    Returns:
        A dictionary mapping the path of a config file to files that will
        pick up that config as their nearest config file.
    """
    # If this ends up being slow to do for many files, GitRepoFinder could
    # be generalized to support caching for this use case too.
    paths_by_config = defaultdict(list)
    for path in paths:
        config = next(configs_in_parents(config_file_name, path), None)
        paths_by_config[config].append(path)
    return paths_by_config
