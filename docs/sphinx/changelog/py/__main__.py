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
"""CLI entrypoint and main orchestrator of the changelog automation."""

from datetime import datetime
from pathlib import Path
import os
import sys
from threading import Thread

from pw_cli.git_repo import GitRepo
from pw_cli.git_repo import GitError
from pw_cli.tool_runner import BasicSubprocessRunner

from commit import Commit
from commit import GerritError
from commit import RevertError
from commit import RollError
from story import HIGH_IMPACT
from story import Story


def _populate_commit(
    sha: str, repo: GitRepo, commits: dict[str, Commit]
) -> None:
    """Initialize a single commit and add it to the commits list."""
    # It's possible that the commit is already initialized because we
    # populate the commit over multiple passes.
    if sha in commits:
        return
    try:
        commits[sha] = Commit(sha, repo)
    # GitRepo sometimes can't find data related to a commit and throws
    # this error. Not sure how, since the list of SHAs come from GitRepo. It
    # happens so rarely in practice that we can just ignore those commits.
    except GitError:
        return
    except GerritError:  # Ignore commits that don't have Gerrit review URLs.
        return
    except RevertError:  # Ignore revert/reverted commits.
        return
    except RollError:  # Ignore roll commits.
        return


def _populate_commits_in_parallel(
    shas: list[str], repo: GitRepo, commits: dict[str, Commit]
) -> None:
    """Initialize the commits with multithreading."""
    threads = []
    for sha in shas:
        if sha in commits:
            continue
        thread = Thread(
            target=_populate_commit,
            args=(
                sha,
                repo,
                commits,
            ),
        )
        threads.append(thread)
        thread.start()
    for thread in threads:
        thread.join()


def _init_commits(year: int, month: int, repo: GitRepo) -> dict[str, Commit]:
    """Create Commit instances for every commit within the specified month."""
    spec = "%Y-%m-%dT%H:%M:%S"
    start_timestamp = "{}-{}-01T00:00:00".format(year, month)
    start = datetime.strptime(start_timestamp, spec)
    end_year = year + 1 if month == 12 else year
    end_month = 1 if month == 12 else month + 1
    end_timestamp = "{}-{}-01T00:00:00".format(end_year, end_month)
    # end_timestamp = "{}-{}-02T00:00:00".format(year, month)
    end = datetime.strptime(end_timestamp, spec)
    shas = [
        # Always use the full/long SHA everywhere.
        repo.commit_hash(sha, short=False)
        for sha in repo.commits(start, end)
    ]
    commits: dict[str, Commit] = {}
    _populate_commits_in_parallel(shas, repo, commits)
    # Do another pass to populate commits that were found in the relation
    # chains. These will be used as background context when drafting stories.
    for sha, commit in commits.items():
        for related_sha in commit.relation_chain:
            if related_sha in shas:
                continue
            shas.append(related_sha)
    _populate_commits_in_parallel(shas, repo, commits)
    return commits


