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
#![cfg_attr(test, no_main)]
#![allow(dead_code)]
use core::cell::UnsafeCell;
use core::marker::PhantomData;
use core::ptr::NonNull;

// Intrusive link structures are particularly tricky in Rust because mutable
// references are expected to be globally unique.  Accessing the data through
// other methods is UB.  There is a good writeup of Tokio's soundness challenges
// at: https://gist.github.com/Darksonn/1567538f56af1a8038ecc3c664a42462.
//
// Here we adapt the strategy that Tokio uses:
// * The link pointer structure is marked with `PhantomPinned`.  This does two
//   things.  First it "poisons" the containing structure so that it's
//   unpinnable (immovable).  Second it triggers the heuristic from
//   https://github.com/rust-lang/rust/pull/82834 which will cause the compiler
//   to omit emitting a `noalias` annotation on mutable references to disable
//   compiler optimization that assume the mutable reference is unique.
// * `next` and `prev` themselves are accessed by `Link` through direct pointer
//   math on the `LinkInner` struct.  This avoids creating mutable references to
//   those fields themselves which can not be annotated with `PhantomPinned`.
//   `LinkInner` is `#[repr(C)]` to make the pointer math deterministic.
//   `LinkInner` is declared in an `inner` module so that `next` and `prev` can
//   not be accessed directly be the rest of the code.
//
// TODO: konkers - Understand if we need to annotate the alignment of LinkInner.
mod inner {
    use core::{marker::PhantomPinned, mem::offset_of, ptr::NonNull};

    use super::Link;

    #[repr(C)]
    pub struct LinkInner {
        #[allow(dead_code)]
        next: Option<NonNull<Link>>,
        #[allow(dead_code)]
        prev: Option<NonNull<Link>>,
        _pin: PhantomPinned,
    }

    impl LinkInner {
        pub const NEXT_OFFSET: usize = offset_of!(LinkInner, next);
        pub const PREV_OFFSET: usize = offset_of!(LinkInner, prev);

        pub const fn new() -> Self {
            Self {
                next: None,
                prev: None,
                _pin: PhantomPinned,
            }
        }
    }
}
use inner::LinkInner;

pub struct Link {
    // UnsafeCell here is used to allow the code to access the data mutably.
    // Bare mutable pointer access is unsound without this.
    inner: UnsafeCell<LinkInner>,
}

#[inline]
unsafe fn get_element(inner: &UnsafeCell<LinkInner>, offset: usize) -> Option<NonNull<Link>> {
    let inner_ptr = inner.get() as *const Option<NonNull<Link>>;
    let element_ptr = inner_ptr.byte_add(offset);
    core::ptr::read(element_ptr)
}

#[inline]
unsafe fn set_element(inner: &UnsafeCell<LinkInner>, offset: usize, value: Option<NonNull<Link>>) {
    let inner_ptr = inner.get() as *mut Option<NonNull<Link>>;
    let element_ptr = inner_ptr.byte_add(offset);
    core::ptr::write(element_ptr, value);
}

impl Link {
    pub const fn new() -> Self {
        Self {
            inner: UnsafeCell::new(LinkInner::new()),
        }
    }

    pub fn is_unlinked(&self) -> bool {
        self.get_next().is_none() && self.get_prev().is_none()
    }

    pub fn is_linked(&self) -> bool {
        !self.is_unlinked()
    }

    #[inline]
    fn get_next(&self) -> Option<NonNull<Link>> {
        unsafe { get_element(&self.inner, LinkInner::NEXT_OFFSET) }
    }

    #[inline]
    fn set_next(&mut self, value: Option<NonNull<Link>>) {
        unsafe { set_element(&self.inner, LinkInner::NEXT_OFFSET, value) }
    }

    #[inline]
    fn get_prev(&self) -> Option<NonNull<Link>> {
        unsafe { get_element(&self.inner, LinkInner::PREV_OFFSET) }
    }

    #[inline]
    fn set_prev(&mut self, value: Option<NonNull<Link>>) {
        unsafe { set_element(&self.inner, LinkInner::PREV_OFFSET, value) }
    }
}

impl Default for Link {
    fn default() -> Self {
        Self::new()
    }
}

// `UnsafeList` uses None to mark the beginning and end of lists as opposed to
// pointers to the base list node.  This means that there are never pointers to
// `UnsafeList` and the same care is not needed to avoid mutable references as
// is taken with the `Link` structure.
pub struct UnsafeList<T, A: Adapter> {
    head: Option<NonNull<Link>>,
    tail: Option<NonNull<Link>>,
    _phantom_type: PhantomData<T>,
    _phantom_adapter: PhantomData<A>,
}

pub trait Adapter {
    const LINK_OFFSET: usize;
}

impl<T, A: Adapter> UnsafeList<T, A> {
    pub const fn new() -> Self {
        Self {
            head: None,
            tail: None,
            _phantom_type: PhantomData,
            _phantom_adapter: PhantomData,
        }
    }

    /// # Safety
    /// It is up to the caller to ensure exclusive access to the list and its
    /// members.
    pub unsafe fn is_empty(&self) -> bool {
        self.head.is_none()
    }

    unsafe fn get_link_ptr(element: &T) -> NonNull<Link> {
        let element_ptr: NonNull<Link> = core::mem::transmute::<&T, NonNull<Link>>(element);
        element_ptr.byte_add(A::LINK_OFFSET)
    }

