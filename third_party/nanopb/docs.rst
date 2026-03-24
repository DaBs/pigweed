.. _module-pw_third_party_nanopb:

======
Nanopb
======

The ``$pw_external_nanopb/`` module contains Nanopb, a tiny protobuf
library. It is used by :ref:`module-pw_protobuf_compiler`.

Follow the documentation on :ref:`module-pw_protobuf_compiler` for general
help on how to use this.

----------------
GN Build Support
----------------
This module provides support to compile Nanopb with GN.

Enabling ``PB_NO_ERRMSG=1``
---------------------------

In your toolchain configuration, you can use the following:

.. code-block::

   pw_third_party_nanopb_CONFIG = "$pw_external_nanopb:disable_error_messages"


This will add ``-DPB_NO_ERRMSG=1`` to the build, which disables error messages
as strings and may save some code space at the expense of ease of debugging.

-------------------
CMake Build Support
-------------------
This module provides support to compile Nanopb with CMake.

It defines the ``pw_third_party.nanopb`` interface target, which one can link
against to use nanopb.

The CMake variable ``dir_pw_third_party_nanopb`` must be set to point to a local
nanopb installation.

In a Zephyr build, one should enable the Nanopb Zephyr module and enable the
``CONFIG_NANOPB`` KConfig option. In this case ``dir_pw_third_party_nanopb``
will be automatically set.
