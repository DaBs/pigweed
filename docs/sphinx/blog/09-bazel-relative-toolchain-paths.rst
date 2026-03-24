.. _blog-09-bazel-relative-toolchain-paths:

================================================================================
Pigweed Blog #9: Satisfying Bazel's relative paths requirement in C++ toolchains
================================================================================
*Published on 2025-11-05 by Armando Montanez*

Today's blog post is an instructive, technical deep-dive into one of Bazel's
most infamous C++ errors:

.. code-block:: none

   ERROR: /projects/pigweed/pw_string/BUILD.bazel:67:11: Compiling pw_string/format.cc failed: absolute path inclusion(s) found in rule '//pw_string:format':
   the source file 'pw_string/format.cc' includes the following non-builtin files with absolute paths (if these are builtin files, make sure these paths are in your toolchain):
     '/private/var/tmp/_bazel_amontanez/e724b21efc8bc19866072fbc72ee5907/external/+_repo_rules6+llvm_toolchain/include/c++/v1/cstdarg'
     '/private/var/tmp/_bazel_amontanez/e724b21efc8bc19866072fbc72ee5907/external/+_repo_rules6+llvm_toolchain/include/c++/v1/__config'
     ...

If you've ever tried to put together a custom C/C++ toolchain, you've probably
encountered this error.

------------------------------------------
What is this error, and why does it exist?
------------------------------------------
Fundamentally, this error is a very strict correctness check provided by Bazel.
C/C++ toolchains traditionally are eager to allow arbitrary local files to leak
into compiler invocations. This can affect build
`hermeticity <https://bazel.build/basics/hermeticity>`__ in sometimes subtle,
and other times catastrophic ways. This check is one of Bazel's most
powerful—but also often confusing—tools to combat toolchain hermeticity leaks.

While the first temptation might be to look for a way to disable the check,
here's two good reasons why you probably shouldn't do that (assuming you could):

* This error flags local dependencies that have leaked into the build. Sometimes
  you may choose to do this anyway, but if you're investing in a Bazel build,
  that decision should not be taken lightly!
* This means Bazel can clearly see a case where compiler outputs are NOT
  deterministic—i.e. hermetic. By nature, these paths can vary from machine to
  machine, which means that Bazel is finding outputs that contain
  machine-specific absolute paths.

How does this work under the hood?
==================================
Most compilers support
`Generating Prerequisites Automatically <https://www.gnu.org/software/make/manual/html_node/Automatic-Prerequisites.html>`__.
Bazel uses this to stamp out ``.d`` files that exhaustively list all loaded
dependencies. When all of these paths match files included in Bazel's
sandbox, it's pretty easy to have confidence that the toolchain hermetically
enumerates all the files involved in the compilation.

When a toolchain hits this error, the ``.d`` file usually looks something like
this:

