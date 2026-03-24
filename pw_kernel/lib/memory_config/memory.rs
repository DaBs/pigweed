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

const READABLE: usize = 1 << 0;
const WRITEABLE: usize = 1 << 1;
const EXECUTABLE: usize = 1 << 2;
const DEVICE: usize = 1 << 3;

#[derive(Clone, Copy)]
#[allow(dead_code)]
#[repr(usize)]
pub enum MemoryRegionType {
    /// Read Only, Non-Executable data.
    ReadOnlyData = READABLE,

    /// Mode Read/Write, Non-Executable data.
    ReadWriteData = READABLE | WRITEABLE,

    /// Mode Read Only, Executable data.
    ReadOnlyExecutable = READABLE | EXECUTABLE,

    /// Mode Read/Write, Executable data.
    ReadWriteExecutable = READABLE | WRITEABLE | EXECUTABLE,

    /// Device MMIO memory.
    Device = READABLE | WRITEABLE | DEVICE,
}

impl MemoryRegionType {
    #[must_use]
    pub fn has_access(&self, request: Self) -> bool {
        let request_bits = request as usize;
        let self_bits = *self as usize;
        request_bits & self_bits == request_bits
    }

    #[must_use]
    const fn is_mask_set(&self, mask: usize) -> bool {
        *self as usize & mask == mask
    }

    #[must_use]
    pub const fn is_readable(&self) -> bool {
        self.is_mask_set(READABLE)
    }

    #[must_use]
    pub const fn is_writeable(&self) -> bool {
        self.is_mask_set(WRITEABLE)
    }

    #[must_use]
    pub const fn is_executable(&self) -> bool {
        self.is_mask_set(EXECUTABLE)
    }
}

/// Architecture independent memory region description.
///
/// `MemoryRegion` provides an architecture independent way to describe a memory
/// regions and its configuration.
#[allow(dead_code)]
pub struct MemoryRegion {
    /// Type of the memory region
    pub ty: MemoryRegionType,

    /// Start address of the memory region (inclusive)
    pub start: usize,

    /// End address of the memory region (exclusive)
    pub end: usize,
}

impl MemoryRegion {
    #[must_use]
    pub const fn new(ty: MemoryRegionType, start: usize, end: usize) -> Self {
        Self { ty, start, end }
    }

    #[must_use]
    pub fn has_access(&self, region: &Self) -> bool {
        if !(self.start..self.end).contains(&region.start)
            || !(self.start..self.end).contains(&(region.end - 1))
        {
            return false;
        }

        self.ty.has_access(region.ty)
    }

    #[must_use]
    pub fn regions_have_access(regions: &[Self], validation_region: &Self) -> bool {
        regions.iter().fold(false, |acc, region| {
            acc | region.has_access(validation_region)
        })
    }

    /// Calculates the size of the region.
    #[must_use]
    pub const fn size(&self) -> usize {
        self.end - self.start
    }

    /// Returns whether the region is a naturally aligned power of two.  This means the size of
    /// the region is a power of two, and the start address is aligned with respect to size.
    #[must_use]
    pub const fn is_napot(&self) -> bool {
        let size = self.size();
        size.is_power_of_two() && self.start & (size - 1) == 0
    }
}

/// Architecture agnostic operation on memory configuration
pub trait MemoryConfig {
    const KERNEL_THREAD_MEMORY_CONFIG: Self;

    /// Check for access to specified address range.
    ///
    /// Returns true if the memory configuration has access (as specified by
    /// `access_type` to the memory range specified by `start_addr` (inclusive)
    /// and `end_addr` (exclusive).
    #[allow(dead_code)]
    fn range_has_access(
        &self,
        access_type: MemoryRegionType,
        start_addr: usize,
        end_addr: usize,
    ) -> bool;

    /// Check for access to a specified object.
    ///
    /// Returns true if the memory configuration has access (as specified by
    /// `access_type` to the memory pointed to by `object`.
    #[allow(dead_code)]
    fn has_access<T: Sized>(&self, access_type: MemoryRegionType, object: *const T) -> bool {
        self.range_has_access(
            access_type,
            object as usize,
            object as usize + core::mem::size_of::<T>(),
        )
    }
}

