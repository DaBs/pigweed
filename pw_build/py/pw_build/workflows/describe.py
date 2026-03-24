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
"""Utilities for dumping portions of a workflows config file."""

import argparse
from collections.abc import Sequence
import logging

from google.protobuf import message, text_format
from pw_build.proto import workflows_pb2
from pw_build.workflows.manager import WorkflowsManager

_LOG = logging.getLogger(__name__)


class Describe:
    """Utilities for dumping portions of a workflows config file."""

    def __init__(
        self,
        config: workflows_pb2.WorkflowSuite | None,
        workflows: WorkflowsManager | None,
    ):
        super().__init__()
        self.config: workflows_pb2.WorkflowSuite | None = config
        self._workflows: WorkflowsManager | None = workflows

    def dump(self, plugin_args: Sequence[str]) -> int:
        print(self.describe(plugin_args=plugin_args))
        return 0

    def describe(self, plugin_args: Sequence[str]) -> str:
        """Extract some or part of a workflows config file."""
        parser = argparse.ArgumentParser(
            prog='./pw describe',
            description=(
                'Describes subsets or expanded views of the current '
                'workflows configuration.'
            ),
        )

        parser.add_argument(
            'name',
            nargs='*',
            default=None,
            help=(
                'The name of a build, tool, group, or build configuration to '
                'inspect. By default, this will emit the requested items as '
                'TextProto.'
            ),
        )

        parser.add_argument(
            '--infra-metadata',
            action='store_true',
            help=(
                'Emits all build driver requests produced by the requested '
                'items as a TextProto BuildDriverRequest message.'
            ),
        )

        args = parser.parse_args(plugin_args)

        if self.config is None:
            return 'Config is empty\n'

        if not args.name:
            return self.dump_config() + '\n'

        if args.infra_metadata:
            return self.dump_build_request(args.name)

        result = []
        for name in args.name:
            result.append(self.dump_fragment(name))
        return '\n---\n'.join(f'{x.strip()}' for x in result)

    def dump_config(self) -> str:
        """Dumps the entire config in a human-readable format."""
        if self.config is None:
            return ''

        return text_format.MessageToBytes(self.config).decode()

    def dump_build_request(self, names: Sequence[str]) -> str:
        """Dumps the unified build driver request for this config fragment."""
        assert self._workflows is not None
        return text_format.MessageToBytes(
            self._workflows.get_unified_driver_request(names, sanitize=False)
        ).decode()

    def dump_fragment(self, fragment_name: str) -> str:
        """Dumps a fragment of the config in a human-readable format."""
        if not fragment_name:
            raise ValueError('Invalid empty fragment name')
        dump: message.Message | None = None

        if self.config is not None:
            for conf in self.config.configs:
                if conf.name == fragment_name:
                    dump = conf
                    break
            for tool in self.config.tools:
                if tool.name == fragment_name:
                    dump = tool
                    break
                if tool.build_config.name == fragment_name:
                    dump = tool.build_config
                    break
            for build in self.config.builds:
                if build.name == fragment_name:
                    dump = build
                    break
            for group in self.config.groups:
                if group.name == fragment_name:
                    dump = group
                    break

        if dump is None:
            raise ValueError(
                f'Could not find any config fragment named `{fragment_name}`'
            )

        return text_format.MessageToBytes(dump).decode()
