// Copyright 2025 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

import * as assert from 'assert';
import { manageBazelInterceptor } from './interceptorLogic';

suite('manageBazelInterceptor', () => {
  test('creates interceptor when enabled and not preconfigured', async () => {
    let created = false;
    const createFn = async () => {
      created = true;
    };

    await manageBazelInterceptor(false, false, createFn);

    assert.strictEqual(created, true);
  });

  test('does not create interceptor when disabled', async () => {
    let created = false;
    const createFn = async () => {
      created = true;
    };

    await manageBazelInterceptor(true, false, createFn);

    assert.strictEqual(created, false);
  });

  test('does not create interceptor when preconfigured', async () => {
    let created = false;
    const createFn = async () => {
      created = true;
    };

    await manageBazelInterceptor(false, true, createFn);

    assert.strictEqual(created, false);
  });

  test('does not create interceptor when disabled and preconfigured', async () => {
    let created = false;
    const createFn = async () => {
      created = true;
    };

    await manageBazelInterceptor(true, true, createFn);

    assert.strictEqual(created, false);
  });
});
