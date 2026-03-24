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

use regs::{ro_block_reg, rw_block_reg, rw_bool_field, rw_int_field};

// Marker type to ensure only this driver can be passed to these block registers.
pub trait CmsdkApbUartBaseAddress: regs::BaseAddress {}

rw_block_reg!(
    Data,
    DataValue,
    u8,
    CmsdkApbUartBaseAddress,
    0,
    "Data Register"
);
#[derive(Copy, Clone)]
#[repr(transparent)]
pub struct DataValue(pub u8);
impl DataValue {
    rw_int_field!(u8, data, 0, 7, u8, "Data");
}

rw_block_reg!(
    State,
    StateValue,
    u8,
    CmsdkApbUartBaseAddress,
    0x004,
    "State Register"
);
#[derive(Copy, Clone)]
#[repr(transparent)]
pub struct StateValue(pub u8);
impl StateValue {
    rw_bool_field!(u8, tx_buffer_full, 0, "Transmit Buffer Full");
    rw_bool_field!(u8, rx_buffer_full, 1, "Receive Buffer Full");
    rw_bool_field!(u8, tx_buffer_overrun, 2, "Transmit Buffer Overrun");
    rw_bool_field!(u8, rx_buffer_overrun, 3, "Receive Buffer Overrun");
}

rw_block_reg!(
    Ctrl,
    CtrlValue,
    u8,
    CmsdkApbUartBaseAddress,
    0x008,
    "Control Register"
);
#[derive(Copy, Clone)]
#[repr(transparent)]
pub struct CtrlValue(pub u8);
impl CtrlValue {
    rw_bool_field!(u8, tx_enable, 0, "Transmit Enable");
    rw_bool_field!(u8, rx_enable, 1, "Receive Enable");
    rw_bool_field!(u8, tx_interrupt_enable, 2, "Transmit Interrupt Enable");
    rw_bool_field!(u8, rx_interrupt_enable, 3, "Receive Interrupt Enable");
    rw_bool_field!(
        u8,
        tx_overrun_interrupt_enable,
        4,
        "Transmit Overrun Interrupt Enable"
    );
    rw_bool_field!(
        u8,
        rx_overrun_interrupt_enable,
        5,
        "Receive Overrun Interrupt Enable"
    );
}

ro_block_reg!(
    IntStatus,
    IntStatusValue,
    u8,
    CmsdkApbUartBaseAddress,
    0x00c,
    "Interrupt Status Register"
);
#[derive(Copy, Clone)]
#[repr(transparent)]
pub struct IntStatusValue(pub u8);
impl IntStatusValue {
    rw_bool_field!(u8, tx_interrupt, 0, "Transmit Interrupt");
    rw_bool_field!(u8, rx_interrupt, 1, "Receive Interrupt");
    rw_bool_field!(u8, tx_overrun_interrupt, 2, "Transmit Overrun Interrupt");
    rw_bool_field!(u8, rx_overrun_interrupt, 3, "Receive Overrun Interrupt");
}

rw_block_reg!(
    IntClear,
    IntClearValue,
    u8,
    CmsdkApbUartBaseAddress,
    0x00c,
    "Interrupt Clear Register"
);
#[derive(Copy, Clone)]
#[repr(transparent)]
pub struct IntClearValue(pub u8);
impl IntClearValue {
    rw_bool_field!(u8, tx_interrupt, 0, "Transmit Interrupt");
    rw_bool_field!(u8, rx_interrupt, 1, "Receive Interrupt");
    rw_bool_field!(u8, tx_overrun_interrupt, 2, "Transmit Overrun Interrupt");
    rw_bool_field!(u8, rx_overrun_interrupt, 3, "Receive Overrun Interrupt");
}