def _group_commits_into_stories(commits: dict[str, Commit]) -> list[Story]:
    """Group commits into "stories" based on relation chain and issue data."""
    # Gerrit provides an API for accessing a commit's relation chain. This is
    # usually a reliable, high-signal way of grouping changes. I.e. commits in
    # the same relation chain are usually highly related to each other. The only
    # downside is that the relation chains sometimes have duplication. E.g.
    # commit A introduces a feature. Commit B is a child of A that implements
    # the feature in pw_foo. Commit C is also a child of A that implements the
    # feature in pw_bar. You'll have one relation chain containing [A, B] and
    # another one containing [A, C].
    # https://gerrit-review.googlesource.com/Documentation/concept-changes.html#related-changes
    relation_chains = []
    stragglers = []  # Commits that don't seem related to any other commits.
    for sha in commits:
        commit = commits[sha]
        # Ignore SHAs that don't have an associated Commit instance. This could
        # happen if the commit was reverted.
        chain = [sha for sha in commit.relation_chain if sha in commits]
        if len(chain) == 0:
            stragglers.append(sha)
            continue
        # Length of chain 1 can happen when only 1 commit in a chain merged
        # and all other commits in the chain were abandoned, reverted, etc.
        if len(chain) == 1:
            stragglers.append(sha)
            continue
        if chain in relation_chains:
            continue
        relation_chains.append(chain)
    # De-duplicate the relation chains. If chain_1 = [A, B] and
    # chain_2 = [A, B, C, D] then we can drop chain_1 because it's a subset.
    dupes = []
    for index_a, chain_a in enumerate(relation_chains):
        for index_b, chain_b in enumerate(relation_chains):
            if index_a == index_b:
                continue
            if set(chain_a) <= set(chain_b):
                dupes.append(index_a)
    # Remove the dupes from the relation chains.
    relation_chains = [
        chain for i, chain in enumerate(relation_chains) if i not in dupes
    ]
    # Start creating stories based off relation chains.
    stories = []
    for chain in relation_chains:
        story_commits = [commits[sha] for sha in chain]
        story = Story(story_commits)
        stories.append(story)
    # Try to add straggler SHAs to stories by looking for shared issues. E.g.
    # if straggler A references issue 12345, and commit B from the story
    # references the same issue, then we assume that straggler A is related to
    # this story.
    adopted = []
    for sha in stragglers:
        straggler_commit = commits[sha]
        for story in stories:
            if story.has_related_issues(straggler_commit):
                story.add(straggler_commit)
                adopted.append(sha)
    # Remove the stragglers that have been "adopted".
    stragglers = [sha for sha in stragglers if sha not in adopted]
    # Remove stragglers that are non-user-facing. This is determined with
    # Gemini API.
    stragglers = [sha for sha in stragglers if commits[sha].is_user_facing]
    # Try to organize the remaining stragglers based on shared issues. At this
    # point there are still some stragglers that were not related to any of the
    # stories. It's possible that two or more stragglers reference the same
    # issue (that was not referenced in any story). In that case we can assume
    # that those two stragglers commits are related to each other and constitute
    # a story.
    issues: dict[str, list[str]] = {}
    for sha in stragglers:
        straggler = commits[sha]
        if straggler.issues is None:
            continue
        for issue in straggler.issues:
            if str(issue) not in issues:
                issues[str(issue)] = []
            issues[str(issue)].append(sha)
    for _, shas in issues.items():
        story_commits = [commits[sha] for sha in shas]
        story = Story(story_commits)
        stories.append(story)
    return stories


def _summarize_stories_in_parallel(
    stories: list[Story], year: int, month: int
) -> None:
    """Draft content for all stories with Gemini API."""
    threads = []
    for story in stories:
        thread = Thread(
            target=story.summarize,
            args=(
                year,
                month,
            ),
        )
        threads.append(thread)
        thread.start()
    for thread in threads:
        thread.join()


def _prioritize_stories_in_parallel(stories: list[Story]) -> None:
    """Guesstimate user-facing impact of each story with Gemini API."""
    threads = []
    for story in stories:
        thread = Thread(target=story.prioritize)
        threads.append(thread)
        thread.start()
    for thread in threads:
        thread.join()


