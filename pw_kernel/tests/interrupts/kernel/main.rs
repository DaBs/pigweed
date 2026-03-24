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
#![no_std]

use core::sync::atomic::{AtomicU32, Ordering};

use kernel::Kernel;
use kernel::interrupt_controller::InterruptController;
use pw_status::Result;

static INTERRUPT_COUNT: AtomicU32 = AtomicU32::new(0);

pub fn test_interrupt_handler<K: Kernel>(kernel: K) {
    // TODO: calling yield_timeslice() to force a reschedule
    // retry is a little hacky.  Improve this test to signal.
    kernel::yield_timeslice(kernel);

    INTERRUPT_COUNT.fetch_add(1, Ordering::SeqCst);
}

pub fn main<K: Kernel>(test_irq: u32) -> Result<()> {
    pw_log::info!("🔄 RUNNING");

    K::InterruptController::enable_interrupt(test_irq);

    let mut initial_count = INTERRUPT_COUNT.load(Ordering::SeqCst);
    pw_assert::assert!(initial_count == 0);

    K::InterruptController::trigger_interrupt(test_irq);
    initial_count = INTERRUPT_COUNT.load(Ordering::SeqCst);
    pw_assert::assert!(initial_count == 1);

    K::InterruptController::trigger_interrupt(test_irq);
    K::InterruptController::trigger_interrupt(test_irq);
    initial_count = INTERRUPT_COUNT.load(Ordering::SeqCst);
    pw_assert::assert!(initial_count == 3);

    pw_log::info!("✅ PASSED");
    Ok(())
}
