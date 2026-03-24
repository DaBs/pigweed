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
"""Provides data related to a single Git commit."""

import json
from string import Template
import time
from typing import TypedDict

from google import genai
from pw_cli.git_repo import GitRepo
import requests

import helpers


# Prompt provided to Gemini to guesstimate whether the commit has any
# user-facing impact. Commits with no user-facing impact are dropped.
_is_user_facing_template = Template(
    """
We are drafting an update for the [Pigweed](https://pigweed.dev) changelog.
Help us determine if the following commit has user-facing impact.

* The intended audience of the changelog is a software engineer in a downstream
  project that relies on Pigweed. If the commit only affects the upstream
  Pigweed codebase and has no affect on the intended audience, then the commit
  has no user-facing impact.

* The commit must add a feature, fix a bug, or change an API in order for it
  to be considered user-facing. A commit that only changes documentation or
  tests is not considered user-facing.

Commit message:

```
$message
```

Commit diff:

```
$diff
```
"""
)


class GerritError(Exception):
    """Raised when a commit without a Gerrit URL is detected."""


class RevertError(Exception):
    """Raised when a revert commit or reverted commit is detected."""


class RollError(Exception):
    """Raised when a roll commit is detected."""


def _fetch_from_gerrit(url: str) -> dict:
    """Fetch data from Gerrit REST API. Includes retry logic."""
    rate_limit_code = 429
    delay = 30
    done = False
    while not done:
        response = requests.get(url, timeout=10)
        if response.status_code == rate_limit_code:
            print(
                "[INF] Retrying Gerrit API request in {} seconds: {}".format(
                    delay, url
                )
            )
            time.sleep(delay)
            continue
        done = True
    return _decode(response.text)


def _decode(text: str) -> dict:
    """Remove the XSSI prevention prefix from Gerrit REST API responses."""
    # https://gerrit-review.googlesource.com/Documentation/rest-api.html#output
    magic_prefix = ")]}'\n"
    if text.startswith(magic_prefix):
        text = text.replace(magic_prefix, "")
    return json.loads(text)


def _diff(diff: str) -> str | None:
    """Generate a summary for large diffs."""
    # If the diff is larger than Gemini's input token limit, there's
    # nothing we can do with it.
    if not helpers.is_under_token_limit(diff):
        return None
    # Summarize diffs larger than 100K tokens.
    very_large_diff = 100000
    if helpers.count_tokens(diff) > very_large_diff:

        class Response(TypedDict):
            summary: str

        config = genai.types.GenerateContentConfig(
            response_mime_type="application/json",
            response_schema=Response,
        )
        model = "gemini-2.5-flash"
        prompt = "Summarize the following diff:\n\n{}".format(diff)
        data = helpers.generate(prompt, model, config)
        if data is None:
            return None
        return data["summary"]
    # Use the diff directly if it's smaller than 100K tokens.
    return diff


class Commit:  # pylint: disable=too-many-instance-attributes
    """A single Git commit."""

    def __init__(self, sha: str, repo: GitRepo):
        self.sha = sha
        self.message = repo.commit_message(sha)
        self.title = self.message.splitlines()[0]
        self.diff = _diff(repo.diff("{}^!".format(sha)))
        self.date = repo.commit_date(sha)
        self.issues = repo.commit_issues(sha)
        self.tags = self._parse_tags()
        if "roll" in self.tags:
            raise RollError
        self.gerrit = repo.commit_review_url(sha)
        if self.gerrit is None:
            raise GerritError
        self._change_number = self.gerrit.rsplit("/", 1)[-1]
        if self._is_revert() or self._is_reverted():
            raise RevertError
        self.relation_chain = self._get_relation_chain()
        self.is_user_facing = self._is_user_facing()
        print('[INF] Initialized Commit instance: "{}"'.format(self.title))

    def _parse_tags(self) -> list[str]:
        """Parse the tags from the first line of the commit message.

        E.g. given `pw_foo: Bar the baz`, the tag is `pw_foo`. Given
        `pw_{foo,bar,baz}: Fix the bork` the tags are `pw_foo`, `pw_bar`, and
        `pw_baz`.
        """
        if ":" not in self.title:
            return []
        raw_start = 0
        raw_end = self.title.index(":")
        raw = self.title[raw_start:raw_end]
        if not raw.startswith("pw_{"):
            return [raw]
        modules_start = raw.index("{") + 1
        modules_end = raw.index("}")
        modules = raw[modules_start:modules_end].split(",")
        return ["pw_{}".format(module.strip().lower()) for module in modules]

    def _is_revert(self) -> bool:
        """Check if this change reverts another change."""
        url = "https://pigweed-review.googlesource.com/changes/{}".format(
            self._change_number
        )
        data = _fetch_from_gerrit(url)
        return "revert_of" in data

    def _is_reverted(self) -> bool:
        """Check if this change has been reverted."""
        base = "https://pigweed-review.googlesource.com"
        url = "{}/changes/?q=revertof:{}".format(base, self._change_number)
        data = _fetch_from_gerrit(url)
        return len(data) > 0

    def _get_relation_chain(self) -> list[str]:
        """Get the parents/children of this commit."""
        # https://gerrit-review.googlesource.com/Documentation/concept-changes.html#related-changes
        # https://gerrit-review.googlesource.com/Documentation/rest-api-changes.html#get-related-changes
        base = "https://pigweed-review.googlesource.com"
        url = "{}/changes/{}/revisions/current/related".format(
            base, self._change_number
        )
        data = _fetch_from_gerrit(url)
        changes = data["changes"]
        # SHA of first commit in relation chain at index 0, SHA of second commit
        # in relation chain at index 1, etc.
        relation_chain = []
        for item in changes:
            change_id = item["change_id"]
            url = "{}/changes/{}?o=CURRENT_REVISION".format(base, change_id)
            change = _fetch_from_gerrit(url)
            if not change["status"] == "MERGED":
                continue
            sha = change["current_revision"]
            relation_chain.append(sha)
        return relation_chain

    def _is_user_facing(self) -> bool:
        """Guesstimate whether the commit has user-facing impact."""

        class Response(TypedDict):
            user_facing: bool

        config = genai.types.GenerateContentConfig(
            response_mime_type="application/json",
            response_schema=Response,
        )
        model = "gemini-2.5-flash"
        prompt = _is_user_facing_template.substitute(
            message=self.message, diff=self.diff
        )
        data = helpers.generate(prompt, model, config)
        if data is None:
            return False
        return data["user_facing"]
