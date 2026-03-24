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

/// Basic configuration interface for the UARTs.
pub trait UartConfigInterface {
    /// Base address of the UART.
    const BASE_ADDRESS: usize;
    /// Interrupt number on which the UART is configured.
    const IRQ: u32;
}

#[doc(hidden)]
pub mod __private {
    pub use paste::paste;
}

#[macro_export]
macro_rules! declare_uarts {
    (
        $arch:ty,
        $uarts_array:ident,
        $uart_type:ty,
        [ $( $uart_name:ident: $config:ident),* $(,)? ]
    ) => {
        $crate::__private::paste! {
            $(
                static $uart_name: $uart_type<$arch> = $uart_type::new($config::BASE_ADDRESS, $config::IRQ);
                fn [<interrupt_handler_$uart_name:lower>](kernel: $arch) {
                    $uart_name.interrupt_handler(kernel);
                }
            )*
            pub static $uarts_array: [&$uart_type<$arch>; [$(stringify!($uart_name)),*].len()] = [
                $( &$uart_name ),*
            ];
        }
    };
}
