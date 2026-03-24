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

// Tests that apply across all client channels.

#include "pw_allocator/testing.h"
#include "pw_bluetooth_proxy/h4_packet.h"
#include "pw_bluetooth_proxy/l2cap_channel_common.h"
#include "pw_bluetooth_proxy/proxy_host.h"
#include "pw_bluetooth_proxy_private/test_utils.h"
#include "pw_function/function.h"
#include "pw_multibuf/multibuf.h"

namespace pw::bluetooth::proxy {

namespace {

constexpr uint16_t kConnectionHandle = 123;

// ########## Util

// See BuildOneOfEachChannel
struct OneOfEachChannelParameters {
  Function<void(multibuf::MultiBuf&& payload)>&& receive_fn = nullptr;
  ChannelEventCallback&& event_fn = nullptr;
};

// See BuildOneOfEachChannel
struct OneOfEachChannel {
  OneOfEachChannel(BasicL2capChannel&& basic,
                   L2capCoc&& coc,
                   GattNotifyChannel&& gatt,
                   UniquePtr<ChannelProxy>&& basic_proxy)
      : basic_{std::move(basic)},
        coc_{std::move(coc)},
        gatt_{std::move(gatt)},
        basic_proxy_(std::move(basic_proxy)) {}

  bool AllChannelsClosed() const {
    return GetState(basic_) == L2capChannel::State::kClosed &&
           GetState(coc_) == L2capChannel::State::kClosed &&
           GetState(gatt_) == L2capChannel::State::kClosed;
  }

  static const size_t kNumChannels = 4;

  BasicL2capChannel basic_;
  L2capCoc coc_;
  GattNotifyChannel gatt_;
  UniquePtr<ChannelProxy> basic_proxy_;
};

class ChannelProxyTest : public ProxyHostTest {
 protected:
  // Builds a struct with one of each channel to support tests across all
  // of them.
  //
  // Note, shared_event_fn is a reference (rather than a rvalue) so it can
  // be shared across each channel.
  OneOfEachChannel BuildOneOfEachChannel(
      ProxyHost& proxy,
      ChannelEventCallback& shared_event_fn,
      uint16_t connection_handle = kConnectionHandle) {
    // Each channel its unique cids and its own rvalue lambda which calls the
    // shared_event_fn.
    return OneOfEachChannel(
        BuildBasicL2capChannel(
            proxy,
            {.handle = connection_handle,
             .local_cid = 201,
             .remote_cid = 301,
             .event_fn =
                 [&shared_event_fn](L2capChannelEvent event) {
                   shared_event_fn(event);
                 }}),
        BuildCoc(proxy,
                 {.handle = connection_handle,
                  .local_cid = 202,
                  .remote_cid = 302,
                  .event_fn =
                      [&shared_event_fn](L2capChannelEvent event) {
                        shared_event_fn(event);
                      }}),
        BuildGattNotifyChannel(
            proxy,
            {.handle = connection_handle,
             .event_fn =
                 [&shared_event_fn](L2capChannelEvent event) {
                   shared_event_fn(event);
                 }}),
        BuildBasicModeChannelProxy(
            proxy,
            {.connection_handle = ConnectionHandle{connection_handle},
             .local_channel_id = 204,
             .remote_channel_id = 304,
             .event_fn = [&shared_event_fn](L2capChannelEvent event) {
               shared_event_fn(event);
             }}));
  }
};

// ########## Tests

// Test that each channel type properly send a close event when it is closed
// due to proxy destruction.
// Note BuildOneOfEachChannel (and Build test utils in general) does a move of
// each channel during ctor, so this tests also verifies stop events work after
// a move.
TEST_F(ChannelProxyTest, ChannelsStopOnProxyDestruction) {
  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      [](H4PacketWithHci&&) {});
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      [](H4PacketWithH4&&) {});
  size_t events_received = 0;

  allocator::test::AllocatorForTest<10000> allocator;
  std::optional<ProxyHost> proxy;
  proxy.emplace(std::move(send_to_host_fn),
                std::move(send_to_controller_fn),
                /*le_acl_credits_to_reserve=*/0,
                /*br_edr_acl_credits_to_reserve=*/0,
                &allocator);
  StartDispatcherOnCurrentThread(*proxy);
  PW_TEST_ASSERT_OK(SendLeConnectionCompleteEvent(
      proxy.value(), kConnectionHandle, emboss::StatusCode::SUCCESS));
  PW_TEST_EXPECT_OK(SendReadBufferResponseFromController(*proxy, 10));
  PW_TEST_EXPECT_OK(SendLeReadBufferResponseFromController(*proxy, 10));

