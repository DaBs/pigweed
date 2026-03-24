.. _docs-bazel-lockfile:

===========================
Managing the Bazel lockfile
===========================
The `Bazel lockfile <https://bazel.build/external/lockfile>`_
(``MODULE.bazel.lock``) is checked into the Pigweed repository. Some changes to
the Bazel build (especially changes to the ``MODULE.bazel`` file) may require
regenerating the lockfile. This document describes how to do this.

------------------------------
Simple case: automatic updates
------------------------------
In the simplest case, Bazel will automatically update the lockfile in the
course of your development work, as you execute commands like ``bazel build``.
You just need to commit the changes as part of your CL.

-------------------------------------
Simple case: outdated extension files
-------------------------------------
If you see the following error:

.. code-block:: none

   ERROR: MODULE.bazel.lock is no longer up-to-date because: One or more files
   the extension '@@rules_python+//python/extensions:pip.bzl%pip' is using have
   changed. Please run `bazel mod deps --lockfile_mode=update` to update your
   lockfile.

Run the following command to fix it:

.. code-block:: console

   bazelisk mod deps --lockfile_mode=update

---------------------------------
Complex case: platform-dependency
---------------------------------
Occasionally, some (transitive) dependency added to the build will be
platform-dependent. An example of this are Rust crates added using the
`crates.from_specs
<https://bazelbuild.github.io/rules_rust/crate_universe_bzlmod.html#from_specs>`__
module extension. If this happens, you will see errors like the following in CQ
builders running on platforms different from the one you developed on:

.. code-block:: console

   ERROR: The module extension
   'ModuleExtensionId{bzlFileLabel=@@rules_rust+//crate_universe:extension.bzl,
   extensionName=crate, isolationKey=Optional.empty}' for platform
   os:osx,arch:x86_64 does not exist in the lockfile.

What's going on here is that the exact versions of external dependencies that
enter the build vary depending on the OS or CPU architecture of the system
Bazel is running on.


Automated Tryjobs
=================
Fortunately, we have tryjob builders to help with the platform-specific
updates, though they are not run automatically. You must manually trigger them
through the Gerrit UI.

#. Upload your change to Gerrit.
#. Use the **Choose Tryjobs** link to add the lockfile tryjobs.

   * ``pigweed-linux-bazel-lockfile``
   * ``pigweed-mac-arm-bazel-lockfile``

   .. image:: https://www.gstatic.com/pigweed/gerrit_choose_tryjobs.png
      :width: 800
      :alt: Choose Tryjobs link in Gerrit

#. If any jobs fail, the post-failure **logs** step will contain a
   **git_diff.txt** file with the patch that is needed.

   You can easily apply the patch locally by running the command just below,
   updating the ``$BBID`` value to the BuildBucket ID for your failure:

   .. code-block:: console

      $ curl https://logs.chromium.org/logs/pigweed/buildbucket/cr-buildbucket/$BBID/+/u/lockfile_check/logs/git_diff.txt/git_diff.txt?format=raw | git apply

   .. tip::
      Here is an example of constraints tryjob failure:

      * https://ci.chromium.org/b/8690767152267268145

      The ``8690767152267268145`` in the URL is the BuildBucket ID.

   You can then upload a new patchset to Gerrit with the updates from the
   failing builds. Note that failures from each of the tryjobs may result in
   overlapping changes to the ``MODULE.bazel.lockfile``. You may want to only
   apply the changes from one of the targets, then re-run to get any additional
   changes needed from the remaining targets.

#. If the job passes, the lockfile is already up to date on this host platform
   and no patching is necessary!
