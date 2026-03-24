.. _docs-cmake-integration-modules:

===================================
Use Pigweed modules in your project
===================================
Once you have :ref:`set up Pigweed <docs-cmake-integration-add-pigweed-as-a-dependency>`,
you can start using Pigweed modules in your project.

Link against Pigweed modules
============================
Pigweed modules are exposed as CMake targets. To use a module, simply link against
it using ``target_link_libraries``.

For example, to use ``pw::Vector`` from :ref:`module-pw_containers`:

1.  Include the header in your code:

    .. code-block:: cpp

       #include "pw_containers/vector.h"

2.  Link against the module in your ``CMakeLists.txt``:

    .. code-block:: cmake

       add_library(my_library my_library.cc)
       target_link_libraries(my_library PUBLIC pw_containers.vector)

Use pw_add_library
==================
Pigweed provides a helper function ``pw_add_library`` which simplifies library
creation and automatically handles some Pigweed-specific configuration. It is
recommended to use this for your own libraries that depend on Pigweed.

.. code-block:: cmake

   include($ENV{PW_ROOT}/pw_build/pigweed.cmake)

   pw_add_library(my_library
     SOURCES
       my_library.cc
     PUBLIC_DEPS
       pw_containers.vector
   )

See :ref:`module-pw_build-cmake` for more details on ``pw_add_library``.

Protobuf Generation
===================
Pigweed provides CMake support for generating C++ and Python code from Protobuf
files.

.. code-block:: cmake

   include($ENV{PW_ROOT}/pw_protobuf_compiler/proto.cmake)

   pw_proto_library(my_protos
     SOURCES
       my_proto.proto
   )

   pw_add_library(my_lib
     SOURCES
       my_lib.cc
     PUBLIC_DEPS
       my_protos.pwpb
   )

See :ref:`module-pw_protobuf_compiler` for more details.
