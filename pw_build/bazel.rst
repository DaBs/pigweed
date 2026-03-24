.. _module-pw_build-bazel:

=====
Bazel
=====
.. pigweed-module-subpage::
   :name: pw_build

Bazel is Pigweed's primary build system (as per :ref:`seed-0111`). This document
provides an overview of the Bazel-specific features provided by ``pw_build``.

.. grid:: 1 2 2 2
   :gutter: 2

   .. grid-item-card:: :octicon:`law` Rules & library helpers
      :link: bazel_rules
      :link-type: doc

      Custom Bazel rules for common needs.

   .. grid-item-card:: :octicon:`filter` Constraints
      :link: bazel_constraints
      :link-type: doc

      Platform and architecture-specific constraints.

   .. grid-item-card:: :octicon:`tools` Miscellaneous
      :link: bazel_helpers
      :link-type: doc

      Other Bazel-related starlark helpers, build targets, and resources.

.. toctree::
   :hidden:
   :maxdepth: 1

   Rules <bazel_rules>
   Compatibility constraints <bazel_constraints>
   Helper targets, macros, and templates <bazel_helpers>