#[cfg(test)]
mod tests {
    use unittest::test;

    use super::{MemoryRegion, MemoryRegionType};

    #[test]
    fn memory_type_is_readable_returns_correct_value() -> unittest::Result<()> {
        unittest::assert_true!(MemoryRegionType::ReadOnlyData.is_readable());
        unittest::assert_true!(MemoryRegionType::ReadWriteData.is_readable());
        unittest::assert_true!(MemoryRegionType::ReadOnlyExecutable.is_readable());
        unittest::assert_true!(MemoryRegionType::ReadWriteExecutable.is_readable());
        unittest::assert_true!(MemoryRegionType::Device.is_readable());
        Ok(())
    }

    #[test]
    fn memory_type_is_writeable_returns_correct_value() -> unittest::Result<()> {
        unittest::assert_false!(MemoryRegionType::ReadOnlyData.is_writeable());
        unittest::assert_true!(MemoryRegionType::ReadWriteData.is_writeable());
        unittest::assert_false!(MemoryRegionType::ReadOnlyExecutable.is_writeable());
        unittest::assert_true!(MemoryRegionType::ReadWriteExecutable.is_writeable());
        unittest::assert_true!(MemoryRegionType::Device.is_writeable());
        Ok(())
    }

    #[test]
    fn memory_type_is_executable_returns_correct_value() -> unittest::Result<()> {
        unittest::assert_false!(MemoryRegionType::ReadOnlyData.is_executable());
        unittest::assert_false!(MemoryRegionType::ReadWriteData.is_executable());
        unittest::assert_true!(MemoryRegionType::ReadOnlyExecutable.is_executable());
        unittest::assert_true!(MemoryRegionType::ReadWriteExecutable.is_executable());
        unittest::assert_false!(MemoryRegionType::Device.is_executable());
        Ok(())
    }

    #[test]
    fn memory_type_has_access_returns_correct_value() -> unittest::Result<()> {
        // These checks are implemented using bit masks/math so the full space
        // is validated explicitly.
        unittest::assert_true!(
            MemoryRegionType::ReadOnlyData.has_access(MemoryRegionType::ReadOnlyData)
        );
        unittest::assert_false!(
            MemoryRegionType::ReadOnlyData.has_access(MemoryRegionType::ReadWriteData)
        );
        unittest::assert_false!(
            MemoryRegionType::ReadOnlyData.has_access(MemoryRegionType::ReadOnlyExecutable)
        );
        unittest::assert_false!(
            MemoryRegionType::ReadOnlyData.has_access(MemoryRegionType::ReadWriteExecutable)
        );
        unittest::assert_false!(
            MemoryRegionType::ReadOnlyData.has_access(MemoryRegionType::Device)
        );

        unittest::assert_true!(
            MemoryRegionType::ReadWriteData.has_access(MemoryRegionType::ReadOnlyData)
        );
        unittest::assert_true!(
            MemoryRegionType::ReadWriteData.has_access(MemoryRegionType::ReadWriteData)
        );
        unittest::assert_false!(
            MemoryRegionType::ReadWriteData.has_access(MemoryRegionType::ReadOnlyExecutable)
        );
        unittest::assert_false!(
            MemoryRegionType::ReadWriteData.has_access(MemoryRegionType::ReadWriteExecutable)
        );
        unittest::assert_false!(
            MemoryRegionType::ReadWriteData.has_access(MemoryRegionType::Device)
        );

        unittest::assert_true!(
            MemoryRegionType::ReadOnlyExecutable.has_access(MemoryRegionType::ReadOnlyData)
        );
        unittest::assert_false!(
            MemoryRegionType::ReadOnlyExecutable.has_access(MemoryRegionType::ReadWriteData)
        );
        unittest::assert_true!(
            MemoryRegionType::ReadOnlyExecutable.has_access(MemoryRegionType::ReadOnlyExecutable)
        );
        unittest::assert_false!(
            MemoryRegionType::ReadOnlyExecutable.has_access(MemoryRegionType::ReadWriteExecutable)
        );
        unittest::assert_false!(
            MemoryRegionType::ReadOnlyExecutable.has_access(MemoryRegionType::Device)
        );

        unittest::assert_true!(
            MemoryRegionType::ReadWriteExecutable.has_access(MemoryRegionType::ReadOnlyData)
        );
        unittest::assert_true!(
            MemoryRegionType::ReadWriteExecutable.has_access(MemoryRegionType::ReadWriteData)
        );
        unittest::assert_true!(
            MemoryRegionType::ReadWriteExecutable.has_access(MemoryRegionType::ReadOnlyExecutable)
        );
        unittest::assert_true!(
            MemoryRegionType::ReadWriteExecutable.has_access(MemoryRegionType::ReadWriteExecutable)
        );
        unittest::assert_false!(
            MemoryRegionType::ReadWriteExecutable.has_access(MemoryRegionType::Device)
        );

        unittest::assert_true!(MemoryRegionType::Device.has_access(MemoryRegionType::ReadOnlyData));
        unittest::assert_true!(
            MemoryRegionType::Device.has_access(MemoryRegionType::ReadWriteData)
        );
        unittest::assert_false!(
            MemoryRegionType::Device.has_access(MemoryRegionType::ReadOnlyExecutable)
        );
        unittest::assert_false!(
            MemoryRegionType::Device.has_access(MemoryRegionType::ReadWriteExecutable)
        );
        unittest::assert_true!(MemoryRegionType::Device.has_access(MemoryRegionType::Device));
        Ok(())
    }

