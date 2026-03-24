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

use crate::scheduler::Priority;

/// Provides bitmask utility sufficient for tracking the status of all priorities.
/// All operations are restricted to the valid range of priorities.
pub struct PriorityBitmask {
    /// Invariant: The bits set within `bitmasks` always correspond to valid
    /// `Priority` enum values. This is enforced by the `set_priority` and
    /// `clear_priority` methods, which only accept `Priority` enum values.
    bitmasks: [usize; Self::CHUNKS],
}

impl Default for PriorityBitmask {
    fn default() -> Self {
        Self::new()
    }
}

impl PriorityBitmask {
    const CHUNKS: usize = Priority::NUM_PRIORITIES.div_ceil(usize::BITS as usize);

    #[must_use]
    pub const fn new() -> Self {
        // Run compile-time assertions that the Priority enum is suitable for
        // use in the PriorityBitmask. This is necessary to guarantee the
        // transmute in `get_highest_priority` is safe.
        const _: () = {
            // Guarantee assumptions on the siz and range of the Priority enum.
            assert!(core::mem::size_of::<Priority>() <= core::mem::size_of::<u8>());
            assert!(Priority::NUM_PRIORITIES - 1 <= u8::MAX as usize);
        };

        Self {
            bitmasks: [0; Self::CHUNKS],
        }
    }

    /// Marks a priority as active.
    pub fn set_priority(&mut self, priority: Priority) {
        let chunk_index = priority as usize / usize::BITS as usize;
        let bit_in_chunk = priority as usize % usize::BITS as usize;
        self.bitmasks[chunk_index] |= 1 << bit_in_chunk;
    }

    /// Marks a priority as inactive.
    pub fn clear_priority(&mut self, priority: Priority) {
        let chunk_index = priority as usize / usize::BITS as usize;
        let bit_in_chunk = priority as usize % usize::BITS as usize;
        self.bitmasks[chunk_index] &= !(1 << bit_in_chunk);
    }

    /// Retrieves the highest priority that is active. If no priorities are active,
    /// `None` is returned.
    #[must_use]
    pub fn get_highest_priority(&self) -> Option<Priority> {
        const OFFSET: u32 = usize::BITS - 1;
        for i in (0..Self::CHUNKS).rev() {
            if self.bitmasks[i] != 0 {
                // `BITS` and `leading_zeros` are defined as u32s so we operate
                // on u32 here.
                let highest_bit_index = OFFSET - self.bitmasks[i].leading_zeros();
                #[allow(clippy::cast_possible_truncation)]
                let raw_priority_value = highest_bit_index + (i as u32 * usize::BITS);

                // Priority values are guaranteed to be u8 sized.
                #[allow(clippy::cast_possible_truncation)]
                let priority_value = raw_priority_value as u8;

                // SAFETY: Per the `PriorityBitmask` invariant, all set bits
                // correspond to valid `Priority` enum values. Thus, the
                // extracted `priority_value` is guaranteed to be a valid
                // discriminant for `Priority`.
                let priority = unsafe { core::mem::transmute::<u8, Priority>(priority_value) };
                return Some(priority);
            }
        }

        None
    }
}
