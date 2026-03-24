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

use circular_buffer::CircularBuffer;
use kernel::Kernel;
use kernel::interrupt_controller::InterruptController;
use kernel::sync::spinlock::SpinLock;
use pw_status::Result;

const LOG_UART: bool = false;

const BUFFER_SIZE: usize = 128;
pub struct Uart<K: Kernel> {
    base_address: usize,
    irq: u32,
    read_buffer: SpinLock<K, CircularBuffer<u8, BUFFER_SIZE>>,
}

impl<K: Kernel> regs::BaseAddress for Uart<K> {
    fn base_address(&self) -> usize {
        self.base_address
    }
}

impl<K: Kernel> uart_16550_regs::Uart16550BaseAddress for Uart<K> {}

impl<K: Kernel> Uart<K> {
    #[must_use]
    pub const fn new(base_address: usize, irq: u32) -> Uart<K> {
        Self {
            base_address,
            irq,
            read_buffer: SpinLock::new(CircularBuffer::new()),
        }
    }

    pub fn enable_loopback(&self) {
        log_if::debug_if!(LOG_UART, "enable loopback");
        let mut mcr = uart_16550_regs::Mcr;
        mcr.write(self, mcr.read(self).with_lo(true));
    }

    pub fn read(&self, kernel: K) -> Result<Option<u8>> {
        log_if::debug_if!(LOG_UART, "uart read");
        Ok(self.read_buffer.lock(kernel).pop_front())
    }

    pub fn write(&self, value: u8) -> Result<()> {
        log_if::debug_if!(LOG_UART, "uart write: {}", value as u8);
        let lsr = uart_16550_regs::Lsr;
        while !lsr.read(self).thre() {}

        uart_16550_regs::Thr.write(self, uart_16550_regs::ThrValue(value));
        log_if::debug_if!(LOG_UART, "done");
        Ok(())
    }

    pub fn interrupt_handler(&self, kernel: K) {
        log_if::debug_if!(
            LOG_UART,
            "uart {:#010x}: interrupt_handler",
            self.base_address as usize
        );

        let lsr = uart_16550_regs::Lsr;
        while lsr.read(self).dr() {
            let value = uart_16550_regs::Rbr.read(self);
            log_if::debug_if!(LOG_UART, "data ready: {}", value.data() as u8);
            let _ = self.read_buffer.lock(kernel).push_back(value.data());
        }
    }
}

pub fn init<K: Kernel>(uarts: &[&Uart<K>]) {
    for uart in uarts {
        let mut ier = uart_16550_regs::Ier;
        ier.write(*uart, ier.read(*uart).with_erbfi(true));

        K::InterruptController::enable_interrupt(uart.irq);
    }
}