    #[test]
    fn memory_region_allows_access_to_full_region() -> unittest::Result<()> {
        unittest::assert_true!(
            MemoryRegion::new(MemoryRegionType::ReadOnlyData, 0x1000_0000, 0x2000_0000).has_access(
                &MemoryRegion::new(MemoryRegionType::ReadOnlyData, 0x1000_0000, 0x2000_0000)
            )
        );

        Ok(())
    }

    #[test]
    fn memory_region_allows_access_to_beginning_region() -> unittest::Result<()> {
        unittest::assert_true!(
            MemoryRegion::new(MemoryRegionType::ReadOnlyData, 0x1000_0000, 0x2000_0000).has_access(
                &MemoryRegion::new(MemoryRegionType::ReadOnlyData, 0x1000_0000, 0x1500_0000)
            )
        );

        Ok(())
    }

    #[test]
    fn memory_region_allows_access_to_middle_region() -> unittest::Result<()> {
        unittest::assert_true!(
            MemoryRegion::new(MemoryRegionType::ReadOnlyData, 0x1000_0000, 0x2000_0000).has_access(
                &MemoryRegion::new(MemoryRegionType::ReadOnlyData, 0x1200_0000, 0x1500_0000)
            )
        );

        Ok(())
    }

    #[test]
    fn memory_region_allows_access_to_ending_region() -> unittest::Result<()> {
        unittest::assert_true!(
            MemoryRegion::new(MemoryRegionType::ReadOnlyData, 0x1000_0000, 0x2000_0000).has_access(
                &MemoryRegion::new(MemoryRegionType::ReadOnlyData, 0x1500_0000, 0x2000_0000)
            )
        );

        Ok(())
    }

    #[test]
    fn memory_region_disallows_access_region_before_start() -> unittest::Result<()> {
        unittest::assert_false!(
            MemoryRegion::new(MemoryRegionType::ReadOnlyData, 0x1000_0000, 0x2000_0000).has_access(
                &MemoryRegion::new(MemoryRegionType::ReadOnlyData, 0x0fff_ffff, 0x2000_0000)
            )
        );

        Ok(())
    }

    #[test]
    fn memory_region_disallows_access_region_after_end() -> unittest::Result<()> {
        unittest::assert_false!(
            MemoryRegion::new(MemoryRegionType::ReadOnlyData, 0x1000_0000, 0x2000_0000).has_access(
                &MemoryRegion::new(MemoryRegionType::ReadOnlyData, 0x1000_0000, 0x2000_0001)
            )
        );

        Ok(())
    }

