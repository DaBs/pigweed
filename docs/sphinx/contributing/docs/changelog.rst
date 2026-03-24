.. _inverted pyramid: https://en.wikipedia.org/wiki/Inverted_pyramid_(journalism)
.. _relation chain: https://gerrit-review.googlesource.com/Documentation/rest-api-changes.html#get-related-changes

.. _contrib-changelog:

=================
Changelog updates
=================
This page shows upstream Pigweed maintainers how to :ref:`update the Pigweed
changelog <contrib-changelog-quickstart>` and :ref:`understand the changelog
automation <contrib-changelog-impl>`.

.. _contrib-changelog-quickstart:

----------
Quickstart
----------
#. Get a `Gemini API key <https://ai.google.dev/gemini-api/docs/api-key>`_.

   .. note::

      Googlers should follow their team's process for getting an API key.

#. Export ``GEMINI_API_KEY`` as an OS environment variable.

#. Run the changelog automation:

   .. code-block:: console

      ./pw changelog --year=<YYYY> --month=<MM>

   Replace ``<YYYY>`` with a valid year and ``<MM>`` with a valid month.

   This step should take 5-10 minutes to complete.

#. Before changing any content, push the generated content to Gerrit as-is so
   that we have a record of it.

#. Edit the content:

   * Review the stories that are commented out. Verify that they are
     uninteresting and low-impact. Delete the comments in that case. Conversely,
     if a commented story seems important, uncomment it.

     .. note::

        We use Gemini API to guesstimate whether a story has major user-facing
        impact or not. The commented-out stories are the ones that Gemini API
        determined to have low user-facing impact.

   * Remove duplicate stories.

     .. note::

        Duplication happens because we group commits based on Gerrit `relation
        chain`_ and Buganizer issue data, and this data sometimes creates
        overlapping connections between commits.

#. Preview the docs and verify that the new content builds and renders
   correctly:

   .. code-block:: console

      bazelisk run //docs:serve

#. For any given story, if you're unsure whether the generated content is
   accurate, tag a relevant code owner for review.

.. _contrib-changelog-impl:

-------------------------
Automation implementation
-------------------------
This section provides extra context for upstream Pigweed maintainers who need
to modify the changelog automation.

.. _contrib-changelog-impl-goals:

Goals
=====
There are many types of changelog in the world. Here are the goals of this
particular changelog:

* **100% automation**. We aim to automate 100% of the changelog authoring
  process. Any teammate on the core Pigweed team should be able to run a single
  command and get back a changelog update that's ready to publish. If the
  changelog update process is toilsome, we're unlikely to consistently publish
  an update every month.

* **Comprehensive, fast, and reliable**. We need strong guarantees that every
  commit that occurred within the specified timeframe is analyzed. You should
  be able to finish the automation in a reasonable amount of time (15 minutes or
  less). You should be able to fire off the automation and trust that it will
  finish reliably.

* **Stories, not commits**. Related commits should be grouped into "stories"
  that summarize the larger body of work. E.g. if there are 6 commits related
  to enabling IPC in ``pw_kernel``, there should be one story along the lines
  of ``pw_kernel: Initial IPC support`` and that story should link to the 6
  related commits.

* **User-facing stories only**. If it's not relevant to a downstream project
  that depends on Pigweed, it doesn't belong in the changelog.

* **Interesting, not comprehensive**. Our readers are not interested in a
  comprehensive digest of how Pigweed has changed over the given timeframe. For
  that, they can read the commit log. The purpose of the changelog is to spread
  awareness that Pigweed is continuing to evolve and to highlight the most
  impactful larger bodies of work that are happening.

* **Inverted pyramid**. Following the `inverted pyramid`_ principle from
  journalism, the most important information (i.e. the most newsworthy stories)
  should be shown first.

.. _contrib-changelog-impl-arch:

Architecture
============
The changelog automation is orchestrated through a Python script. Here's the
general workflow:

#. Gather all commits that occurred within the specified month.

#. Group the commits into stories using Gerrit `relation chain`_ and Buganizer
   issue data.

#. Use Gemini API to draft content for each story.

#. Transform the content into reStructuredText.

See the ``__main__`` function in :cs:`docs/sphinx/changelog/py/__main__.py`
for more details.
