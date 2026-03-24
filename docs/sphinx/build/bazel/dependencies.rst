.. _docs-bazel-dependencies:

===============================================
Manage external dependencies in a Bazel project
===============================================
Bazel is both a build system, and a package manager. This combination is
extremely powerful, but does introduce some complexity. This page explains
the core constructs you'll need to understand to get started with a Bazel
project.

.. _docs-bazel-dependencies-declare:

-----------------
Declare a project
-----------------
In Bazel, projects are defined as `Bazel Modules
<https://bazel.build/external/module>`__. A project can be started with a single
line in a ``MODULE.bazel`` file.

**MODULE.bazel**

.. code-block:: python

   module(name = "my_project", version = "0.0.1")

.. note::

   You may find some Bazel projects that use a ``WORKSPACE`` file to set up a
   project. ``WORKSPACE`` is a deprecated, legacy system that is inferior to
   using ``MODULE.bazel``.

.. _docs-bazel-dependencies-overview:

--------
Overview
--------
One of Bazel's most powerful features is that in addition to being a build
system, it is also a package manager. Before tackling any significant project
in Bazel, it's important to understand how to use—and properly constrain—Bazel's
package management system.

Bazel has **three** major systems for managing external resources:

- :ref:`docs-bazel-dependencies-overview-modules`
- :ref:`docs-bazel-dependencies-overview-extensions`
- :ref:`docs-bazel-dependencies-overview-repository-rules`

Each of these systems play a crucial role in how projects are structured, and
how various projects interact with each other. Each of these allow different
resources (source code, Bazel extensions, or arbitrary binary data) to be
exposed as sectioned-off `repositories
<https://bazel.build/external/overview#repository>`__.

If Bazel needs a resource that is not included in the source checkout, it should
be expressed in a ``MODULE.bazel`` somewhere.

.. _docs-bazel-dependencies-overview-modules:

Bazel modules
=============
`Bazel Modules <https://bazel.build/external/module>`__ are the de-facto unit
for a project. Bazel modules can express dependencies on other modules,
configure/use module extensions, and instantiate any independent
`repository_rule
<https://bazel.build/rules/lib/globals/bzl.html#repository_rule>`__ they need to
function properly.

There are some important things you should know about modules:

#. Under normal circumstances, there is a **single**, unified definition of each
   Bazel module. Because of this, version resolution across transitive module
   dependencies is used to determine which version is actually used. Once a
   version is decided, all Bazel modules will use the same definition.
#. Module dependencies propagate. If you depend on a module that depends on
   other modules, you inherit the same ``bazel_dep`` entries in their
   ``MODULE.bazel``. These transitive module dependencies are not, however,
   directly accessible from your project unless you add an explicit
   ``bazel_dep`` on them.
#. Modules come from `registries <https://bazel.build/external/registry>`__.
   The `Bazel Central Registry (BCR) <https://registry.bazel.build/>`__ is the
   most prominent registry, and is the default registry enabled in Bazel.

Languages and integrations
--------------------------
While Bazel itself natively supports building a few programming languages, the
ecosystem is gradually migrating everything to use Bazel's extensibility
primitives to offer language support and other integrations.

Just like when you declare your own project in a ``MODULE.bazel``, build support
for various programming languages are offered through other projects that have
done the same. Here's some examples:

* `rules_cc <https://registry.bazel.build/modules/rules_cc>`__: C/C++ support.
* `rules_rust <https://registry.bazel.build/modules/rules_rust>`__: Rust
  support.
* `rules_python <https://registry.bazel.build/modules/rules_python>`__: Python
  support.

In addition to this, various modules in the ecosystem provide fundamental Bazel
building blocks to address common needs. A few of the most prominent ones are:

* `platforms <https://registry.bazel.build/modules/platforms>`__: Canonical
  definitions for ecosystem-wide platform configuration axis.
* `bazel_skylib <https://registry.bazel.build/modules/bazel_skylib>`__: Utility
  module with an assortment of helpers and general-purpose rules.

To do anything useful in Bazel, you'll need to use community-maintained modules.

