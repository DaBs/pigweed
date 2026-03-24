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
"""Helper functions that multiple changelog automation modules depend on."""

import time

from google import genai
from google.genai.errors import ClientError
from google.genai.errors import ServerError


def max_tokens() -> int:
    """The maximum number of input tokens allowed."""
    return 1048576  # Flash and Pro have the same limit


def count_tokens(text: str) -> int:
    """The number of input tokens that a text will require."""
    gemini = genai.Client()
    model = "gemini-2.5-flash"  # Pro and Flash have the same limit
    token_count = gemini.models.count_tokens(
        model=model, contents=text
    ).total_tokens
    if token_count is None:
        # Token is approximately 4 characters
        return int(len(text) / 4)
    return token_count


def is_under_token_limit(text: str) -> bool:
    """Determine whether the given text is under the maximum input limit."""
    return count_tokens(text) < max_tokens()


class TokenLimitError(Exception):
    """Raised when a roll commit is detected."""


def generate(
    prompt: str, model: str, config: genai.types.GenerateContentConfig
) -> dict | None:
    """Generate text with Gemini API."""
    gemini = genai.Client()
    done = False
    delay = 10
    rate_limit_code = 429
    token_limit_code = 400
    while not done:
        try:
            response = gemini.models.generate_content(
                model=model, contents=prompt, config=config
            )
        except ClientError as e:
            if e.code == token_limit_code:
                raise TokenLimitError
            if e.code == rate_limit_code:
                print(
                    "[INF] Retrying Gemini API request in {} seconds…".format(
                        delay
                    )
                )
                time.sleep(delay)
                delay += 10
                continue
        except ServerError:
            print(
                "[INF] Gemini API server error. Retrying in {} seconds…".format(
                    delay
                )
            )
            time.sleep(delay)
            delay += 10
            continue
    data = response.parsed
    return data if isinstance(data, dict) else None
