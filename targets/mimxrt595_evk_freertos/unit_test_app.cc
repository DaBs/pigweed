// Copyright 2024 The Pigweed Authors
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

#include "pw_unit_test/framework.h"
#include "pw_unit_test/logging_event_handler.h"

namespace pw::system {

// This will run once after pw::system::Init() completes. This callback must
// return or it will block the work queue.
void UserAppInit() {
  testing::InitGoogleTest(0, NULL);
  pw::unit_test::LoggingEventHandler handler;
  pw::unit_test::RegisterEventHandler(&handler);
  RUN_ALL_TESTS();
}

}  // namespace pw::system
