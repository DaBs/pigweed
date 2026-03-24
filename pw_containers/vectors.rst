.. _module-pw_containers-vectors:

=======
Vectors
=======
.. pigweed-module-subpage::
   :name: pw_containers

A vector is a one-dimensional array with a variable length.

----------
pw::Vector
----------
:cc:`pw::Vector` class is similar to ``std::vector``, except it is backed by a
fixed-size buffer.

Vectors must be declared with an explicit maximum size
(e.g. ``Vector<int, 10>``) but vectors can be used and referred to without the
max size template parameter (e.g. ``Vector<int>``).

To allow referring to a ``pw::Vector`` without an explicit maximum size, all
Vector classes inherit from the generic ``Vector<T>``, which stores the maximum
size in a variable. This allows Vectors to be used without having to know
their maximum size at compile time. It also keeps code size small since
function implementations are shared for all maximum sizes.

Example
=======
.. literalinclude:: examples/vector.cc
   :language: cpp
   :linenos:
   :start-after: [pw_containers-vector]
   :end-before: [pw_containers-vector]

Size report
===========
The tables below illustrate the following scenarios:

* The memory and code size cost incurred by a adding a single ``Vector``.
* The memory and code size cost incurred by adding another ``Vector`` with the
  same type as the first scenario, but with a different size. As ``Vector``
  is templated on both type and size, a different size results in additional
  code being generated.
* The memory and code size cost incurred by adding another ``Vector`` with the
  same size as the first scenario, but with a different type. As ``Vector``
  is templated on both type and size, a different size results in additional
  code being generated.

.. include:: vectors_size_report

-----------------
pw::DynamicVector
-----------------
:cc:`pw::DynamicVector` is similar to :cc:`pw::Vector`, except that it uses
:cc:`pw::Allocator` for memory operations.

--------------------
pw::DynamicPtrVector
--------------------
:cc:`pw::DynamicPtrVector` behaves like a ``std::vector<T>``, but stores
pointers to objects of type ``T`` allocated using a :cc:`pw::Allocator`. This
allows it to store objects that are not movable or copyable, or objects of
polymorphic types, while still providing a vector-like interface.

To support this, :cc:`pw::DynamicPtrVector` differs from `std::vector` in a few
ways:

*   **Memory Management**: It manages the lifetime of the objects it holds.
    When an element is removed (e.g., via ``pop_back``, ``erase``, or
    destruction of the vector), the object is destroyed and its memory deallocated.
*   **Pointer Storage**: The underlying storage is a ``DynamicVector<T*>``.
    Functions like ``data()`` return ``T* const*``.
*   **Capacity**: ``reserve_ptr`` and ``ptr_capacity`` refer to the capacity of
    the underlying pointer vector, not the objects themselves. Objects are
    allocated individually.
*   **Emplace**: ``emplace`` and ``emplace_back`` allow constructing derived
    classes (e.g. ``vec.emplace_back<Derived>(args...)``).

-------------
API reference
-------------
Moved: :cc:`pw_containers_vectors`
