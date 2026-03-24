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

use regs::{ro_block_reg, ro_int_field, rw_block_reg, rw_bool_field, rw_int_field};

// Marker type to ensure only this driver can be passed to these block registers.
pub trait Uart16550BaseAddress: regs::BaseAddress {}

ro_block_reg!(
    Rbr,
    RbrValue,
    u8,
    Uart16550BaseAddress,
    0,
    "Receiver Buffer Register"
);
pub struct RbrValue(pub u8);
impl RbrValue {
    ro_int_field!(u8, data, 0, 7, u8, "Data");
}

rw_block_reg!(
    Thr,
    ThrValue,
    u8,
    Uart16550BaseAddress,
    0,
    "Transmitter Holding Register"
);
pub struct ThrValue(pub u8);
impl ThrValue {
    rw_int_field!(u8, data, 0, 7, u8, "Data");
}

rw_block_reg!(
    Ier,
    IerVal,
    u8,
    Uart16550BaseAddress,
    1,
    "Interrupt Enable Register"
);
#[repr(transparent)]
pub struct IerVal(pub u8);
impl IerVal {
    rw_bool_field!(u8, erbfi, 0, "Received Data Available Interrupt");
    rw_bool_field!(u8, etbei, 1, "Transmitter Holding Register Empty Interrupt");
    rw_bool_field!(u8, elsi, 2, "Receiver Line Status Interrupt");
    rw_bool_field!(u8, edssi, 3, "Modem Status Interrupt");
}

rw_block_reg!(
    Mcr,
    McrVal,
    u8,
    Uart16550BaseAddress,
    4,
    "Modem Control Register"
);
#[derive(Copy, Clone)]
#[repr(transparent)]
pub struct McrVal(pub u8);
impl McrVal {
    rw_bool_field!(u8, dtr, 0, "Data Terminal Ready");
    rw_bool_field!(u8, rts, 1, "Request to Send");
    rw_bool_field!(u8, out1, 2, "Output1");
    rw_bool_field!(u8, out2, 3, "Output2");
    rw_bool_field!(u8, lo, 4, "Loopback");
}

rw_block_reg!(
    Lsr,
    LsrVal,
    u8,
    Uart16550BaseAddress,
    5,
    "Line Status Register"
);
#[repr(transparent)]
pub struct LsrVal(pub u8);
impl LsrVal {
    rw_bool_field!(u8, dr, 0, "Data Ready");
    rw_bool_field!(u8, oe, 1, "Overrun Error");
    rw_bool_field!(u8, pe, 2, "Parity Error");
    rw_bool_field!(u8, fe, 3, "Framing Error");
    rw_bool_field!(u8, bi, 4, "Break Interrupt");
    rw_bool_field!(u8, thre, 5, "Transmitter Holding Register Empty");
    rw_bool_field!(u8, temt, 6, "Transmitter Empty indicator");
    rw_bool_field!(u8, erf, 7, "Error in Receiver FIFO");
}