  // This event function will be called by each of the channels' event
  // functions.
  ChannelEventCallback shared_event_fn =
      [&events_received](L2capChannelEvent event) {
        ++events_received;
        EXPECT_EQ(event, L2capChannelEvent::kChannelClosedByOther);
      };

  BasicL2capChannel close_first_channel = BuildBasicL2capChannel(
      proxy.value(),
      BasicL2capParameters{
          .handle = kConnectionHandle,
          .event_fn = [&shared_event_fn](L2capChannelEvent event) {
            shared_event_fn(event);
          }});

  OneOfEachChannel channel_struct =
      BuildOneOfEachChannel(proxy.value(), shared_event_fn, kConnectionHandle);

  // Channel already closed before Proxy destruction should not be affected.
  EXPECT_NE(GetState(close_first_channel), L2capChannel::State::kClosed);

  close_first_channel.Close();
  EXPECT_EQ(events_received, 1ul);
  EXPECT_EQ(GetState(close_first_channel), L2capChannel::State::kClosed);

  // Proxy dtor should result in close event for each of
  // the previously still open channels (and they should now be closed).
  proxy.reset();
  EXPECT_EQ(events_received, 1 + OneOfEachChannel::kNumChannels);
  EXPECT_TRUE(channel_struct.AllChannelsClosed());

  // And first channel should remain closed of course.
  EXPECT_EQ(GetState(close_first_channel), L2capChannel::State::kClosed);
}

// Test that each channel type properly send a close event when it is closed
// due to reset.
// Note BuildOneOfEachChannel (and Build test utils in general) does a move of
// each channel during ctor, so this tests also verifies close events work after
// a move.
TEST_F(ChannelProxyTest, ChannelsCloseOnReset) {
  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      [](H4PacketWithHci&&) {});
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      [](H4PacketWithH4&&) {});
  size_t events_received = 0;
  allocator::test::AllocatorForTest<10000> allocator;
  ProxyHost proxy = ProxyHost(std::move(send_to_host_fn),
                              std::move(send_to_controller_fn),
                              /*le_acl_credits_to_reserve=*/0,
                              /*br_edr_acl_credits_to_reserve=*/0,
                              &allocator);
  StartDispatcherOnCurrentThread(proxy);
  PW_TEST_ASSERT_OK(SendLeConnectionCompleteEvent(
      proxy, kConnectionHandle, emboss::StatusCode::SUCCESS));
  PW_TEST_EXPECT_OK(SendReadBufferResponseFromController(proxy, 10));
  PW_TEST_EXPECT_OK(SendLeReadBufferResponseFromController(proxy, 10));

  // This event function will be called by each of the channels' event
  // functions.
  ChannelEventCallback shared_event_fn =
      [&events_received](L2capChannelEvent event) {
        if (++events_received == 1) {
          EXPECT_EQ(event, L2capChannelEvent::kChannelClosedByOther);
        } else {
          EXPECT_EQ(event, L2capChannelEvent::kReset);
        }
      };

  BasicL2capChannel close_first_channel = BuildBasicL2capChannel(
      proxy,
      BasicL2capParameters{
          .event_fn = [&shared_event_fn](L2capChannelEvent event) {
            shared_event_fn(event);
          }});

  // BuildOneOfEachChannel does a move of each channel, so we are testing them
  // after a move.
  OneOfEachChannel channel_struct =
      BuildOneOfEachChannel(proxy, shared_event_fn);

  // Channel already closed before Proxy reset should not be affected.
  close_first_channel.Close();
  EXPECT_EQ(events_received, 1ul);
  EXPECT_EQ(GetState(close_first_channel), L2capChannel::State::kClosed);

  // Proxy reset should result in close event for each of
  // the previously still open channels (and they should now be closed).
  proxy.Reset();
  EXPECT_EQ(events_received, 1 + OneOfEachChannel::kNumChannels);
  EXPECT_TRUE(channel_struct.AllChannelsClosed());

  // And first channel should remain closed of course.
  EXPECT_EQ(GetState(close_first_channel), L2capChannel::State::kClosed);
}

}  // namespace

}  // namespace pw::bluetooth::proxy
