.. _module-pw_async2-dispatcher:

==========
Dispatcher
==========
.. pigweed-module-subpage::
   :name: pw_async2

``pw_async2`` dispatchers implement the :cc:`pw::async2::Dispatcher` interface.
The :cc:`pw::async2::RunnableDispatcher` class can optionally be used to support
running the dispatcher directly in a thread.

Pigweed provides two :cc:`Dispatcher <pw::async2::Dispatcher>` implementations:

* :cc:`pw::async2::BasicDispatcher` is a simple thread-notification-based
  :cc:`RunnableDispatcher <pw::async2::RunnableDispatcher>` implementation.
* :cc:`pw::async2::EpollDispatcher` is a :cc:`RunnableDispatcher
  <pw::async2::RunnableDispatcher>` backed by Linux's `epoll`_ notification
  system.

.. _module-pw_async2-dispatcher-overview:

------------------------------
How a dispatcher manages tasks
------------------------------
The purpose of a :cc:`Dispatcher <pw::async2::Dispatcher>` is to keep
track of a set of :cc:`Task <pw::async2::Task>` objects and run them to
completion. The dispatcher is essentially a scheduler for cooperatively
scheduled (non-preemptive) threads (tasks).

Tasks notify their dispatchers when they are ready to run. The dispatcher then
runs and advances each task by invoking its :cc:`DoPend
<pw::async2::Task::DoPend>` method. The ``DoPend`` method is typically
implemented manually by users, though it is automatically provided by
coroutines.

If the task is able to complete, ``DoPend`` will return ``Ready``, in which
case the dispatcher will deregister the task.

If the task is unable to complete, ``DoPend`` must return ``Pending`` and
arrange for the task to be woken up when it is able to make progress again.
Once the task is rewoken, the task is re-added to the ``Dispatcher`` queue. The
dispatcher will then invoke ``DoPend`` once more, continuing the cycle until
``DoPend`` returns ``Ready`` and the task is completed.

See :ref:`module-pw_async2-informed-poll` for more conceptual explanation.

.. _epoll: https://man7.org/linux/man-pages/man7/epoll.7.html
