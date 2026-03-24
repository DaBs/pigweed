# Copyright 2024 The Pigweed Authors
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

"""Bazel transitions for the host device simulator."""

load("@rules_platform//platform_data:defs.bzl", "platform_data")

def _host_device_simulator_binary_impl(name, binary, **kwargs):
    platform_data(
        name = name,
        target = binary,
        platform = Label("//targets/host_device_simulator"),
        **kwargs
    )

host_device_simulator_binary = macro(
    implementation = _host_device_simulator_binary_impl,
    inherit_attrs = "common",
    attrs = {
        "binary": attr.label(
            doc = "cc_binary build for the host_device_simulator",
            mandatory = True,
        ),
    },
)