def _write(stories: list[Story], year: int, month: int, path: Path) -> None:
    """Transform the data into reST."""

    def date_spec() -> str:
        return "%Y-%m-%dT%H:%M:%S"

    def normalize_month(month: int) -> str:
        """Provide month as a 2-character string e.g. "09"."""
        spec = date_spec()
        date = datetime.strptime("2025-{}-01T00:00:00".format(month), spec)
        return date.strftime("%m")

    def normalize_year(year: int) -> str:
        """Provide year as a 4-character string e.g. "2025"."""
        spec = date_spec()
        date = datetime.strptime("{}-01-01T00:00:00".format(year), spec)
        return date.strftime("%Y")

    def section_id(story_id: str, year: int, month: int) -> str:
        """Use a consistent format to generate section IDs."""
        return "changelog-{}-{}-{}".format(
            normalize_year(year), normalize_month(month), story_id
        )

    def wrap(text: str, limit: int = 80, is_list_item: bool = False) -> str:
        lines = []
        while text:
            if len(text) <= limit:
                lines.append(text)
                break
            split_point = text.rfind(' ', 0, limit + 1)
            if split_point == -1:
                lines.append(text[:limit])
                text = text[limit:]
            else:
                lines.append(text[:split_point])
                text = text[split_point:].strip()
        if is_list_item:
            for index, line in enumerate(lines):
                if index == 0:
                    continue
                lines[index] = "  {}".format(line)
        return "\n".join(lines)

    def escape(subject: str) -> str:
        subject = subject.replace("`", r"\`")
        return subject

    date = datetime.strptime("{}-{}".format(year, month), "%Y-%m").strftime(
        "%B %Y"
    )
    title = "What's new ({})".format(date)
    # Build up the final reStructuredText string imperatively.
    rst = ".. _changelog-{}-{}:\n\n".format(
        normalize_year(year), normalize_month(month)
    )
    rst += "{}\n".format("=" * len(title))
    rst += "{}\n".format(title)
    rst += "{}\n\n".format("=" * len(title))
    rst += ".. changelog_highlights_start\n\n"
    rst += "Highlights:\n\n"
    # A highlight is displayed for every story. The user is expected to trim
    # this list of highlights down to 3-5 before publishing.
    for story in stories:
        if story.highlight is None:
            continue
        highlight = "* :ref:`{}`:\n".format(section_id(story.id, year, month))
        highlight += "  {}\n\n".format(wrap(story.highlight, 75, True))
        if story.impact != HIGH_IMPACT:
            # Comment out stories with insufficient user-facing impact.
            lines = [".. {}".format(line) for line in highlight.splitlines()]
            highlight = "\n".join(lines)
        rst += highlight
    rst += ".. changelog_highlights_end\n\n"
    # Generate a section for each story.
    for story in stories:
        if story.title is None:
            continue
        if story.body is None:
            continue
        section = ".. _{}:\n\n".format(section_id(story.id, year, month))
        section += "{}\n".format("-" * len(story.title))
        section += "{}\n".format(story.title)
        section += "{}\n".format("-" * len(story.title))
        section += "{}\n\n".format(wrap(story.body, 77, False))
        section += "Changes:\n\n"
        for commit in story.commits:
            section += "* `{}\n".format(escape(commit.title))
            section += "  <{}>`__\n\n".format(commit.gerrit)
        if story.impact != HIGH_IMPACT:
            # Comment out stories with insufficient user-facing impact.
            lines = [".. {}".format(line) for line in section.splitlines()]
            section = "\n".join(lines)
        rst += section
    path.write_text(rst)


def main() -> int:
    """CLI entrypoint and main script for the changelog automation."""
    if "GEMINI_API_KEY" not in os.environ:
        sys.exit("[ERROR] $GEMINI_API_KEY env var not found")
    year = None
    month = None
    for arg in sys.argv:
        if "--year=" in arg:
            year = int(arg.replace("--year=", ""))
        if "--month=" in arg:
            month = int(arg.replace("--month=", ""))
    if year is None or month is None:
        sys.exit("[ERR] Usage: ./pw changelog --year=YYYY --month=MM")
    workspace = os.environ.get("BUILD_WORKSPACE_DIRECTORY")
    if workspace is None:
        sys.exit("[ERR] $BUILD_WORKSPACE_DIRECTORY not found")
    root = Path(workspace)
    path = root / Path("docs/sphinx/changelog/py")
    repo = GitRepo(root, BasicSubprocessRunner())
    outdir = path / Path("../{}/{}".format(year, month))
    if not outdir.is_dir():
        outdir.mkdir(parents=True)
    commits = _init_commits(year, month, repo)
    stories = _group_commits_into_stories(commits)
    _summarize_stories_in_parallel(stories, year, month)
    _prioritize_stories_in_parallel(stories)
    stories.sort(key=lambda story: story.title)
    _write(stories, year, month, outdir / Path("index.rst"))
    return 0


if __name__ == '__main__':
    sys.exit(main())
