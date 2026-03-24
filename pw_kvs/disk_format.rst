.. _module-pw_kvs-disk-format:

=========================
On-disk format and design
=========================
The ``pw_kvs`` module stores data as a log of entries written sequentially into
flash sectors. This append-only design enables safe recovery from unexpected
power loss. The system is designed primarily for
`NOR flash <https://en.wikipedia.org/wiki/Flash_memory#Distinction_between_NOR_and_NAND_flash>`_,
which is common in embedded systems. It does not include features required for
NAND flash, such as bad block management, as a dedicated flash translation
layer (FTL) typically handles these.

.. _module-pw_kvs-disk-format-log-structured:

High-level structure: a log of entries in sectors
=================================================
``pw_kvs`` divides flash memory into sectors. The KVS operates as a
log-structured file system, meaning ``pw_kvs`` always appends data to the log;
it never modifies data in place.

``pw_kvs`` writes entries one after another into the current active sector.
When updating a key, ``pw_kvs`` writes a new entry containing the new value to
the end of the log. ``pw_kvs`` considers the previous entry for that key
"stale." ``pw_kvs`` handles deletes similarly, by writing a special
"tombstone" entry.

This append-only approach suits NOR flash memory, which allows writing in small
increments but requires erasing in larger blocks.
:ref:`Garbage collection <module-pw_kvs-guides-garbage-collection>` is triggered
during maintenance operations to reclaim space from stale entries.


.. _module-pw_kvs-disk-format-entry-structure:

Low-level structure: the entry format
=====================================
``pw_kvs`` stores each piece of data in an "entry" packet. This packet
contains metadata alongside the key and value. The structure is compact and
efficient for embedded systems.

.. list-table:: KVS entry structure
   :widths: 20 15 65
   :header-rows: 1

   * - Field
     - Size
     - Description
   * - Magic
     - 32-bit
     - Identifier that marks the beginning of a valid entry and indicates the
       data format version.
   * - Checksum
     - Variable
     - Checksum (e.g. CRC16) of the entry's contents, which verifies data
       integrity. The ``ChecksumAlgorithm`` configured for the KVS determines
       the size.
   * - Alignment
     - 8-bit
     - An 8-bit field (`alignment_units`) used to calculate the entry's
       alignment. The alignment in bytes is ``(alignment_units + 1) * 16``.
       This allows for alignments from 16 to 4096 bytes.
   * - Key Length
     - 8-bit
     - Length of the key string in bytes.
   * - Value Size
     - 16-bit
     - Size of the value in bytes. A special value of ``0xFFFF`` marks the
       entry as a "tombstone," indicating a deleted key.
   * - Transaction ID
     - 32-bit
     - Monotonically increasing number that orders the entries.
   * - Key
     - Variable
     - Key string, with a variable length up to 255 bytes. It is not
       null-terminated.
   * - Value
     - Variable
     - Data associated with the key.
   * - Padding
     - Variable
     - Zero-bytes added to the end of the entry so that its total size is a
       multiple of the entry's calculated alignment.

.. _module-pw_kvs-disk-format-invariants:

Design principle: no global metadata
====================================
Many storage systems maintain a central block of metadata (e.g., allocation
tables, journals) that contains information about the overall state of the
system. While this can offer performance benefits, it also introduces a single
point of failure.

``pw_kvs`` avoids this by deriving its state entirely from the individual
key-value entries stored in flash. This design has several advantages:

- **Robustness**: No single block of data can be corrupted and render the
  entire KVS unusable. Also, no single block requires repeated modification,
  which would lead to flash degradation.
- **Simplicity**: This simplifies KVS logic, since no complicated invariants
  exist between the entries and a global metadata block.

With this approach, ``pw_kvs`` initializes by scanning the entire flash
partition and reading all valid entries. The tradeoff for avoiding global
metadata is that the KVS can't easily support multi-key transactions, where
``pw_kvs`` modifies values for multiple keys, and records either all updates
(in a successful write) or none (in a data loss event). This tradeoff is
sufficient for common embedded use cases.

.. note::

   Depending on the size of the flash partition and the number of entries, KVS
   initialization can take a significant amount of time. Regularly running
   maintenance operations (GC) will help keep the KVS responsive.

Invariants
==========
The KVS maintains the following invariants.

- **One free sector for garbage collection**: The KVS requires at least one
  free (erased) sector at all times to guarantee that garbage collection can
  always proceed.

- **Entries do not cross sector boundaries**: An entry must be fully contained
  within a single sector. This is a direct consequence of the physical
  constraints of flash hardware, which can only be erased in fixed-size blocks
  (sectors). If an entry spanned two sectors, erasing one would corrupt the
  entry.

- **Self-contained entries**: Every entry is a complete, self-describing
  record. It contains all the information needed to interpret its own data,
  including its size (via alignment), key length, and value size.

- **Redundant copies in different sectors**: The KVS maintains a configured
  number of copies (replicas) for each logical key-value entry. Each of these
  entries is bit-for-bit identical, including the key, value, and transaction
  ID. As a placement heuristic, ``pw_kvs`` stores each copy in a different
  flash sector to protect against sector-level corruption. If a corruption
  event or flash failure leads to the loss of one or more copies, the KVS
  detects this deficit and restores the configured level of redundancy.

- **Highest transaction ID is newest**: For a given key, the entry with the
  highest transaction ID is the most recent one.

Implementation details
======================

Alignment, padding, and forward compatibility
---------------------------------------------
The ``Alignment`` and ``Padding`` fields work together to ensure that
``pw_kvs`` stores each entry correctly on flash memory, which often has
specific alignment requirements for writes.

``pw_kvs`` stores the required alignment **within each entry's header**. This
8-bit value isn't the alignment itself, but an ``alignment_units`` value used
to calculate byte alignment with the formula ``(alignment_units + 1) * 16``.
Advantages of this approach:

- **Flexibility**: It supports alignments from 16 bytes (for ``alignment_units
  = 0``) to 4096 bytes (for ``alignment_units = 255``), which is sufficient for
  most flash hardware.
- **Space efficiency**: Storing a single 8-bit integer is more compact than
  storing a 16-bit or 32-bit alignment value.
- **Forward compatibility**: Because each entry is self-describing, a future
  firmware update can change the alignment for new entries. The new firmware
  can still correctly calculate the size of older entries. When moving an old
  entry during garbage collection, ``pw_kvs`` rewrites it with the new, larger
  alignment, upgrading the data in place over time.

Transaction ID rollover
-----------------------
To identify the most recent version of a key, the KVS uses a 32-bit transaction
ID incremented on every write.

By design, the KVS does not handle the rollover of this transaction ID. This is a
practical trade-off based on the lifecycle of typical flash hardware. A
32-bit transaction ID provides approximately 4.3 billion unique IDs. In
contrast, a standard NOR flash sector has a write/erase endurance of around
100,000 cycles per sector.

Because ``pw_kvs`` distributes writes across all available sectors for
wear-leveling, physical flash memory will likely wear out long before the
transaction ID space is exhausted. For most embedded applications, this makes
transaction ID rollover a theoretical concern rather than a practical one.
