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

const LOG_UART: bool = false;

pub struct Uart {
    base_address: usize,
}

impl regs::BaseAddress for Uart {
    fn base_address(&self) -> usize {
        self.base_address
    }
}

impl uart_16550_regs::Uart16550BaseAddress for Uart {}

impl Uart {
    #[must_use]
    pub fn new(base_address: usize) -> Uart {
        let instance = Self { base_address };

        let mut ier = uart_16550_regs::Ier;
        // Enable the Received Data Available Interrupt.
        ier.write(&instance, ier.read(&instance).with_erbfi(true));

        instance
    }

    pub fn enable_loopback(&mut self) {
        let mut mcr = uart_16550_regs::Mcr;
        let val = mcr.read(self).with_lo(true);
        mcr.write(self, val);
        log_if::debug_if!(
            LOG_UART,
            "set MCR to {:#04x}, should be {:#04x}",
            mcr.read(self).0 as u8,
            val.0 as u8
        );
    }

    pub fn wait_for_rx(&self) {
        let lsr = uart_16550_regs::Lsr;

        while !lsr.read(self).dr() {}
    }

    pub fn read(&mut self) -> Option<u8> {
        //log_if::debug_if!(LOG_UART, "uart read");

        let lsr = uart_16550_regs::Lsr;
        let lsr_val = lsr.read(self);
        //log_if::debug_if!(LOG_UART, "uart read lsr = {:02x}", lsr_val.0 as u32);
        if !lsr_val.dr() {
            return None;
        }
        let value = uart_16550_regs::Rbr.read(self);
        log_if::debug_if!(LOG_UART, "data ready: {}", value.data() as u8);
        Some(value.data())
    }

    pub fn write(&mut self, value: u8) {
        log_if::debug_if!(LOG_UART, "uart write: {}", value as u8);
        let lsr = uart_16550_regs::Lsr;
        while !lsr.read(self).thre() {}

        uart_16550_regs::Thr.write(self, uart_16550_regs::ThrValue(value));
        log_if::debug_if!(LOG_UART, "done");
    }
}
