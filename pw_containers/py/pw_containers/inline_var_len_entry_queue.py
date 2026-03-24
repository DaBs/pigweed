# Copyright 2023 The Pigweed Authors
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
"""Decodes the in-memory representation of a sized-entry ring buffer."""

import struct
from typing import Iterable
from pw_containers import var_len_entry_queue

_HEADER = struct.Struct('III')  # data_size_bytes, head, tail


def parse(queue: bytes) -> Iterable[bytes]:
    """Decodes the in-memory representation of a variable-length entry queue.

    The queue includes both the data buffer and the indices that designate the
    range in the buffer that contains variable-length entries.

    Args:
      queue: The bytes representation of a variable-length entry queue.

    Yields:
      Each entry in the buffer as bytes.
    """
    array_size_bytes, head, tail = _HEADER.unpack_from(queue)

    total_encoded_size = _HEADER.size + array_size_bytes
    if len(queue) < total_encoded_size:
        raise ValueError(
            f'Ring buffer data ({len(queue)} B) is smaller than the encoded '
            f'size ({total_encoded_size} B)'
        )

    data = queue[_HEADER.size : total_encoded_size]
    return var_len_entry_queue.parse(data, head, tail)
