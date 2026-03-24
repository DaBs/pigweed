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

/// Represents the priority of a thread.
///
/// Note: An enum is used to provide guarantees to the compiler about the
/// range of values that can be used. An example of where this is useful is that
/// it can allow the compiler to elide bounds checks for an array indexed by
/// Priority. Another example is that it allows for eliding valid range checks
/// in bitmask operations. Downsides include that the Priority declaration is
/// quite verbose and normal integer operations are not straightforward.
///
/// This is designed such that:
/// - The priority can be used as an index in an array.
/// - An array indexed by all priorities will be compact with no gaps.
///
/// A choice was made to explicitly support 32 priorities initially with the
/// possibility of customizing the Priority type being supported in the future.
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
#[repr(u8)]
pub enum Priority {
    Level0 = 0,
    Level1 = 1,
    Level2 = 2,
    Level3 = 3,
    Level4 = 4,
    Level5 = 5,
    Level6 = 6,
    Level7 = 7,
    Level8 = 8,
    Level9 = 9,
    Level10 = 10,
    Level11 = 11,
    Level12 = 12,
    Level13 = 13,
    Level14 = 14,
    Level15 = 15,
    Level16 = 16,
    Level17 = 17,
    Level18 = 18,
    Level19 = 19,
    Level20 = 20,
    Level21 = 21,
    Level22 = 22,
    Level23 = 23,
    Level24 = 24,
    Level25 = 25,
    Level26 = 26,
    Level27 = 27,
    Level28 = 28,
    Level29 = 29,
    Level30 = 30,
    Level31 = 31,
}

impl Priority {
    pub const IDLE_PRIORITY: Priority = Priority::Level0;
    pub const DEFAULT_PRIORITY: Priority = Priority::Level15;
    /// The total number of priorities supported.
    pub const NUM_PRIORITIES: usize = Priority::Level31 as usize + 1;
}
