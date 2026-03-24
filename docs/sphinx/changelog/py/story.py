# Copyright 2025 The Pigweed Authors
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.
"""A group of related commits.

In the changelog we don't discuss individual commits. We discuss "stories"
which are groups of related commits representing a larger body of work.

Commits are related when they're in the same relation chain or reference the
same issues.
"""

import random
import string
from string import Template
from typing import TypedDict

from google import genai

from commit import Commit
import helpers


# Enums related to the level of user-facing impact.
LOW_IMPACT = 0
MEDIUM_IMPACT = 1
HIGH_IMPACT = 2


# Prompt provided to Gemini API to create a summary of the story.
_summarize_template = Template(
    """
Draft a summary for the following group of commits. The summary will be used in
the [Pigweed](https://pigweed.dev) changelog.

The intended audience of the changelog is a software engineer working in a
downstream project that depends on Pigweed. Your changelog summary must
concisely explain the user-facing impact of the topic.

Note: In the context of Pigweed, the term `module` must only be used when
discussing one of the libraries or products that start with `pw_*`, e.g.
`pw_rpc`, `pw_string`, etc.

Output guidelines:

* `title`: a pithy H2-level section heading, no more than 60 characters.
  It must only use plaintext. It must not contain any backticks. It must use
  the format `{tag}: {description}` where `tag` is a Pigweed module
  (e.g. `pw_tokenizer`) or area (e.g. `Bazel`). If the story is related to
  multiple modules or areas, pick the module or area that is most relevant.
  Examples:

  * pw_kernel: Flexible RISC-V timer selection

  * pw_tokenizer: Token domains now supported

* `body`: a 1-3 sentence explanation of the topic. It must use double
  backticks to represent inline code formatting. It must not contain any
  URLs. You must only mention the most important user-facing changes.
  Do not mention secondary, trivial changes. If making a subjective statement
  about why the change is an improvement, you must use extremely humble and
  fact-based language. Examples:

  * A new ``pw::log_tokenized::ParseFields`` C++ function is now available for
    parsing metadata fields from tokenized log format strings. This enables
    downstream projects to easily extract key-value pairs from structured logs,
    using configurable delimiters such as ``■`` and ``♦``.

  * The thread priority system in ``pw_thread`` now supports 64-bit integer
    types for thread priorities. This enhancement allows for a broader and more
    granular range of priority values, accommodating systems with larger
    priority scales.

  * ``pw_kernel`` now provides a configurable interface for the RISC-V ``MTVEC``
    register. This allows projects to explicitly define whether exceptions are
    handled in ``Direct`` or ``Vectored`` mode during early kernel
    initialization, offering greater flexibility for system-level exception
    management.

* `highlight`: a 1 sentence summary of the change. It MUST be 1 sentence only
  and follow the formatting guidelines for `body`.

General writing guidelines:

* When referring to Pigweed modules, just use the module name. E.g.
  "The ``pw_clock_tree_mcuxpresso`` module now includes …" should be rewritten
  to "``pw_clock_tree_mcuxpresso`` now includes …``".

* Don't refer to "you" or "engineers" when addressing the user. Instead,
  use "projects" or "downstream projects".

* Don't use possessive ``'s`` after inline code. E.g. "``pw_rpc``'s ``Send``
  method``" should be rewritten to "the ``Send`` method of ``pw_rpc``". In many
  contexts you can omit the reference to the module completely and just say
  "the ``Send`` method".

* Code items such as names of files, methods, classes, CLI tools, etc. must
  always be formatted as inline code by wrapping them in double backtick (``)
  characters. Do not use a single backtick (`) character! The single backtick
  formats these items as italic.

Here are the target commits that occurred during the target timeframe.

```
$targets
```

Here are related commits that occurred in a previous timeframe. These commits
are only provided for background context. They should not be mentioned in
your response.

```
$context
```
"""
)


# Prompt provided to Gemini API to guesstimate user-facing impact.
_prioritize_template = Template(
    """
We are drafting an update for the [Pigweed](https://pigweed.dev) changelog.
The intended audience of the changelog is a software engineer working in a
downstream project that depends on Pigweed. Help us determine the importance
of the following story.

Output `0`, `1`, or `2` according to these guidelines:

* `0`: Documentation updates, testing changes, build system changes,
  internal refactors, version bumps, and other run-of-the-mill updates.

* `1`: Normal bug fixes, moderate reliability improvements, warnings about
  edge cases, minor new features, moderate reliability or performance
  improvements, deprecations of unpopular APIs, and other updates that
  probably only appeal to a subset of downstream projects.

* `2`: Critical bug fixes, huge reliability or performance improvements, 
  critical bug fixes, the introduction of new foundational APIs, and other
  extremely noteworthy updates that will be interesting to most downstream
  projects.

```
$title

$body
```
"""
)


class Story:
    """A group of related commits representing a larger body of work."""

    def __init__(self, commits: list[Commit]):
        alphabet = string.ascii_lowercase + string.digits
        # Generate a random ID for the story so that we can create deeplinks to
        # it in the reStructuredText.
        self.id = ''.join(random.choices(alphabet, k=8))
        self.commits = commits
        self.title = ""
        self.body = None
        self.highlight = None
        self.impact = None

    def has_related_issues(self, straggler: Commit) -> bool:
        """Check if a straggler commit related to this story's commits.

        If the straggler references issue A, and one of the commits in this
        story also references issue A, then the straggler is assumed to be
        related to this story.
        """
        if straggler.issues is None:
            return False
        for commit in self.commits:
            if commit.issues is None:
                continue
            for issue in straggler.issues:
                if issue in commit.issues:
                    return True
        return False

    def add(self, commit: Commit) -> None:
        """Add a commit to this story."""
        if commit in self.commits:
            return
        self.commits.append(commit)

    def summarize(self, year: int, month: int) -> None:
        """Summarize this group of commits into a story."""

        class Response(TypedDict):
            title: str
            body: str
            highlight: str

        config = genai.types.GenerateContentConfig(
            response_mime_type="application/json",
            response_schema=Response,
        )
        model = "gemini-2.5-flash"
        target_commits = [
            commit
            for commit in self.commits
            if commit.date.year == year and commit.date.month == month
        ]
        context_commits = [
            commit
            for commit in self.commits
            if commit.date.year != year or commit.date.month != month
        ]
        targets = ""
        for target_commit in target_commits:
            diff = "" if target_commit.diff is None else target_commit.diff
            targets += "{}\n\n{}\n\n".format(target_commit.message, diff)
        context = ""
        for context_commit in context_commits:
            diff = "" if context_commit.diff is None else context_commit.diff
            context += "{}\n\n{}\n\n".format(context_commit.message, diff)
        prompt = _summarize_template.substitute(
            targets=targets, context=context
        )
        data = helpers.generate(prompt, model, config)
        self.title = "" if data is None else data["title"]
        self.body = None if data is None else data["body"]
        self.highlight = None if data is None else data["highlight"]
        print('[INF] Generated story: "{}"'.format(self.title))

    def prioritize(self) -> None:
        """Guesstimate the user-facing impact of this story."""

        class Response(TypedDict):
            impact: int

        config = genai.types.GenerateContentConfig(
            response_mime_type="application/json",
            response_schema=Response,
        )
        model = "gemini-2.5-flash"
        prompt = _prioritize_template.substitute(
            title=self.title, body=self.body
        )
        data = helpers.generate(prompt, model, config)
        self.impact = None if data is None else data["impact"]
        print(
            '[INF] Guesstimated impact for "{}": {}'.format(
                self.title, self.impact
            )
        )
