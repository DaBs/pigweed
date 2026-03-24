.. _docs-cmake-integration-add-pigweed-as-a-dependency:

========================================
Set up Pigweed as an external dependency
========================================
Pigweed can be added to your CMake project as a subdirectory. This can be done
either by using ``FetchContent`` or by adding it as a git submodule.

Add Pigweed using FetchContent
==============================
The recommended way to add Pigweed is using CMake's ``FetchContent`` module.

.. code-block:: cmake

   include(FetchContent)

   FetchContent_Declare(
     pigweed
     GIT_REPOSITORY https://pigweed.googlesource.com/pigweed/pigweed
     GIT_TAG        c00e9e430addee0c8add16c32eb6d8ab94189b9e # Replace with desired commit
   )

   FetchContent_MakeAvailable(pigweed)

   # Set PW_ROOT environment variable for Pigweed modules to use
   FetchContent_GetProperties(pigweed SOURCE_DIR PIGWEED_SOURCE_DIR)
   set(ENV{PW_ROOT} "${PIGWEED_SOURCE_DIR}")

   # Include the main Pigweed CMake file
   include($ENV{PW_ROOT}/pw_build/pigweed.cmake)

Alternative: Add Pigweed as Git submodule
=========================================
If you prefer to manage dependencies as submodules, add Pigweed as a submodule
and then add it to your ``CMakeLists.txt``:

.. code-block:: bash

   git submodule add https://pigweed.googlesource.com/pigweed/pigweed third_party/pigweed

Then in your ``CMakeLists.txt``:

.. code-block:: cmake

   set(ENV{PW_ROOT} "${CMAKE_CURRENT_SOURCE_DIR}/third_party/pigweed")
   include($ENV{PW_ROOT}/pw_build/pigweed.cmake)

Configure Backends
==================
Pigweed uses facades (interfaces) that require backends (implementations) to be
configured. You must configure these backends before defining your targets.

For example, to use the basic logging backend:

.. code-block:: cmake

   # Set the backend for pw_log
   pw_set_backend(pw_log pw_log_basic)

   # Set the backend for pw_assert
   pw_set_backend(pw_assert pw_assert_log)

See :ref:`module-pw_build-cmake` for more details on backend configuration.
