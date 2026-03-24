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
#pragma once

#include <string_view>

#include "pw_function/function.h"
#include "pw_status/status.h"

namespace codelab::webui {

void StartWebUIServer(
    pw::Function<pw::Status(std::string_view)> do_received_text);

void SetDisplay(std::string_view text);

void SetDispenserMotorState(int state);

}  // namespace codelab::webui
