.. _module-pw_kvs-guides:

============================
Guides
============================

.. _module-pw_kvs-get-started:

----------
Quickstart
----------
This guide provides a walkthrough of how to set up and use ``pw_kvs``.

Build system integration
========================
Add ``pw_kvs`` as a dependency in your build system.

.. tab-set::

   .. tab-item:: Bazel

      Add ``@pigweed//pw_kvs`` to your target's ``deps``:

      .. code-block::

         cc_binary(
           # ...
           deps = [
             # ...
             "@pigweed//pw_kvs",
             # ...
           ]
         )

   .. tab-item:: GN

      Add ``$dir_pw_kvs`` to the ``deps`` list in your ``pw_executable()``
      build target:

      .. code-block::

         pw_executable("...") {
           # ...
           deps = [
             # ...
             "$dir_pw_kvs",
             # ...
           ]
         }

   .. tab-item:: CMake

      Link your library to ``pw_kvs``:

      .. code-block::

         add_library(my_lib ...)
         target_link_libraries(my_lib PUBLIC pw_kvs)

Set up the KVS
==============
To set up a ``pw_kvs::KeyValueStore``, follow these three stages:

1. :ref:`Implement the hardware interface <module-pw_kvs-guides-implement-hardware-interface>`:
   Implement a C++ class that allows the KVS to communicate with your target's
   flash hardware.

2. :ref:`Configure and instantiate the KVS <module-pw_kvs-guides-configure-and-instantiate-kvs>`:
   With the hardware interface in place, create a ``KeyValueStore`` instance,
   defining the on-disk data format and memory buffers.

3. :ref:`Configure garbage collection <module-pw_kvs-guides-garbage-collection>`:
   Decide how the KVS will perform garbage collection to reclaim space.

The following sections provide a detailed walkthrough of these stages.

.. _module-pw_kvs-guides-implement-hardware-interface:

Step 1: Implement the hardware interface
----------------------------------------
To use ``pw_kvs`` on a specific hardware platform, implement the
``pw::kvs::FlashMemory`` interface. This class provides a hardware abstraction
layer that the KVS uses to interact with flash storage.

The ``FlashMemory`` class defines the fundamental operations for interacting
with a flash device. Create a concrete class that inherits from
``pw::kvs::FlashMemory`` and implements its pure virtual functions (like
``Read``, ``Write``, and ``Erase``).

When creating your implementation, pass key hardware attributes to the
``FlashMemory`` base class constructor. Consult the datasheet for your MCU or
flash chip to determine these values. The most critical are:

- **Sector size**: The smallest erasable unit of the flash memory, in bytes.
  All erases happen in multiples of this size.

- **Sector count**: The total number of sectors in the flash device.

- **Alignment**: The minimum write size and address alignment for the flash
  hardware, in bytes. This dictates how the KVS packs data.

  Note that ``pw::kvs::FlashMemory`` requires a read alignment of 1. If your
  physical flash has a read alignment greater than 1, your ``FlashMemory``
  implementation must handle this (e.g., by buffering inside
  ``FlashMemory::Read()``) to present an alignment of 1 to the KVS.

  - If your flash supports writing single bytes at any address, set alignment
    to ``1``.
  - If your flash has restrictions, such as only allowing a 4-byte word to be
    written once per erase cycle, set alignment to ``4``. The KVS respects
    these boundaries, preventing invalid partial-word writes.

Once you have a ``FlashMemory`` implementation, create a ``FlashPartition``. A
partition is a separate logical address space representing a contiguous block
of sectors within a ``FlashMemory`` dedicated to a specific purpose, such as a
KVS.

.. code-block:: cpp

   #include "pw_kvs/flash_memory.h"

   // 1. A skeleton of a custom FlashMemory implementation.
   class MyFlashMemory : public pw::kvs::FlashMemory {
    public:
     MyFlashMemory()
         : pw::kvs::FlashMemory(kSectorSize, kSectorCount, kAlignment) {}

     // Implement the pure virtual functions from FlashMemory here...
     // Status Enable() override;
     // Status Disable() override;
     // bool IsEnabled() const override;
     // Status Erase(Address address, size_t num_sectors) override;
     // StatusWithSize Read(Address address, pw::span<std::byte> output) override;
     // StatusWithSize Write(Address address,
     //                      pw::span<const std::byte> data) override;

    private:
     static constexpr size_t kSectorSize = 4096;
     static constexpr size_t kSectorCount = 4;
     static constexpr size_t kAlignment = 4;
   };

   // 2. An instance of your FlashMemory.
   MyFlashMemory my_flash;

   // 3. A partition that uses the first 2 sectors of the flash.
   pw::kvs::FlashPartition partition(&my_flash, 0, 2);

.. _module-pw_kvs-guides-configure-and-instantiate-kvs:

Step 2: Configure and instantiate the KVS
-----------------------------------------
After implementing ``FlashMemory`` and creating a ``FlashPartition``, create
your ``KeyValueStore`` instance. This requires two final pieces of
configuration:

- **Entry format**: The ``pw::kvs::EntryFormat`` struct specifies the magic
  value and checksum algorithm for KVS entries. For a detailed breakdown of
  the on-disk format, see :ref:`module-pw_kvs-disk-format-entry-structure`.
  The magic value is a unique identifier for your KVS, and the checksum
  verifies data integrity.

- **KVS buffers**: The ``pw::kvs::KeyValueStoreBuffer`` template class requires
  specifying the maximum number of entries and sectors the KVS can manage.
  This allocates the necessary memory for the KVS to operate.

