.. _module-pw_build-bazel_helpers:

===========================
Miscellaneous Bazel helpers
===========================
.. pigweed-module-subpage::
   :name: pw_build

This page documents miscellaneous Bazel-related helpers that don't fall into
the categories of rules or constraints.

-------------
Build targets
-------------

.. _module-pw_build-bazel-empty_cc_library:

``@pigweed//pw_build:empty_cc_library``
=======================================
This empty library is used as a placeholder for label flags that need to point
to a ``cc_library`` of some kind, but don't actually need the dependency to
amount to anything.

.. note::

   Prefer ``@rules_cc//:empty_lib`` in most cases. This will eventually
   redirect to the utility stub in ``@rules_cc``.

``@pigweed//pw_build:default_link_extra_lib``
=============================================
This library groups together all libraries commonly required at link time by
Pigweed modules. See :ref:`docs-build_system-bazel_link-extra-lib` for more
details.

``@pigweed//pw_build:unspecified_backend``
==========================================
A special target used instead of a ``cc_library`` as the default condition in
backend multiplexer select statements to signal that a facade is in an
unconfigured state. This produces better error messages than e.g. using an
invalid label.

----------------------
Starlark helper macros
----------------------
These helper macros offer convenient ways to express values for various rule
attributes. These strive to provide ALL of the following benefits:

* **Clarify intent.** While ``select`` statements are fundamental building
  blocks, their meaning can be obscured by the syntax. Macros should improve
  readability of the intent of the produced value.
* **Reduce unnecessary verbosity.** Some expressions require significant boiler
  plate to spell out. Macros should never be more verbose than what they
  replace.
* **Scale better.** Some macros opaquely embed values or parameterize incoming
  choices to improve scalability of common patterns. This makes it easier
  to adjust implementation details over time across many different uses of the
  pattern.

Platform-based flag merging
===========================
Macros that help with using platform-based flags are in
``//pw_build:merge_flags.bzl``. These are useful, for example, when you wish to
:ref:`docs-bazel-compatibility-facade-backend-dict`.

incompatible_with_mcu
---------------------
A ``target_compatible_with`` compatibility helper that tags targets as
fundamentally incompatible with MCUs (e.g. Python tooling, Java code,
JavaScript/TypeScript code).

glob_dirs
=========
A starlark helper for performing ``glob()`` operations strictly on directories.

.. py:function:: glob_dirs(include: List[str], exclude: List[str] = [], allow_empty: bool = True) -> List[str]

   Matches the provided glob pattern to identify a list of directories.

   This helper follows the same semantics as Bazel's native ``glob()`` function,
   but only matches directories.

   Args:
     include: A list of wildcard patterns to match against.
     exclude: A list of wildcard patterns to exclude.
     allow_empty: Whether or not to permit an empty list of matches.

   Returns:
     List of directory paths that match the specified constraints.

match_dir
=========
A starlark helper for using a wildcard pattern to match a single directory

.. py:function:: match_dir(include: List[str], exclude: List[str] = [], allow_empty: bool = True) -> Optional[str]

   Identifies a single directory using a wildcard pattern.

   This helper follows the same semantics as Bazel's native ``glob()`` function,
   but only matches a single directory. If more than one match is found, this
   will fail.

   Args:
     include: A list of wildcard patterns to match against.
     exclude: A list of wildcard patterns to exclude.
     allow_empty: Whether or not to permit returning ``None``.

   Returns:
     Path to a single directory that matches the specified constraints, or
     ``None`` if no match is found and ``allow_empty`` is ``True``.

----------------
bazelrc snippets
----------------
Due to Bazel's ever-evolving nature, Pigweed requires some flags to work as
intended in bazel-based projects. The flags are split into two categories:
the first category is flags that **must** be set for Pigweed to work as
intended, and the other category is flags that are highly recommended to
improve your project's Bazel experience.

For more information, see how to
:ref:`docs-bazel-integration-add-pigweed-as-a-dependency`.

-----------------
Credential Helper
-----------------
``pw_build/cred_helper.sh`` is used by Google projects to support features
that require authentication.
