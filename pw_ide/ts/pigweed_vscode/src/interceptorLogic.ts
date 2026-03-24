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

/**
 * Determines whether to create the Bazel interceptor file.
 *
 * The interceptor is enabled if it is not explicitly disabled AND compile commands
 * are NOT preconfigured(preconfiguration takes precedence).
 *
 * @param disableInterceptorSetting The value of the `pigweed.disableBazelInterceptor` setting.
 * @param isPreconfigured Whether compile commands are preconfigured in the project.
 * @param createInterceptorFn The function to create the interceptor file.
 */
export async function manageBazelInterceptor(
  disableInterceptorSetting: boolean,
  isPreconfigured: boolean,
  createInterceptorFn: () => Promise<void>,
): Promise<void> {
  if (!disableInterceptorSetting && !isPreconfigured) {
    await createInterceptorFn();
  }
}