.. code-block:: none

   bazel-out/darwin_arm64-fastbuild/bin/pw_string/_objs/format/format.pic.o: \
     pw_string/format.cc \
     bazel-out/darwin_arm64-fastbuild/bin/pw_string/_virtual_includes/format/pw_string/format.h \
     /private/var/tmp/_bazel_amontanez/e724b21efc8bc19866072fbc72ee5907/external/+_repo_rules6+llvm_toolchain/bin/../include/c++/v1/cstdarg \bazel_amontanez/e724b21efc8bc19866072fbc72ee5907/external/+_repo_rules6+llvm_toolchain/bin/../include/c++/v1/__type_traits/void_t.h \
     /private/var/tmp/_bazel_amontanez/e724b21efc8bc19866072fbc72ee5907/external/+_repo_rules6+llvm_toolchain/bin/../include/c++/v1/__type_traits/is_void.h \
     /private/var/tmp/_bazel_amontanez/e724b21efc8bc19866072fbc72ee5907/external/+_repo_rules6+llvm_toolchain/bin/../include/c++/v1/__type_traits/is_array.h \
     /private/var/tmp/_bazel_amontanez/e724b21efc8bc19866072fbc72ee5907/external/+_repo_rules6+llvm_toolchain/bin/../include/c++/v1/__cstddef/size_t.h \
     /private/var/tmp/_bazel_amontanez/e724b21efc8bc19866072fbc72ee5907/external/+_repo_rules6+llvm_toolchain/bin/../include/c++/v1/__type_traits/is_function.h \
     /private/var/tmp/_bazel_amontanez/e724b21efc8bc19866072fbc72ee5907/external/+_repo_rules6+llvm_toolchain/bin/../include/c++/v1/__type_traits/remove_extent.h \
     /private/var/tmp/_bazel_amontanez/e724b21efc8bc19866072fbc72ee5907/external/+_repo_rules6+llvm_toolchain/bin/../include/c++/v1/__type_traits/enable_if.h \
     /private/var/tmp/_bazel_amontanez/e724b21efc8bc19866072fbc72ee5907/external/+_repo_rules6+llvm_toolchain/bin/../include/c++/v1/__type_traits/is_base_of.h \
     /private/var/tmp/_bazel_amontanez/e724b21efc8bc19866072fbc72ee5907/external/+_repo_rules6+llvm_toolchain/bin/../include/c++/v1/__type_traits/is_core_convertible.h \
     /private/var/tmp/_bazel_amontanez/e724b21efc8bc19866072fbc72ee5907/external/+_repo_rules6+llvm_toolchain/bin/../include/c++/v1/__type_traits/is_member_pointer.h \
     /private/var/tmp/_bazel_amontanez/e724b21efc8bc19866072fbc72ee5907/external/+_repo_rules6+llvm_toolchain/bin/../include/c++/v1/__type_traits/is_reference_wrapper.h \
     /private/var/tmp/_bazel_amontanez/e724b21efc8bc19866072fbc72ee5907/external/+_repo_rules6+llvm_toolchain/bin/../include/c++/v1/__type_traits/is_same.h \
     ...

On the other hand, a ``.d`` file that *does not* trigger this error looks
something like this:

.. code-block:: none

   bazel-out/darwin_arm64-fastbuild/bin/pw_string/_objs/format/format.pic.o: \
     pw_string/format.cc \
     bazel-out/darwin_arm64-fastbuild/bin/pw_string/_virtual_includes/format/pw_string/format.h \
     external/+_repo_rules6+llvm_toolchain/bin/../include/c++/v1/cstdarg \
     external/+_repo_rules6+llvm_toolchain/bin/../include/c++/v1/__config \
     external/+_repo_rules6+llvm_toolchain/bin/../include/c++/v1/__config_site \
     external/+_repo_rules6+llvm_toolchain/bin/../include/c++/v1/__configuration/abi.h \
     external/+_repo_rules6+llvm_toolchain/bin/../include/c++/v1/__configuration/compiler.h \
     external/+_repo_rules6+llvm_toolchain/bin/../include/c++/v1/__configuration/platform.h \
     external/+_repo_rules6+llvm_toolchain/bin/../include/c++/v1/__configuration/availability.h \
     external/+_repo_rules6+llvm_toolchain/bin/../include/c++/v1/__configuration/language.h \
     external/+_repo_rules6+llvm_toolchain/lib/clang/21/include/stdarg.h \
     external/+_repo_rules6+llvm_toolchain/lib/clang/21/include/__stdarg_header_macro.h \
     external/+_repo_rules6+llvm_toolchain/lib/clang/21/include/__stdarg___gnuc_va_list.h \
     external/+_repo_rules6+llvm_toolchain/lib/clang/21/include/__stdarg_va_list.h \
     ...

Notice how there's no ``_bazel_amontanez`` in this second file? Bazel parses
these files and checks the enumerated paths to ensure they're either relative,
or allowlisted. This segues us nicely into our next topic...

------------------------
How do I fix this error?
------------------------
Best case scenario, fixing this is actually very easy! All you have to do is
add the following flags to your toolchain configuration:

.. _blog-09-bazel-relative-toolchain-paths-required-flags:

.. tab-set::

    .. tab-item:: Clang

        .. code-block:: none

           -no-canonical-prefixes

    .. tab-item:: GCC

        .. code-block:: none

           -no-canonical-prefixes
           -fno-canonical-system-headers

These flags tell the compiler to *not* convert relative paths to absolute paths.
For well-behaving toolchain distributions, these flags are usually all you need
to get past the error.

But what about the times where this *isn't* enough?


Troubleshooting: Builtin toolchain headers are still absolute
=============================================================
Sometimes, despite enabling
:ref:`the required flags <blog-09-bazel-relative-toolchain-paths-required-flags>`,
a toolchain may still produce absolute paths for builtins. This is typically
caused by one of the following:

