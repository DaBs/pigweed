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
"""Decodes a sized-entry ring buffer."""

from typing import Iterable


def _decode_leb128(
    data: bytes, offset: int = 0, max_bits: int = 32
) -> tuple[int, int]:
    count = value = shift = 0

    while offset < len(data):
        byte = data[offset]

        count += 1
        value |= (byte & 0x7F) << shift

        if not byte & 0x80:
            return offset + count, value

        shift += 7
        if shift >= max_bits:
            raise ValueError(f'Varint exceeded {max_bits}-bit limit')

    raise ValueError(f'Unterminated varint {data[offset:]!r}')


def parse(data: bytes, head: int, tail: int) -> Iterable[bytes]:
    """Decodes a variable-length entry queue.

    Unlike `inline_var_len_entry_queue.parse`, the indices to the buffer that
    that contain variable-length entries must be explicitly provided.

    Args:
      data: A ring buffer holding variable-length entries.
      head: Index into the data buffer at which the first entry starts.
      tail: Index into the data buffer after the end of the last entry.

    Yields:
      Each entry in the buffer as bytes.
    """
    if 0 > head or head >= len(data):
        raise ValueError(
            'Corruption detected; '
            f'ending index {head} is invalid for a {len(data)} B array'
        )

    if 0 > tail or tail >= len(data):
        raise ValueError(
            'Corruption detected; '
            f'starting index {head} is invalid for a {len(data)} B array'
        )

    if tail < head:
        return parse_contiguous(data[head:], data[:tail])

    return parse_contiguous(data[head:tail], bytes())


def parse_contiguous(data1: bytes, data2: bytes) -> Iterable[bytes]:
    """Decodes a sequence of contiguous variable-length entries.

    The entries are may be spread across one or two byte sequences, depending on
    whether they wrapped around the end of the ring buffer they were copied
    from.

    Args:
      data1: A sequence of variable-length entries. May end in a partial entry.
      data2: A sequence of variable-length entries. May begin with the
             partial entry that  completes the end of `data1`.

    Yields:
      Each entry in the buffer as bytes.
    """
    data = data1 + data2
    index = 0
    while index < len(data):
        index, size = _decode_leb128(data, index)

        if index + size > len(data):
            raise ValueError(
                'Corruption detected; '
                f'encoded size {size} B is too large for a {len(data)} B array'
            )
        yield data[index : index + size]

        index += size
