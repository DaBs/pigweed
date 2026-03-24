# End to End tests

This directory contains all the pw_kernel end to end tests.  The directory is organized by the `<FUNCTIONALITY>/{user|kernel}/` where `user` contains userspace tests and `kernel` contains kernelspace tests.

These tests are designed to be target agnostic, and can be run on any pw_kernel target such as qemu-virt-riscv32 and mps2-an505.
