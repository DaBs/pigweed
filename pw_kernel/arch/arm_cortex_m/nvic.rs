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

use kernel::interrupt_controller::InterruptController;
use kernel::scheduler::PreemptDisableGuard;
use kernel::{Kernel, interrupt_controller};
use kernel_config::{NvicConfig, NvicConfigInterface};
use log_if::debug_if;
use pw_log::info;

use crate::regs;

const LOG_INTERRUPTS: bool = false;

pub struct Nvic {}

impl Nvic {
    #[must_use]
    pub const fn new() -> Self {
        Self {}
    }
}

impl InterruptController for Nvic {
    fn early_init(&self) {
        info!("Initializing NVIC");
        let nvic_regs = regs::Nvic {};
        // Set all of the NVIC external IRQs to the same priority as SysTick.
        for i in 0..NvicConfig::MAX_IRQS {
            nvic_regs.set_priority(i as usize, 0b0100_0000);
        }
    }

    fn enable_interrupt(irq: u32) {
        debug_if!(LOG_INTERRUPTS, "Enable interrupt {}", irq as u32);
        let mut nvic_regs = regs::Nvic {};
        nvic_regs.enable(irq as usize);
    }

    fn disable_interrupt(irq: u32) {
        debug_if!(LOG_INTERRUPTS, "Disable interrupt {}", irq as u32);
        let mut nvic_regs = regs::Nvic {};
        nvic_regs.disable(irq as usize);
    }

    fn userspace_interrupt_ack(irq: u32) {
        debug_if!(
            LOG_INTERRUPTS,
            "Userspace interrupt {} handler ack",
            irq as u32
        );
        // re-enable the interrupt to allow it to be triggered again.
        Self::enable_interrupt(irq);
    }

    fn userspace_interrupt_handler_enter<K: Kernel>(kernel: K, irq: u32) -> PreemptDisableGuard<K> {
        debug_if!(
            LOG_INTERRUPTS,
            "Userspace interrupt {} handler enter",
            irq as u32
        );
        Self::disable_interrupt(irq);
        PreemptDisableGuard::new(kernel)
    }

    fn userspace_interrupt_handler_exit<K: Kernel>(
        kernel: K,
        irq: u32,
        preempt_guard: PreemptDisableGuard<K>,
    ) {
        debug_if!(
            LOG_INTERRUPTS,
            "Userspace interrupt {} handler exit",
            irq as u32
        );
        interrupt_controller::handler_done(kernel, preempt_guard);
    }

    fn kernel_interrupt_handler_enter<K: Kernel>(kernel: K, irq: u32) -> PreemptDisableGuard<K> {
        debug_if!(
            LOG_INTERRUPTS,
            "Kernel interrupt {} handler enter",
            irq as u32
        );
        // No need to mask the interrupt, as the NVIC automatically masks
        // the interrupt while the handler is running.
        PreemptDisableGuard::new(kernel)
    }

    fn kernel_interrupt_handler_exit<K: Kernel>(
        kernel: K,
        irq: u32,
        preempt_guard: PreemptDisableGuard<K>,
    ) {
        debug_if!(
            LOG_INTERRUPTS,
            "Kernel interrupt {} handler exit",
            irq as u32
        );
        // No need to unmask the interrupt, as the NVIC will automatically
        // unmask the interrupt when the handler exits.
        interrupt_controller::handler_done(kernel, preempt_guard);
    }

    fn enable_interrupts() {
        debug_if!(LOG_INTERRUPTS, "Enable interrupts");
        unsafe {
            cortex_m::interrupt::enable();
        }
    }

    fn disable_interrupts() {
        debug_if!(LOG_INTERRUPTS, "Disable interrupts");
        cortex_m::interrupt::disable();
    }

    fn interrupts_enabled() -> bool {
        // It's a complicated concept in cortex-m:
        // If PRIMASK is inactive, then interrupts are 100% disabled otherwise
        // if the current interrupt priority level is not zero (BASEPRI register) interrupts
        // at that level are not allowed. For now we're treating nonzero as full disabled.
        let primask = cortex_m::register::primask::read();
        let basepri = cortex_m::register::basepri::read();
        debug_if!(
            LOG_INTERRUPTS,
            "Interrupts enabled: PRIMASK={}, BASEPRI={}",
            primask as u32,
            basepri as u8,
        );
        primask.is_active() && (basepri == 0)
    }

    fn trigger_interrupt(irq: u32) {
        debug_if!(LOG_INTERRUPTS, "Trigger interrupt {}", irq as u32);
        let mut nvic_regs = regs::Nvic {};
        nvic_regs.set_pending(irq as usize);
    }
}
