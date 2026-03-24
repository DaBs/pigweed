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
#pragma once

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/dt-bindings/i2c/i2c.h>

#include <cstdint>

#include "pw_i2c/initiator.h"
#include "pw_sync/lock_annotations.h"
#include "pw_sync/mutex.h"

namespace pw::i2c {

class ZephyrInitiator final : public Initiator {
 public:
  // I2C bitrates defined in zephyr/dt-bindings/i2c/i2c.h
  enum class BitRate : uint32_t {
    // Use I2C_SPEED_SET(...) to set enum value so it can be used by
    // i2c_configure(...)
    kStandard100kHz = I2C_SPEED_SET(I2C_SPEED_STANDARD),
    kFast400kHz = I2C_SPEED_SET(I2C_SPEED_FAST),
    kFastPlus1MHz = I2C_SPEED_SET(I2C_SPEED_FAST_PLUS),
    kHigh3p4MHz = I2C_SPEED_SET(I2C_SPEED_HIGH),
    kUltra5MHz = I2C_SPEED_SET(I2C_SPEED_ULTRA),
  };

  ZephyrInitiator(const struct device* dev)
      : Initiator(Initiator::Feature::kStandard),
        dev_(dev),
        config_(I2C_MODE_CONTROLLER | I2C_SPEED_SET(I2C_SPEED_DT)) {}

  // Use this constructor to explicitly set the bitrate, for example, when the
  // vendor doesn't support using "I2C_SPEED_SET(I2C_SPEED_DT)".
  ZephyrInitiator(const struct device* dev, BitRate bitrate)
      : Initiator(Initiator::Feature::kStandard),
        dev_(dev),
        config_(I2C_MODE_CONTROLLER | static_cast<uint32_t>(bitrate)) {}

  void Enable() PW_LOCKS_EXCLUDED(mutex_);

 private:
  Status DoWriteReadFor(Address device_address,
                        ConstByteSpan tx_buffer,
                        ByteSpan rx_buffer,
                        chrono::SystemClock::duration timeout) override
      PW_LOCKS_EXCLUDED(mutex_);

  sync::Mutex mutex_;
  const struct device* dev_ PW_GUARDED_BY(mutex_);
  const uint32_t config_ PW_GUARDED_BY(mutex_);
};
}  // namespace pw::i2c
