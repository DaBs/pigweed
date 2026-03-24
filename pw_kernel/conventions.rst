.. _module-pw_kernel-conventions:

===========
Conventions
===========
.. pigweed-module-subpage::
   :name: pw_kernel

This section outlines the conventions used in ``pw_kernel`` to ensure
consistency and maintainability.

Logging and error handling
==========================
Consistent logging is crucial for debugging and maintaining ``pw_kernel``.

General logging style
---------------------
- **Capitalization**: Log messages should be capitalized as sentences, unless
  they begin with a specific identifier or symbol that is lowercase by
  definition (e.g., ``thread_name``).
- **Punctuation**: Do not end log messages with a trailing period. *Exception*:
  If a log message contains multiple full sentences, use periods for the
  intermediate sentences, but omit it for the final one.
- **Clarity**: Prefer clear, descriptive messages over cryptic abbreviations.

Register and value dumping
--------------------------
- **Format**: Use ``Key=Value`` format for technical dumps, separated by commas
  if on the same line. *Example*: ``Exception: cause={:#x}, epc={:#x}``
- **Hexadecimal**: Use standard Rust hex formatting ``{:#x}`` for values. For
  full 32-bit addresses, use ``{:#010x}`` to include the ``0x`` prefix and
  leading zeros.
- **Threads**: When logging thread information, use the format ``'thread_name'
  ({:#010x})``, where the name is in single quotes and the ID is in parentheses
  and hex format. *Example*: ``Context switch to thread 'idle' (0x20001234)``

Optional debug logging
----------------------
High-frequency or verbose logs that are useful for specific debugging sessions
but too noisy for general use should be gated by a ``const bool`` flag using
the ``log_if`` crate.

- **Flags**: Define a ``const bool`` flag at the top of the file (e.g., ``const
  LOG_SCHEDULER_EVENTS: bool = false;``).
- **Default State**: These flags must be committed as ``false`` by default.
- **Usage**: Use ``log_if::info_if!`` or ``log_if::debug_if!`` with the flag.
  *Example*: ``log_if::info_if!(LOG_CONTEXT_SWITCH, "Context switch to thread
  '{}' ({:#x})", ...)``

Panics
------
- **Macro**: Always use ``pw_assert::panic!`` instead of the standard library
  ``panic!``.
- **Style**: Follow the same capitalization and punctuation rules as general
  logs (capitalized, no trailing period).
- **Context**: Provide as much context as reasonable in the panic message.

  - *Bad*: ``pw_assert::panic!("Run queue empty")``
  - *Good*: ``pw_assert::panic!("Run queue empty: no runnable threads (idle
    thread missing?)")``

- **Unimplemented**: For unimplemented features, use
  ``pw_assert::panic!("Unimplemented: <feature_name>")`` instead of ``todo!()``
  or generic "Unimplemented" messages.
