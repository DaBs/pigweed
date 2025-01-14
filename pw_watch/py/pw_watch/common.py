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
"""Utilities for managing and filtering paths to watch with pw_watch."""

import logging
import os
from pathlib import Path
import subprocess
from typing import Callable, Iterable, NoReturn

import pw_cli.color

from watchdog.events import FileSystemEventHandler
from watchdog.observers import Observer

_LOG = logging.getLogger('pw_watch')
_COLOR = pw_cli.color.colors()

# Suppress events under 'fsevents', generated by watchdog on every file
# event on MacOS.
# TODO: b/182281481 - Fix file ignoring, rather than just suppressing logs
logging.getLogger('fsevents').setLevel(logging.WARNING)


ERRNO_INOTIFY_LIMIT_REACHED = 28
WATCH_PATTERNS = (
    '*.bazel',
    '*.bzl',
    '*.bloaty',
    '*.c',
    '*.cc',
    '*.css',
    '*.cpp',
    '*.cmake',
    'CMakeLists.txt',
    '*.dts',
    '*.dtsi',
    '*.emb',
    '*.gn',
    '*.gni',
    '*.go',
    '*.h',
    '*.hpp',
    '*.html',
    '*.java',
    '*.js',
    '*.ld',
    '*.md',
    '*.options',
    '*.proto',
    '*.py',
    '*.rs',
    '*.rst',
    '*.s',
    '*.S',
    '*.toml',
    '*.ts',
)


def git_ignored(file: Path) -> bool:
    """Returns true if this file is in a Git repo and ignored by that repo.

    Returns true for ignored files that were manually added to a repo.
    """
    file = file.resolve()
    directory = file.parent

    # Run the Git command from file's parent so that the correct repo is used.
    while True:
        try:
            returncode = subprocess.run(
                ['git', 'check-ignore', '--quiet', '--no-index', file],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                cwd=directory,
            ).returncode
            return returncode in (0, 128)
        except FileNotFoundError:
            # If the directory no longer exists, try parent directories until
            # an existing directory is found or all directories have been
            # checked. This approach makes it possible to check if a deleted
            # path is ignored in the repo it was originally created in.
            if directory == directory.parent:
                return False

            directory = directory.parent


def get_common_excludes(root: Path) -> Iterable[Path]:
    """Find commonly excluded directories, and return them as a [Path]"""
    exclude_list: list[Path] = []

    typical_ignored_directories: list[str] = [
        '.environment',  # Legacy bootstrap-created CIPD and Python venv.
        '.presubmit',  # Presubmit-created CIPD and Python venv.
        '.git',  # Pigweed's git repo.
        '.mypy_cache',  # Python static analyzer.
        '.cargo',  # Rust package manager.
        'environment',  # Bootstrap-created CIPD and Python venv.
        'out',  # Typical build directory.
    ]

    # Preset exclude for common project structures.
    exclude_list.extend(
        root / ignored_directory
        for ignored_directory in typical_ignored_directories
    )

    # Ignore bazel-* directories
    exclude_list.extend(
        d for d in root.glob('bazel-*') if d.is_dir() and d.is_symlink()
    )

    # Check for and warn about legacy directories.
    legacy_directories = [
        '.cipd',  # Legacy CIPD location.
        '.python3-venv',  # Legacy Python venv location.
    ]
    found_legacy = False
    for legacy_directory in legacy_directories:
        full_legacy_directory = root / legacy_directory
        if full_legacy_directory.is_dir():
            _LOG.warning(
                'Legacy environment directory found: %s',
                str(full_legacy_directory),
            )
            exclude_list.append(full_legacy_directory)
            found_legacy = True
    if found_legacy:
        _LOG.warning(
            'Found legacy environment directory(s); these ' 'should be deleted'
        )

    return exclude_list


_FILESYSTEM_EVENTS_THAT_TRIGGER_BUILDS = (
    'created',
    'modified',
    'deleted',
    'moved',
)


def handle_watchdog_event(
    event, watch_patterns: Iterable[str], ignore_patterns: Iterable[str]
) -> Path | None:
    """Returns the path if the event is significant, otherwise None."""

    def path_matches(path: Path) -> bool:
        return not any(path.match(x) for x in ignore_patterns) and any(
            path.match(x) for x in watch_patterns
        )

    # There isn't any point in triggering builds on new directory creation.
    # It's the creation or modification of files that indicate something
    # meaningful enough changed for a build.
    if event.is_directory:
        return None

    if event.event_type not in _FILESYSTEM_EVENTS_THAT_TRIGGER_BUILDS:
        return None

    # Collect paths of interest from the event.
    paths: list[str] = []
    if hasattr(event, 'dest_path'):
        paths.append(os.fsdecode(event.dest_path))
    if event.src_path:
        paths.append(os.fsdecode(event.src_path))

    # Check whether Git cares about any of these paths.
    for path in (Path(p).resolve() for p in paths):
        if not git_ignored(path) and path_matches(path):
            return path

    return None


