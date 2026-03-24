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

use core::cell::UnsafeCell;
use core::ops::{Deref, DerefMut};

use crate::scheduler::PreemptDisableGuard;
use crate::{Arch, Kernel};

pub trait BareSpinLock: Send + Sync {
    type Guard<'a>
    where
        Self: 'a;

    const NEW: Self;

    fn try_lock(&self) -> Option<Self::Guard<'_>>;

    // This default implementation of `lock()` will usually be overridden by
    // architecture-specific implementations.
    #[inline(always)]
    fn lock(&self) -> Self::Guard<'_> {
        loop {
            if let Some(sentinel) = self.try_lock() {
                return sentinel;
            }
        }
    }

    /// Unconditionally break the lock.
    ///
    /// Do not call directly.
    ///
    /// # Safety
    /// See [`SpinLock::break_lock()`] for use and safety information.
    unsafe fn break_lock(&self);

    // TODO - konkers: Add optimized path for functions that know they are in
    // atomic context (i.e. interrupt handlers).
}

pub struct SpinLockGuard<'lock, K: Kernel, T> {
    lock: &'lock SpinLock<K, T>,
    _preempt_guard: PreemptDisableGuard<K>,
    _inner_guard: <<K as Arch>::BareSpinLock as BareSpinLock>::Guard<'lock>,
}

impl<K: Kernel, T> Deref for SpinLockGuard<'_, K, T> {
    type Target = T;

    fn deref(&self) -> &T {
        unsafe { &*self.lock.data.get() }
    }
}

impl<K: Kernel, T> DerefMut for SpinLockGuard<'_, K, T> {
    fn deref_mut(&mut self) -> &mut T {
        unsafe { &mut *self.lock.data.get() }
    }
}

pub struct SpinLock<K: Kernel, T> {
    data: UnsafeCell<T>,
    inner: K::BareSpinLock,
}
// As long as the inner type is `Send` and the bare spinlock is `Sync`, the lock
// can be shared between threads.
unsafe impl<K: Sync + Kernel, T: Send> Sync for SpinLock<K, T> {}

impl<K: Kernel, T> SpinLock<K, T> {
    pub const fn new(initial_value: T) -> Self {
        Self {
            data: UnsafeCell::new(initial_value),
            inner: K::BareSpinLock::NEW,
        }
    }

    pub fn try_lock(&self, kernel: K) -> Option<SpinLockGuard<'_, K, T>> {
        self.inner.try_lock().map(|guard| SpinLockGuard {
            lock: self,
            _preempt_guard: PreemptDisableGuard::new(kernel),
            _inner_guard: guard,
        })
    }

    pub fn lock(&self, kernel: K) -> SpinLockGuard<'_, K, T> {
        let inner_guard = self.inner.lock();
        SpinLockGuard {
            lock: self,
            _preempt_guard: PreemptDisableGuard::new(kernel),
            _inner_guard: inner_guard,
        }
    }

    /// Unconditionally break the lock.
    ///
    /// This method is used to forcibly release the lock without dropping the
    /// guard. This is unsafe because it breaks the lock invariants and can
    /// lead to data corruption if not used carefully.
    ///
    /// # Safety
    /// This method should only be called in specific scenarios:
    /// 1. Thread starting: to drop the scheduler lock on thread start.
    /// 2. Resource access during a system wide crash dump.
    ///
    /// The caller must ensure that breaking the lock will not cause data corruption.
    pub unsafe fn break_lock(&self) {
        unsafe {
            self.inner.break_lock();
        }
    }
}
