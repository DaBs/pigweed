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

const LOG_UART: bool = true;

const BUFFER_SIZE: usize = 16;
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

impl<K: Kernel> cmsdk_apb_uart_regs::CmsdkApbUartBaseAddress for Uart<K> {}

impl<K: Kernel> Uart<K> {
    #[must_use]
    pub const fn new(base_address: usize, irq: u32) -> Uart<K> {
        Self {
            base_address,
            irq,
            read_buffer: SpinLock::new(CircularBuffer::new()),
        }
    }

    pub fn read(&self, kernel: K) -> Result<Option<u8>> {
        log_if::debug_if!(LOG_UART, "uart read");
        Ok(self.read_buffer.lock(kernel).pop_front())
    }

    pub fn write(&self, value: u8) -> Result<()> {
        log_if::debug_if!(LOG_UART, "uart write: {}", value as u8);

        let state_reg = cmsdk_apb_uart_regs::State;
        while state_reg.read(self).tx_buffer_full() {}

        let mut data_reg = cmsdk_apb_uart_regs::Data;
        data_reg.write(self, cmsdk_apb_uart_regs::DataValue(value));

        log_if::debug_if!(LOG_UART, "done");
        Ok(())
    }

    pub fn interrupt_handler(&self, kernel: K) {
        log_if::debug_if!(
            LOG_UART,
            "uart {:#010x}: interrupt_handler",
            self.base_address as usize
        );

        let int_status_reg = cmsdk_apb_uart_regs::IntStatus;
        let int_status = int_status_reg.read(self);

        if int_status.rx_interrupt() {
            log_if::debug_if!(LOG_UART, "RX interrupt");

            let state_reg = cmsdk_apb_uart_regs::State;
            while state_reg.read(self).rx_buffer_full() {
                let data_reg = cmsdk_apb_uart_regs::Data;
                let value = data_reg.read(self);
                log_if::debug_if!(LOG_UART, "data ready: {}", value.data() as u8);
                let _ = self.read_buffer.lock(kernel).push_back(value.data());
            }
            let mut int_clear_reg = cmsdk_apb_uart_regs::IntClear;
            let int_clear = cmsdk_apb_uart_regs::IntClearValue(0).with_rx_interrupt(true);
            int_clear_reg.write(self, int_clear);
        }

        if int_status.tx_interrupt() {
            log_if::debug_if!(LOG_UART, "TX interrupt");

            let mut int_clear_reg = cmsdk_apb_uart_regs::IntClear;
            let int_clear = cmsdk_apb_uart_regs::IntClearValue(0).with_tx_interrupt(true);
            int_clear_reg.write(self, int_clear);
        }
    }
}

pub fn init<K: Kernel>(uarts: &[&Uart<K>]) {
    for uart in uarts {
        // Enable RX and TX
        let mut ctrl_reg = cmsdk_apb_uart_regs::Ctrl;
        let mut ctrl_value = ctrl_reg.read(*uart);
        ctrl_value = ctrl_value.with_tx_enable(true);
        ctrl_value = ctrl_value.with_tx_interrupt_enable(true);
        ctrl_value = ctrl_value.with_rx_enable(true);
        ctrl_value = ctrl_value.with_rx_interrupt_enable(true);

        ctrl_reg.write(*uart, ctrl_value);
        K::InterruptController::enable_interrupt(uart.irq);
    }
}