Suspect #1: Files missing from the sandbox
------------------------------------------
Sometimes when headers or supporting binaries/libraries are missing from the
sandbox, rather than getting some kind of "file not found" error, depfile paths
will start magically resolving to absolute paths.

Solution: Ensure all toolchain resources are included in the sandbox
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
When in doubt, broadly ``glob()`` all toolchain files to ensure they make it
into the sandbox. If you're trying to be more surgical, keep an eye out for
these patterns:

* Headers in unexpected toolchain subdirectories.
* Intermediate binaries like ``bin/llvm`` and
  ``lib/gcc/arm-none-eabi/10.2.1/cc1``.
* Runtime libraries that the compiler dynamically loads.

Suspect #2: Poorly designed wrappers
------------------------------------
Some toolchain distributions include wrappers that inherently prevent the
toolchain flags that make depfile paths relative from working as intended. While
this can be resolved in some cases by simply bypassing the wrapper, usually
the only workaround is significantly more involved.

Solution: Manually declare builtins with ``-isystem``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
This, unfortunately, requires some manual inspection of your specific
toolchain's behavior. Still, it's not *too* difficult.

.. _blog-09-bazel-relative-toolchain-paths-discover-builtins:

**Step 1: Determine builtin directories**

By adding ``-v`` to your compiler flags (or via ``--copt``), LLVM and GCC-based
compilers will emit the include search path:

.. code-block:: none

   #include "..." search starts here:
    .
    bazel-out/k8-fastbuild/bin
   #include <...> search starts here:
    /usr/local/google/home/amontanez/.cache/bazel/_bazel_amontanez/06cb1b6ef37c7adbbc0068a64c52919c/external/+_repo_rules6+llvm_toolchain/bin/../include/x86_64-unknown-linux-gnu/c++/v1
    /usr/local/google/home/amontanez/.cache/bazel/_bazel_amontanez/06cb1b6ef37c7adbbc0068a64c52919c/external/+_repo_rules6+llvm_toolchain/bin/../include/c++/v1
    /usr/local/google/home/amontanez/.cache/bazel/_bazel_amontanez/06cb1b6ef37c7adbbc0068a64c52919c/external/+_repo_rules6+llvm_toolchain/lib/clang/21/include
    external/+_repo_rules5+linux_sysroot/usr/include/x86_64-linux-gnu
    external/+_repo_rules5+linux_sysroot/usr/include
   End of search list.

Take note of the absolute paths **and their order**, as you'll need to override
these paths with relative alternatives **in the same order**. Also, keep in mind
that this list can change as you target different platforms, or whenever you
switch between compiling for C and C++. For example, compiling for the Raspberry
Pi Pico produces a list like this:

.. code-block:: none

   #include "..." search starts here:
    .
    bazel-out/rp2040-fastbuild/bin
   #include <...> search starts here:
    /usr/local/google/home/amontanez/.cache/bazel/_bazel_amontanez/06cb1b6ef37c7adbbc0068a64c52919c/external/+_repo_rules6+llvm_toolchain/bin/../include/armv6m-unknown-none-eabi/c++/v1
    /usr/local/google/home/amontanez/.cache/bazel/_bazel_amontanez/06cb1b6ef37c7adbbc0068a64c52919c/external/+_repo_rules6+llvm_toolchain/bin/../include/c++/v1
    /usr/local/google/home/amontanez/.cache/bazel/_bazel_amontanez/06cb1b6ef37c7adbbc0068a64c52919c/external/+_repo_rules6+llvm_toolchain/lib/clang/21/include
    /usr/local/google/home/amontanez/.cache/bazel/_bazel_amontanez/06cb1b6ef37c7adbbc0068a64c52919c/external/+_repo_rules6+llvm_toolchain/bin/../include/armv6m-unknown-none-eabi
   End of search list.

.. admonition:: Tip

   A quick way to get this list is to run the compiler in preprocess-only mode
   using this command:

   .. code-block:: console

      $ clang -v -E -x- < /dev/null

   Then you can add flags like ``-xc++`` to see how they affect the include
   search list.

**Step 2: Identify relative alternatives**

Now that you have the list of builtins, you need to determine what the relative
equivalent would be in the sandbox. With rule-based toolchains, you can use
``directory`` rules and the formatting syntax to skip this step.

