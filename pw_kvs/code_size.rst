.. _module-pw_kvs-code-size:

==================
Code size analysis
==================

The following size report details the memory usage of ``KeyValueStore`` and
``FlashPartition``.

.. include:: kvs_size

Understanding the report
========================
The size report shows the changes in flash (code and read-only data) and RAM
(static data) resulting from using ``pw_kvs``.

- **FlashPartition**: This column shows the base cost of using the
  ``pw::kvs::FlashPartition`` abstraction, which is required by KVS.
- **KeyValueStore**: This column shows the additional cost of the KVS
  implementation itself.

Code size
=========
The code size of ``pw_kvs`` is driven by features that ensure data integrity and
longevity on flash media:

- **Garbage collection**: Reclaims space from obsolete entries.
- **Wear leveling**: Distributes writes across sectors to extend flash life.
- **Checksums**: Detects data corruption.
- **Redundancy**: Manages multiple copies of entries for fault tolerance.

RAM usage
=========
``pw_kvs`` uses RAM to store metadata about sectors and entries for fast lookups
and management. Buffers for this metadata are typically allocated at compile
time using ``pw::kvs::KeyValueStoreBuffer``.

The RAM usage depends on the KVS configuration:

- **Maximum number of entries**: Metadata is stored for each key-value pair.
- **Maximum number of sectors**: Metadata is stored for each flash sector.
- **Redundancy level**: Additional space is required to track redundant copies.

Example
-------
Consider a KVS configured for a typical embedded application:

- **Maximum entries**: 50
- **Maximum sectors**: 8
- **Redundancy**: 2

On a 32-bit architecture, the RAM usage for metadata buffers would be
approximately **1.0 kB**. This includes:

- **Sector descriptors**: ~32 bytes
- **Key descriptors**: ~600 bytes
- **Addresses**: ~408 bytes