    #[test]
    fn memory_region_disallows_access_to_superset() -> unittest::Result<()> {
        unittest::assert_false!(
            MemoryRegion::new(MemoryRegionType::ReadOnlyData, 0x1000_0000, 0x2000_0000).has_access(
                &MemoryRegion::new(MemoryRegionType::ReadOnlyData, 0x0fff_ffff, 0x2000_0001)
            )
        );

        Ok(())
    }

    #[test]
    fn memory_regions_allows_access_to_subregion_regions() -> unittest::Result<()> {
        const REGIONS: &[MemoryRegion] = &[
            MemoryRegion::new(MemoryRegionType::ReadOnlyData, 0x1000_0000, 0x2000_0000),
            MemoryRegion::new(MemoryRegionType::ReadOnlyData, 0x2000_0000, 0x3000_0000),
        ];

        unittest::assert_true!(MemoryRegion::regions_have_access(
            REGIONS,
            &MemoryRegion::new(MemoryRegionType::ReadOnlyData, 0x1000_0000, 0x2000_0000)
        ));
        unittest::assert_true!(MemoryRegion::regions_have_access(
            REGIONS,
            &MemoryRegion::new(MemoryRegionType::ReadOnlyData, 0x2000_0000, 0x3000_0000)
        ));

        Ok(())
    }

    #[test]
    fn memory_regions_disallows_access_spanning_multiple_regions() -> unittest::Result<()> {
        const REGIONS: &[MemoryRegion] = &[
            MemoryRegion::new(MemoryRegionType::ReadOnlyData, 0x1000_0000, 0x2000_0000),
            MemoryRegion::new(MemoryRegionType::ReadOnlyData, 0x2000_0000, 0x3000_0000),
        ];

        unittest::assert_false!(MemoryRegion::regions_have_access(
            REGIONS,
            &MemoryRegion::new(MemoryRegionType::ReadOnlyData, 0x1000_0000, 0x3000_0000)
        ));

        Ok(())
    }

    #[test]
    fn memory_regions_allows_access_to_sub_region_type() -> unittest::Result<()> {
        const REGIONS: &[MemoryRegion] = &[
            MemoryRegion::new(
                MemoryRegionType::ReadOnlyExecutable,
                0x1000_0000,
                0x2000_0000,
            ),
            MemoryRegion::new(MemoryRegionType::ReadWriteData, 0x2000_0000, 0x3000_0000),
        ];

        unittest::assert_true!(MemoryRegion::regions_have_access(
            REGIONS,
            &MemoryRegion::new(MemoryRegionType::ReadOnlyData, 0x1000_0000, 0x2000_0000)
        ));
        unittest::assert_true!(MemoryRegion::regions_have_access(
            REGIONS,
            &MemoryRegion::new(MemoryRegionType::ReadOnlyData, 0x2000_0000, 0x3000_0000)
        ));

        Ok(())
    }

    #[test]
    fn memory_regions_disallows_access_to_wrong_region_type() -> unittest::Result<()> {
        const REGIONS: &[MemoryRegion] = &[
            MemoryRegion::new(
                MemoryRegionType::ReadOnlyExecutable,
                0x1000_0000,
                0x2000_0000,
            ),
            MemoryRegion::new(MemoryRegionType::ReadWriteData, 0x2000_0000, 0x3000_0000),
        ];

        unittest::assert_false!(MemoryRegion::regions_have_access(
            REGIONS,
            &MemoryRegion::new(MemoryRegionType::ReadWriteData, 0x1000_0000, 0x2000_0000)
        ));
        unittest::assert_false!(MemoryRegion::regions_have_access(
            REGIONS,
            &MemoryRegion::new(
                MemoryRegionType::ReadOnlyExecutable,
                0x2000_0000,
                0x3000_0000
            )
        ));

        Ok(())
    }
}
