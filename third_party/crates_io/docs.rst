.. _module-pw_third_party_crates_io:

======================
crates.io dependencies
======================
This directory is where third party crate dependencies are managed.  There are
two sets of crates: ``./crates_no_std`` and ``./crates_std``.  These are
``cargo`` based "null" Rust libraries that exist solely to express which crates
to expose to Bazel via the a single unified hub.  ``./rust_crates`` is a Bazel
repository that exposes these crates to Bazel.

---------------
Getting started
---------------
Crates are split by whether they use ``std`` or ``no_std``. By default,
platforms are assumed to use ``std``. Embedded platforms typically need to add
``@pigweed//pw_build/constraints/rust:no_std`` from Pigweed's :ref:`Rust Bazel
constraints <module-pw_build-bazel_constraints-rust>` to the
``constraint_values`` attribute of the ``platform``.

.. code-block:: py

   platform(
       name = "mps2_an505",
       constraint_values = [
           "@pigweed//pw_build/constraints/arm:cortex-m33",
           "@pigweed//pw_build/constraints/rust:no_std",
           "@platforms//cpu:armv8-m",
           "@platforms//os:none",
       ],
   )

-------------------------
Vendoring your own crates
-------------------------
Pigweed's ``pw_rust_crates_extension`` Bazel module extension allows the
``@rust_crates`` repo used by Pigweed to be overridden so alternative crate
definitions may be used. This can be done by adding the following
to your project's ``MODULE.bazel``:

.. code-block:: py

   local_repository(name = "rust_crates", path = "build/crates_io")

   pw_rust_crates = use_extension(
       "@pigweed//pw_build:pw_rust_crates_extension.bzl",
       "pw_rust_crates_extension",
   )
   override_repo(pw_rust_crates, rust_crates = "rust_crates")

-------------------------------------
Adding a third party crate to Pigweed
-------------------------------------

1. Add the crate to either or both of ``./crates_no_std`` and ``./crates_std``
   by running the following command in the appropriate directory:

   .. code-block:: console

      $ cargo add <crate_name> --features <additional_features>

2. Run cargo deny:

   .. code-block:: console

      $ (cd crates_std; cargo deny check) && \
           (cd crates_no_std; cargo deny check)

   .. admonition:: Note

      Install with ``cargo install --locked cargo-deny`` if not already
      installed.

3. Update ``./rust_crates/alias_hub.BUILD`` by running the following command
   in the ``third_party/crates_io`` directory:

   .. code-block:: console

      $ cargo run -- --config config.toml > rust_crates/alias_hub.BUILD