Here is an example of how to create a ``KeyValueStore`` instance:

.. code-block:: cpp

   #include "my_flash_memory.h"  // Your FlashMemory implementation
   #include "pw_kvs/crc16_checksum.h"
   #include "pw_kvs/key_value_store.h"

   // Assumes `partition` from the previous step is available.

   pw::kvs::ChecksumCrc16 checksum;
   static constexpr pw::kvs::EntryFormat kvs_format = {.magic = 0xd253a8a9,
                                                       .checksum = &checksum};

   constexpr size_t kMaxEntries = 64;
   constexpr size_t kMaxSectors = 2;  // Must match the partition's sector count

   pw::kvs::KeyValueStoreBuffer<kMaxEntries, kMaxSectors> kvs(&partition,
                                                              kvs_format);

   kvs.Init();

.. _module-pw_kvs-guides-garbage-collection:

Step 3: Configure garbage collection
------------------------------------
``pw_kvs`` requires periodic garbage collection (GC) to reclaim space from
stale or deleted entries. Decide whether to trigger this automatically by the
KVS or manually by your application.

Automatic garbage collection
^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Automatic GC is recommended for most use cases. ``pw_kvs`` automatically runs
a GC cycle during a ``Put()`` operation if it cannot find enough space for new
data. Configure this via the ``gc_on_write`` option passed to the
``KeyValueStore`` constructor.

.. code-block:: cpp

   pw::kvs::Options options;
   options.gc_on_write = pw::kvs::GargbageCollectOnWrite::kAsManySectorsNeeded;

   pw::kvs::KeyValueStoreBuffer<kMaxEntries, kMaxSectors> kvs(&partition,
                                                              kvs_format,
                                                              options);

Available automatic GC options:

- ``kAsManySectorsNeeded`` (Default): ``pw_kvs`` garbage collects as many
  sectors as needed to make space for the write.
- ``kOneSector``: ``pw_kvs`` garbage collects at most one sector. If that is
  not enough to create space, the write fails.
- ``kDisabled``: Disables automatic GC. See the manual section below.

Manual garbage collection
^^^^^^^^^^^^^^^^^^^^^^^^^
If your application requires fine-grained control over potentially
long-running flash operations, trigger GC manually. Manual GC can be performed
independently of the automatic GC configuration.

To disable automatic GC and rely solely on manual triggers:

.. code-block:: cpp

   pw::kvs::Options options;
   options.gc_on_write = pw::kvs::GargbageCollectOnWrite::kDisabled;

   pw::kvs::KeyValueStoreBuffer<kMaxEntries, kMaxSectors> kvs(&partition,
                                                              kvs_format,
                                                              options);

Call one of the maintenance functions at appropriate times in your
application's logic:

- ``kvs.PartialMaintenance()``: Performs GC on a single sector. Use this for
  incrementally cleaning up the KVS over time.
- ``kvs.FullMaintenance()``: Performs a GC of all sectors if the KVS is over
  70% full. This operation also updates all entries to the primary format and
  ensures all entries have the configured redundancy.
- ``kvs.HeavyMaintenance()``: Performs a ``FullMaintenance()`` and does a
  maximal cleanup removing all deleted and all stale entries.

.. _module-pw_kvs-guides-advanced-topics:

---------------
Advanced topics
---------------

.. _module-pw_kvs-guides-updating-kvs-configuration:

Updating KVS configuration over time
====================================
A key consideration for long-lived products is handling firmware updates that
might need to change the KVS configuration. ``pw_kvs`` is flexible, allowing
for several types of changes to its size and layout.

Here are general guidelines for what you can safely modify in a firmware
update.

Flash partition and sector count
--------------------------------
You can resize or move the flash partition used by the KVS.

- **Increasing sectors**: You can safely increase the number of sectors. The
  new flash partition can grow forwards, backwards, or be in a completely
  different location, as long as it includes all non-erased sectors from the
  old KVS instance.
- **Decreasing sectors**: You can decrease the number of sectors, provided the
  new, smaller partition still contains all sectors that have valid KVS data.
- **Sector size**: The logical sector size **must remain the same** across
  firmware updates. Changing the sector size prevents the KVS from correctly
  interpreting existing data.

Maximum entry count
-------------------
You can adjust the maximum number of key-value entries the KVS can hold.

- **Increasing entries**: You can safely increase the maximum entry count at
  any time. This simply allocates more RAM for tracking entries and doesn't
  affect the on-disk format.
- **Decreasing entries**: You can decrease the maximum entry count, but the
  new limit must be greater than or equal to the number of entries currently
  stored in the KVS.

Redundancy
----------
You can change the number of redundant copies for each entry.

- **Changing redundancy level**: You can safely increase or decrease the
  redundancy level between firmware updates. When initialized with the new
  redundancy level, the KVS detects the mismatch. During the next maintenance
  cycle (e.g., a call to ``PartialMaintenance()`` or ``FullMaintenance()``),
  the KVS automatically writes new redundant copies or ignores extra ones to
  match the new configuration.

Entry format
------------
The ``EntryFormat`` defines the magic value and checksum algorithm for entries.

- **Adding new formats**: To support backward compatibility, provide a list of
  ``EntryFormat`` structs to the ``KeyValueStore`` constructor. The KVS can
  read entries matching any of the provided formats. The first format in the
  list is the "primary" format, used for all new entries written to the KVS.
- **Changing existing formats**: **Do not change** an existing ``EntryFormat``
  (magic or checksum). Doing so causes the KVS to fail to read existing
  entries, treating them as corrupt data.