Example: Using rules_cc
-----------------------
Using ``rules_cc`` requires just a single line in your ``MODULE.bazel`` file:

**MODULE.bazel**

.. code-block:: python

   module(name = "my_project", version = "0.0.1")

   bazel_dep(name = "rules_cc", version = "0.2.4")

After that, you can reference things from ``rules_cc`` with the form:
``@rules_cc//path/to/directory:file_or_target``. There are some important
things you should know about modules:

When to use a module
--------------------
* You're starting a new project.
* You need to express dependency relationships on other Bazel projects.
* You want to define a repository rule or extension that can be used by other
  projects.

.. _docs-bazel-dependencies-overview-extensions:

Module extensions
=================
Bazel modules use `module extensions <https://bazel.build/external/extension>`__
to configure and unify more complex external repository definitions. One very
common example use case for a module extension is bridging to  external package
managers like `pip <https://pip.pypa.io/en/stable/>`__ or `cargo
<https://doc.rust-lang.org/cargo/>`__. The underlying mechanism for module
extensions is more widely applicable, though.

The architecture of an extension is roughly as follows:

* An extension defines a configuration interface. From a user perspective, this
  always looks like method calls on an object that specify parameters exposed by
  the extension. Each user-facing method is technically called a `tag class
  <https://bazel.build/rules/lib/globals/bzl#tag_class>`__.
* Users of the extension inject metadata by calling the methods exposed by the
  extension.
* The implementation of the module extension scans the injected metadata from
  all modules, and through coding and algorithms generates one or more
  repositories.
* Bazel modules can then expose any repository generated by the extension to
  the current module/project scope.

.. _docs-bazel-dependencies-overview-extensions-example:

Example: libusb
---------------
The following extension configures the ``libusb`` extension offered by the
``rules_libusb`` module:

**MODULE.bazel**

.. code-block:: py

   module(name = "my_project", version = "0.0.1")

   # In this example, the extension logic lives in `@rules_libusb`.
   bazel_dep(name = "rules_libusb", version = "0.1.0-rc2")

   # Get access to the extension so it can be configured and used.
   libusb = use_extension("@rules_libusb//:extensions.bzl", "libusb")

   # Inject metadata that declares a need for a minimum libusb version of
   # 1.0.27 into the `source_release` tag class.
   #
   # NOTE: Sometimes this isn't necessary, and you can just rely on how other
   #       modules have configured the extension.
   libusb.source_release(min_version = "1.0.27")

   # Expose the repository named `libusb` that is generated by the libusb
   # extension.
   #
   # NOTE: If your project just needs to configure the extension but doesn't
   #       need to directly reference @libusb, you can skip this.
   use_repo(libusb, "libusb")

.. _docs-bazel-dependencies-overview-extensions-usage:

When to use a module extension
------------------------------

* You need to configure an external package manager that is integrated into
  another project (like ``rules_python`` for ``pip``, or ``rules_rust`` for
  ``cargo``).
* You need to access a shared external package or resource that is defined by
  a module extension.
* You want to offer a configurable interface that results in one or more
  repositories that contain downloaded resources or generated files that respond
  to metadata injected by all users of the extension.
* You want an external resource or generated repository to be shared and
  accessible from multiple projects.

.. _docs-bazel-dependencies-overview-repository-rules:

Individual repository rules
===========================
While repository rules are one of the building blocks for module extensions,
they may also be directly used in a ``MODULE.bazel`` file through
`use_repo_rule()
<https://bazel.build/rules/lib/globals/module.html#use_repo_rule>`__. This
is the only mechanism that declares a private resource. The repository
will still work when your module is another module's dependency, but no other
module will be able to *directly* reference its contents.

.. caution::

   If two Bazel modules make *exactly identical* repositories created through
   ``use_repo_rule()``, they will exist as parallel duplicates.

Example: http_archive
---------------------