# Go over each directory inside of the current directory.
# If it is not on the path of elements in directories_to_exclude, add
# (directory, True) to subdirectories_to_watch and later recursively call
# Observer() on them.
# Otherwise add (directory, False) to subdirectories_to_watch and later call
# Observer() with recursion=False.
def minimal_watch_directories(to_watch: Path, to_exclude: Iterable[Path]):
    """Determine which subdirectory to watch recursively"""
    try:
        to_watch = Path(to_watch)
    except TypeError:
        assert False, "Please watch one directory at a time."

    # Reformat to_exclude.
    directories_to_exclude: list[Path] = [
        to_watch.joinpath(directory_to_exclude)
        for directory_to_exclude in to_exclude
        if to_watch.joinpath(directory_to_exclude).is_dir()
    ]

    # Split the relative path of directories_to_exclude (compared to to_watch),
    # and generate all parent paths needed to be watched without recursion.
    exclude_dir_parents = {to_watch}
    for directory_to_exclude in directories_to_exclude:
        # Irrelevant excluded path
        if not Path(directory_to_exclude).is_relative_to(to_watch):
            continue

        parts = list(Path(directory_to_exclude).relative_to(to_watch).parts)[
            :-1
        ]
        dir_tmp = to_watch
        for part in parts:
            dir_tmp = Path(dir_tmp, part)
            exclude_dir_parents.add(dir_tmp)

    # Go over all layers of directory. Append those that are the parents of
    # directories_to_exclude to the list with recursion==False, and others
    # with recursion==True.
    for directory in exclude_dir_parents:
        dir_path = Path(directory)
        yield dir_path, False
        for item in Path(directory).iterdir():
            if (
                item.is_dir()
                and item not in exclude_dir_parents
                and item not in directories_to_exclude
            ):
                yield item, True


def watch(
    watch_path: Path,
    exclude_list: Iterable[Path],
    event_handler: FileSystemEventHandler,
) -> Callable[[], None]:
    """Attaches the filesystem watcher for the specified paths.

    Returns:
      A function that, when called, blocks the thread until an internal watcher
      error occurs.
    """
    # It can take awhile to configure the filesystem watcher, so have the
    # message reflect that with the "...". Run inside the try: to
    # gracefully handle the user Ctrl-C'ing out during startup.

    # Try to make a short display path for the watched directory that has
    # "$HOME" instead of the full home directory. This is nice for users
    # who have deeply nested home directories.
    path_to_log = str(watch_path.resolve()).replace(str(Path.home()), '$HOME')
    _LOG.info('Attaching filesystem watcher to %s/...', path_to_log)

    # Observe changes for all files in the root directory. Whether the
    # directory should be observed recursively or not is determined by the
    # second element in subdirectories_to_watch.
    observers = []
    for path, rec in minimal_watch_directories(watch_path, exclude_list):
        observer = Observer()
        observer.schedule(
            event_handler,
            str(path),
            recursive=rec,
        )
        observer.start()
        observers.append(observer)

    def wait_function() -> None:
        for observer in observers:
            while observer.is_alive():
                observer.join(1)
        _LOG.error('Observers joined unexpectedly')

    return wait_function


def log_inotify_watch_limit_reached() -> None:
    """Log that the inotify watch limit was reached.

    Show information and suggested commands in OSError: inotify limit reached.
    """
    _LOG.error(
        'Inotify watch limit reached: run this in your terminal if '
        'you are in Linux to temporarily increase inotify limit.'
    )
    _LOG.info('')
    _LOG.info(
        _COLOR.green(
            '        sudo sysctl fs.inotify.max_user_watches=' '$NEW_LIMIT$'
        )
    )
    _LOG.info('')
    _LOG.info(
        '  Change $NEW_LIMIT$ with an integer number, '
        'e.g., 20000 should be enough.'
    )


def log_inotify_instance_limit_reached() -> None:
    """Log that the inotify instance limit was reached.

    Show information and suggested commands in OSError: inotify limit reached.
    """
    _LOG.error(
        'Inotify instance limit reached: run this in your terminal if '
        'you are in Linux to temporarily increase inotify limit.'
    )
    _LOG.info('')
    _LOG.info(
        _COLOR.green(
            '        sudo sysctl fs.inotify.max_user_instances=' '$NEW_LIMIT$'
        )
    )
    _LOG.info('')
    _LOG.info(
        '  Change $NEW_LIMIT$ with an integer number, '
        'e.g., 20000 should be enough.'
    )


def exit_immediately(code: int) -> NoReturn:
    """Exits quickly without waiting for threads to finish."""
    # Flush all log handlers
    logging.shutdown()
    # Note: The "proper" way to exit is via observer.stop(), then
    # running a join. However it's slower, so just exit immediately.
    #
    # Additionally, since there are several threads in the watcher, the usual
    # sys.exit approach doesn't work. Instead, run the low level exit which
    # kills all threads.
    os._exit(code)  # pylint: disable=protected-access