    unsafe fn get_element_ptr(link: NonNull<Link>) -> *const T {
        link.byte_sub(A::LINK_OFFSET).as_ptr() as *const T
    }

    unsafe fn get_element_mut(link: NonNull<Link>) -> *mut T {
        link.byte_sub(A::LINK_OFFSET).as_ptr() as *mut T
    }

    /// unchecked means we don't `assert!((*element_ptr.as_ptr()).is_unlinked());`
    ///
    /// # Safety
    /// It is up to the caller to ensure exclusive access to the list and its
    /// members.
    /// It is up to the caller to ensure the element is not in a list
    pub unsafe fn push_front_unchecked(&mut self, element: &mut T) {
        let element_ptr = Self::get_link_ptr(element);

        // Link up the added element.
        (*element_ptr.as_ptr()).set_next(self.head);
        (*element_ptr.as_ptr()).set_prev(None);

        match self.head {
            // If `head` was `None`, the list is empty and `tail` should point
            // to the added element.
            None => self.tail = Some(element_ptr),

            // If `head` is not `None`, point the previous `head` to the added
            // element.
            Some(head) => (*head.as_ptr()).set_prev(Some(element_ptr)),
        }

        // Finally point `head` to the added element.
        self.head = Some(element_ptr);
    }

    /// unchecked means we don't `assert!((*element_ptr.as_ptr()).is_unlinked());`
    ///
    /// # Safety
    /// It is up to the caller to ensure exclusive access to the list and its
    /// members.
    /// It is up to the caller to ensure the element is not in a list
    pub unsafe fn push_back_unchecked(&mut self, element: &mut T) {
        let element_ptr = Self::get_link_ptr(element);

        // Link up the added element.
        (*element_ptr.as_ptr()).set_next(None);
        (*element_ptr.as_ptr()).set_prev(self.tail);

        match self.tail {
            // If `tail` was `None`, the list is empty and `head` should point
            // to the added element.
            None => self.head = Some(element_ptr),

            // If `tail` is not `None`, point the previous `tail` to the added
            // element.
            Some(tail) => (*tail.as_ptr()).set_next(Some(element_ptr)),
        }

        // Finally point `tail` to the added element.
        self.tail = Some(element_ptr);
    }

    /// unlinks element from the linked list.
    ///
    /// # Safety
    /// It is up to the caller to ensure exclusive access to the list and its
    /// members.
    /// It is up to the caller to ensure the element is in the list
    pub unsafe fn unlink_element(&mut self, element: &T) {
        let element_ptr = Self::get_link_ptr(element);

        let prev = (*element_ptr.as_ptr()).get_prev();
        let next = (*element_ptr.as_ptr()).get_next();

        match prev {
            // Element is at the head of the list
            None => self.head = next,

            // Element has elements before it in the list.
            Some(prev_ptr) => (*prev_ptr.as_ptr()).set_next(next),
        }

        match next {
            // Element is at the tail of the list
            None => self.tail = prev,

            // Element has elements after it in the list.
            Some(next_ptr) => (*next_ptr.as_ptr()).set_prev(prev),
        }
    }

    /// # Safety
    /// It is up to the caller to ensure exclusive access to the list and its
    /// members.
    pub unsafe fn for_each<E, F: FnMut(&T) -> Result<(), E>>(
        &self,
        mut callback: F,
    ) -> Result<(), E> {
        let mut cur = self.head;

        loop {
            let Some(cur_ptr) = cur else {
                break;
            };

            let element = Self::get_element_ptr(cur_ptr);
            callback(&*element)?;

            cur = (*cur_ptr.as_ptr()).get_next();
        }

        Ok(())
    }

    /// Filter iterates over every element in the list calling `callback` on
    /// each one.  If `callback` returns false, the element will be removed
    /// from the list without modifying the element itself.  It is safe to
    /// add the element to the another linked list within `callback` if it
    /// returns false.
    ///
    /// # Safety
    /// It is up to the caller to ensure exclusive access to the list and its
    /// members.
    pub unsafe fn filter<F: FnMut(&mut T) -> bool>(&mut self, mut callback: F) {
        let mut cur = self.head;

        loop {
            let Some(cur_ptr) = cur else {
                break;
            };

            let element = Self::get_element_mut(cur_ptr);

            // Cache the next element so that we don't rely on `element` staying
            // coherent across calls to `callback`.
            let next = (*cur_ptr.as_ptr()).get_next();

            if !callback(&mut *element) {
                self.unlink_element(&*element);
            }

            cur = next;
        }
    }

    /// Return a reference to the first element in the list, clearing the
    /// prev/next fields in the element.
    ///
    /// TODO: reevalulate the lifetime marker here, since it is a lie.
    ///
    /// # Safety
    /// It is up to the caller to ensure exclusive access to the list and its
    /// members.
    pub unsafe fn pop_head<'a>(&mut self) -> Option<&'a mut T> {
        let cur = self.head?;

        let element = Self::get_element_mut(cur);

        self.unlink_element(&*element);
        let element_ptr = Self::get_link_ptr(&*element);
        (*element_ptr.as_ptr()).set_next(None);
        (*element_ptr.as_ptr()).set_prev(None);
        Some(&mut *element)
    }
}

impl<T, A: Adapter> Default for UnsafeList<T, A> {
    fn default() -> Self {
        Self::new()
    }
}