.. code-block:: py

   module(name = "my_project", version = "0.0.1")

   # Expose the repository rule for use.
   http_archive = use_repo_rule("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

   # Create an isolated repository containing a toolchain.
   http_archive(
       name = "arm_gcc_linux-aarch64",
       build_file = "//bazel/toolchain:gcc_arm_none_eabi.BUILD",
       sha256 = "8fd8b4a0a8d44ab2e195ccfbeef42223dfb3ede29d80f14dcf2183c34b8d199a",
       strip_prefix = "arm-gnu-toolchain-13.2.Rel1-aarch64-arm-none-eabi",
       url = "https://developer.arm.com/-/media/Files/downloads/gnu/13.2.rel1/binrel/arm-gnu-toolchain-13.2.rel1-aarch64-arm-none-eabi.tar.xz",
   )

When to use individual repository rules
---------------------------------------

* You want an isolated resource that can only be directly referenced
  from within the module that called the repository rule.

---------------------------
Debug external repositories
---------------------------

Use a local version of an external resource/repository
======================================================
For Bazel modules:

* Build with
  ``--override_module=dep_to_override=/path/to/local/dep_to_override``

For repositories created through a module extension or ``use_repo_rule()``:

* Find the `canonical name
  <https://bazel.build/external/overview#canonical-repo-name>`__:

  .. code-block:: console

     $ bazel mod dump_repo_mapping "" | jq '.my_repo_to_override'
     +_repo_rules5+my_repo_to_override

* Build with
  ``--override_repository=+_repo_rules5+my_repo_to_override=/path/to/local/my_repo_to_override``

If you want to check in overrides because you have a monorepo project layout,
consider setting up a local in-tree Bazel registry.

Locate the repository created by Bazel
======================================
In the ``external`` subdirectory of the ``output_base``. You can list the
contents with the following command:

.. code-block:: console

   $ ls $(bazel info output_base)/external
   rules_rust++rust+rust_toolchains
   rules_shell+
   rules_shell++sh_configure+local_config_shell
   rules_uv+
   stardoc+
   toolchains_protoc+

.. caution::

   Do not modify the contents of these directories. Doing so puts Bazel into
   an ill-defined state that may result in confusing errors.

Helpful flags
=============
* ``--override_module``: Overrides a module, pointing to a local path.
* ``--override_repository``: Overrides a repository using its `canonical
  repository name <https://bazel.build/external/overview#canonical-repo-name>`__
  to point to a local path.
* ``--registry``: Use a different registry for resolving modules. Can be a local
  path.
* ``--sandbox_debug``: If the build fails, retain Bazel's sandboxed view of the
  failing action. This is extremely useful for debugging incorrect file paths,
  or files that were not properly enumerated as inputs to the build.

---------------------
Avoid common pitfalls
---------------------

Prevent multiple definitions of the same repository
===================================================
In the situation where the same repository name is used by a bazel module,
module extension, and repository created via ``use_repo_rule()``, all
definitions will happily coexist **in parallel**. Be careful, as this
can cause problems where the same resource enters the build through multiple
paths.

Don't place resources in an ``external/`` directory
===================================================
Do not place any Bazel resources in ``external/``. That directory name is
silently reserved by Bazel. (`bazelbuild/bazel#4508
<https://github.com/bazelbuild/bazel/issues/4508>`__)

Properly ignore nested projects
===============================
Without additional configuration, Bazel will fail with inscrutable errors
when a Bazel module lives within another module. You can use a ``.bazelignore``
file to tell Bazel to ignore specific subdirectories. If you need wildcard
expressions in ignore patterns, use `ignore_directories()
<https://bazel.build/rules/lib/globals/repo#ignore_directories>`__ in a
``REPO.bazel`` file placed adjacent to your project's ``MODULE.bazel`` file.

---------
Resources
---------

* `MODULE.bazel reference <https://bazel.build/rules/lib/globals/module>`__
* `External dependencies overview <https://bazel.build/external/overview>`__
* `Bazel modules <https://bazel.build/external/module>`__
* `Bazel registries <https://bazel.build/external/registry>`__
* `Module extensions <https://bazel.build/external/extension>`__
* `Vendor mode <https://bazel.build/external/vendor>`__
