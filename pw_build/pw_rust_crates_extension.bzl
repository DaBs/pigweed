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
"""A crates_io hub that allows projects to redirect Pigweed's crates.io deps.

See the documentation at https://pigweed.dev/third_party/crates_io/ for more
information.
"""

def _crates_io_hub_impl(ctx):
    build_file_path = ctx.path(ctx.attr._pigweed_hub_build_file)
    ctx.watch(build_file_path)
    build_file_contents = ctx.read(build_file_path)
    ctx.file("BUILD.bazel", content = build_file_contents)

_crates_io_hub = repository_rule(
    implementation = _crates_io_hub_impl,
    attrs = {
        "_pigweed_hub_build_file": attr.label(
            allow_single_file = True,
            default = "//third_party/crates_io/rust_crates:alias_hub.BUILD",
        ),
    },
)

def _pw_rust_crates_extension_impl(_ctx):
    _crates_io_hub(name = "rust_crates")

pw_rust_crates_extension = module_extension(
    implementation = _pw_rust_crates_extension_impl,
)