If you are using the legacy toolchain API, you can construct the base path
for toolchains that live in an external repository like this:

.. code-block::

   LLVM_TOOLCHAIN = "external/" + Label("@llvm_toolchain").repo_name

**Step 3: Replace builtin include paths**

Now that you have the list of new include paths, you can override the builtin
paths. While the correct flag spelling varies from compiler to compiler, it
usually looks something like this:

.. code-block:: py

   cc_args(
       name = "relative_builtins",
       actions = ["@rules_cc//cc/toolchains/actions:compile_actions"],
       args = [
           "-nostdinc",
           "-isystem",
           "{INCLUDE_CPP_V1}",
           "-isystem",
           "{LIB_CLANG_INCLUDE}",
           # ...
       ],
       format = {
           "LIB_CLANG_INCLUDE": "@llvm_toolchain//:lib-clang-include",
           "INCLUDE_CPP_V1": "@llvm_toolchain//:include-c++-v1",
           # ...
       },
   )

.. admonition:: Note

   You may need to split these groups of include paths into C and C++ specific
   sets to prevent C++-only include paths from ending up on the include path
   when they shouldn't.

Suspect #3: Calling from ``PATH``
---------------------------------
Fun fact, if you invoke ``clang`` relying on the system ``PATH``,
``-no-canonical-prefixes`` won't work as expected.

Solution: Directly invoke the compiler
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Rather than relying on finding the compiler via ``PATH``, ensure that the
compiler is directly called with a relative path.


.. _blog-09-bazel-relative-toolchain-paths-allowlist-absolute-paths:

Troubleshooting: Local dependencies
===================================
Sometimes, rather than pointing to paths in the Bazel sandbox, the error will
indicate that files at absolute, local paths like ``/usr/include`` are being
pulled in during compilation. Usually, the ideal way to address these is to
increase the coverage of your vendored dependencies. For Linux, this usually
means `Setting up a sysroot <https://llvm.org/docs/HowToCrossCompileLLVM.html#setting-up-a-sysroot>`__
that contains any system libraries/headers required at compile time.

The status-quo for macOS, on the other hand, is typically to just allowlist some
of these absolute paths due to redistribution restrictions. This definitely
isn't ideal, but is a relatively common choice (as seen in
`@apple_support <https://github.com/bazelbuild/apple_support/blob/b5658f5ebd3d3585dc49751e839316762ed008a0/crosstool/osx_cc_configure.bzl#L249>`__).

If you decide you want Bazel to accept some of these absolute paths, you
can use `cc_args.allowlist_absolute_include_directories
<https://github.com/bazelbuild/rules_cc/blob/65a547d606d4d31949d8f831d1c40ab25b3d26bb/docs/toolchain_api.md#cc_args-allowlist_absolute_include_directories>`__
(or ``cxx_builtin_include_directories`` for the older
`cc_common.create_cc_toolchain_config_info <https://bazel.build/rules/lib/toplevel/cc_common#create_cc_toolchain_config_info>`__)
to explicitly allow absolute paths to leak into the build.

What about Windows?
===================
Unfortunately, I haven't had the pleasure of diving deep into hermetic Windows
toolchains, so I can't say from experience what fun (for some definition of the
word) lies there. When Pigweed brings up our Windows support for our Bazel
build, rest assured that this topic will be revisited!

----------------------------------------------------
Why don't I see this with Bazel's default toolchain?
----------------------------------------------------
The default Bazel toolchain doesn't emit this error for two reasons:

#. It relies on the locally installed C/C++ toolchain, which is not hermetic.
#. It automagically :ref:`discovers builtin include directories <blog-09-bazel-relative-toolchain-paths-discover-builtins>`
   and :ref:`allowlists <blog-09-bazel-relative-toolchain-paths-allowlist-absolute-paths>`
   all of them.

Because the default C/C++ toolchain isn't vendored or hermetic, it must
suppress the most likely ways you'll encounter this error.

---------------------------------------
What if I don't want to deal with this?
---------------------------------------
Pigweed has a handful of pre-baked toolchain configurations in our
:ref:`module-pw_toolchain-bazel`. While they might not be a perfect fit for
everyone, they're built using modular pieces to allow projects to re-use what
they like, and diverge in ways that suit their needs better.

Vendoring a hermetic C/C++ toolchain is worth it, but perhaps that's a story
for another blog post. 😉
