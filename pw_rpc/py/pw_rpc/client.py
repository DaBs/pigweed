# Copyright 2020 The Pigweed Authors
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.
"""Provides a pw_rpc client for Python."""

from __future__ import annotations

import abc
from dataclasses import dataclass
import logging
from typing import (
    Any,
    Callable,
    Collection,
    Iterable,
    Iterator,
)

from google.protobuf.message import DecodeError, Message
from pw_status import Status

from pw_rpc import descriptors, packets
from pw_rpc.descriptors import Channel, Service, Method, PendingRpc
from pw_rpc.internal.packet_pb2 import PacketType, RpcPacket

_LOG = logging.getLogger(__package__)

# Calls with ID of `kOpenCallId` were unrequested, and are updated to have the
# call ID of the first matching request.
LEGACY_OPEN_CALL_ID: int = 0
OPEN_CALL_ID: int = (2**32) - 1

_MAX_CALL_ID: int = 1 << 21


class Error(Exception):
    """Error from incorrectly using the RPC client classes."""


class _PendingRpcMetadata:
    def __init__(self, context: object):
        self.context = context


class PendingRpcs:
    """Tracks pending RPCs and encodes outgoing RPC packets."""

    def __init__(self) -> None:
        self._pending: dict[PendingRpc, _PendingRpcMetadata] = {}
        # We skip call_id = 0 in order to avoid LEGACY_OPEN_CALL_ID.
        self._next_call_id: int = 1

    def allocate_call_id(self) -> int:
        call_id = self._next_call_id
        self._next_call_id = (self._next_call_id + 1) % _MAX_CALL_ID
        # We skip call_id = 0 in order to avoid LEGACY_OPEN_CALL_ID.
        if self._next_call_id == 0:
            self._next_call_id = 1
        return call_id

    def request(
        self, rpc: PendingRpc, request: Message | None, context: object
    ) -> bytes:
        """Starts the provided RPC and returns the encoded packet to send."""
        # Ensure that every context is a unique object by wrapping it in a list.
        self.open(rpc, context)
        return packets.encode_request(rpc, request)

    def send_request(
        self, rpc: PendingRpc, request: Message | None, context: object
    ) -> None:
        """Starts the provided RPC and sends the request packet to the channel.

        Returns:
          the previous context object or None
        """
        self.open(rpc, context)
        packet = packets.encode_request(rpc, request)
        rpc.channel.output(packet)

    def open(self, rpc: PendingRpc, context: object) -> None:
        """Creates a context for an RPC, but does not invoke it.

        open() can be used to receive streaming responses to an RPC that was not
        invoked by this client. For example, a server may stream logs with a
        server streaming RPC prior to any clients invoking it.

        Returns:
          the previous context object or None
        """
        _LOG.debug('Starting %s', rpc)
        metadata = _PendingRpcMetadata(context)

        if self._pending.setdefault(rpc, metadata) is not metadata:
            # If the context was not added, the RPC was already pending.
            raise Error(
                f'Sent request for {rpc}, but it is already pending! '
                'Cancel the RPC before invoking it again'
            )

    def send_client_stream(self, rpc: PendingRpc, message: Message) -> None:
        if rpc not in self._pending:
            raise Error(f'Attempt to send client stream for inactive RPC {rpc}')

        rpc.channel.output(packets.encode_client_stream(rpc, message))

    def send_client_stream_end(self, rpc: PendingRpc) -> None:
        if rpc not in self._pending:
            raise Error(
                f'Attempt to send client stream end for inactive RPC {rpc}'
            )

        rpc.channel.output(packets.encode_client_stream_end(rpc))

    def cancel(self, rpc: PendingRpc) -> bytes:
        """Cancels the RPC.

        Returns:
          The CLIENT_ERROR packet to send.

        Raises:
          KeyError if the RPC is not pending
        """
        _LOG.debug('Cancelling %s', rpc)
        del self._pending[rpc]

        return packets.encode_cancel(rpc)

    def send_cancel(self, rpc: PendingRpc) -> bool:
        """Calls cancel and sends the cancel packet, if any, to the channel."""
        try:
            packet = self.cancel(rpc)
        except KeyError:
            return False

        if packet:
            rpc.channel.output(packet)

        return True

    def _match_unrequested_rpcs(
        self, rpc: PendingRpc, completed: bool
    ) -> _PendingRpcMetadata | None:
        # If the inbound packet is unrequested, route to any matching call.
        # If both the client and server calls use the open ID, they would have
        # matched in the initial lookup before this function is called.
        if rpc.call_id in (OPEN_CALL_ID, LEGACY_OPEN_CALL_ID):
            for pending, context in self._pending.items():
                if rpc.matches_channel_service_method(pending):
                    if completed:
                        del self._pending[pending]

                    return context

        # Otherwise, look for an existing open call that matches. If one is
        # found, the unrequested call adopts the inbound call's ID.
        for pending in self._pending:
            if (
                pending.call_id == OPEN_CALL_ID
                and rpc.matches_channel_service_method(pending)
            ):
                # Change the call ID in the PendingRpc object. The PendingRpc
                # MUST be removed from the self._pending dict first since it is
                # hashable.
                #
                # TODO: https://pwbug.dev/359401616 - Changing a hashable object
                # is not good, but the ClientImpl abstraction boundary makes
                # updating the call's PendingRpc instance difficult. This code
                # should be updated after the client is refactored.
                context = self._pending.pop(pending)
                object.__setattr__(pending, 'call_id', rpc.call_id)
                if not completed:
                    self._pending[pending] = context
                return context

        return None

    def get_pending(
        self, rpc: PendingRpc, completed: bool
    ) -> _PendingRpcMetadata | None:
        """Gets the pending RPC's context. If status is set, clears the RPC."""
        # Look up the RPC. If there is no match, check for unrequested RPCs.
        if (meta := self._pending.get(rpc)) is None:
            meta = self._match_unrequested_rpcs(rpc, completed)
        elif completed:
            del self._pending[rpc]

        return meta


class ClientImpl(abc.ABC):
    """The internal interface of the RPC client.

    This interface defines the semantics for invoking an RPC on a particular
    client.
    """

    def __init__(self) -> None:
        self.client: Client | None = None
        self.rpcs: PendingRpcs | None = None

    @abc.abstractmethod
    def method_client(self, channel: Channel, method: Method) -> Any:
        """Returns an object that invokes a method using the given channel."""

    @abc.abstractmethod
    def handle_response(
        self,
        rpc: PendingRpc,
        context: Any,
        payload: Any,
    ) -> Any:
        """Handles a response from the RPC server.

        Args:
          rpc: Information about the pending RPC
          context: Arbitrary context object associated with the pending RPC
          payload: A protobuf message
        """

    @abc.abstractmethod
    def handle_completion(
        self,
        rpc: PendingRpc,
        context: Any,
        status: Status,
    ) -> Any:
        """Handles the successful completion of an RPC.

        Args:
          rpc: Information about the pending RPC
          context: Arbitrary context object associated with the pending RPC
          status: Status returned from the RPC
        """

    @abc.abstractmethod
    def handle_error(
        self,
        rpc: PendingRpc,
        context,
        status: Status,
    ):
        """Handles the abnormal termination of an RPC.

        args:
          rpc: Information about the pending RPC
          context: Arbitrary context object associated with the pending RPC
          status: which error occurred
        """


class ServiceClient(descriptors.ServiceAccessor):
    """Navigates the methods in a service provided by a ChannelClient."""

    def __init__(
        self, client_impl: ClientImpl, channel: Channel, service: Service
    ):
        super().__init__(
            {
                method: client_impl.method_client(channel, method)
                for method in service.methods
            },
            as_attrs='members',
        )

        self._channel = channel
        self._service = service

    def __repr__(self) -> str:
        return (
            f'Service({self._service.full_name!r}, '
            f'methods={[m.name for m in self._service.methods]}, '
            f'channel={self._channel.id})'
        )

    def __str__(self) -> str:
        return str(self._service)


class Services(descriptors.ServiceAccessor[ServiceClient]):
    """Navigates the services provided by a ChannelClient."""

    def __init__(
        self, client_impl, channel: Channel, services: Collection[Service]
    ):
        super().__init__(
            {s: ServiceClient(client_impl, channel, s) for s in services},
            as_attrs='packages',
        )

        self._channel = channel
        self._services = services

    def __repr__(self) -> str:
        return (
            f'Services(channel={self._channel.id}, '
            f'services={[s.full_name for s in self._services]})'
        )


def _decode_status(rpc: PendingRpc, packet) -> Status | None:
    if packet.type == PacketType.SERVER_STREAM:
        return None

    try:
        return Status(packet.status)
    except ValueError:
        _LOG.warning('Illegal status code %d for %s', packet.status, rpc)
        return Status.UNKNOWN


def _decode_payload(rpc: PendingRpc, packet) -> Message | None:
    if packet.type == PacketType.SERVER_ERROR:
        return None

    # Server streaming RPCs do not send a payload with their RESPONSE packet.
    if packet.type == PacketType.RESPONSE and rpc.method.server_streaming:
        return None

    return packets.decode_payload(packet, rpc.method.response_type)


@dataclass(frozen=True, eq=False)
class ChannelClient:
    """RPC services and methods bound to a particular channel.

    RPCs are invoked through service method clients. These may be accessed via
    the `rpcs` member. Service methods use a fully qualified name: package,
    service, method. Service methods may be selected as attributes or by
    indexing the rpcs member by service and method name or ID.

      # Access the service method client as an attribute
      rpc = client.channel(1).rpcs.the.package.FooService.SomeMethod

      # Access the service method client by string name
      rpc = client.channel(1).rpcs[foo_service_id]['SomeMethod']

    RPCs may also be accessed from their canonical name.

      # Access the service method client from its full name:
      rpc = client.channel(1).method('the.package.FooService/SomeMethod')

      # Using a . instead of a / is also supported:
      rpc = client.channel(1).method('the.package.FooService.SomeMethod')

    The ClientImpl class determines the type of the service method client. A
    synchronous RPC client might return a callable object, so an RPC could be
    invoked directly (e.g. rpc(field1=123, field2=b'456')).
    """

    client: Client
    channel: Channel
    rpcs: Services

    def method(self, method_name: str):
        """Returns a method client matching the given name.

        Args:
          method_name: name as package.Service/Method or package.Service.Method.

        Raises:
          ValueError: the method name is not properly formatted
          KeyError: the method is not present
        """
        return descriptors.get_method(self.rpcs, method_name)

    def services(self) -> Iterator:
        return iter(self.rpcs)

    def methods(self) -> Iterator:
        """Iterates over all method clients in this ChannelClient."""
        for service_client in self.rpcs:
            yield from service_client

    def __repr__(self) -> str:
        return (
            f'ChannelClient(channel={self.channel.id}, '
            f'services={[str(s) for s in self.services()]})'
        )


def _update_for_backwards_compatibility(
    rpc: PendingRpc, packet: RpcPacket
) -> None:
    """Adapts server streaming RPC packets to the updated protocol if needed."""
    # The protocol changes only affect server streaming RPCs.
    if rpc.method.type is not Method.Type.SERVER_STREAMING:
        return

    # Prior to the introduction of SERVER_STREAM packets, RESPONSE packets with
    # a payload were used instead. If a non-zero payload is present, assume this
    # RESPONSE is equivalent to a SERVER_STREAM packet.
    #
    # Note that the payload field is not 'optional', so an empty payload is
    # equivalent to a payload that happens to encode to zero bytes. This would
    # only affect server streaming RPCs on the old protocol that intentionally
    # send empty payloads, which will not be an issue in practice.
    if packet.type == PacketType.RESPONSE and packet.payload:
        packet.type = PacketType.SERVER_STREAM


class Client:
    """Sends requests and handles responses for a set of channels.

    RPC invocations occur through a ChannelClient.

    Users may set an optional response_callback that is called before processing
    every response or server stream RPC packet.
    """

    @classmethod
    def from_modules(
        cls, impl: ClientImpl, channels: Iterable[Channel], modules: Iterable
    ):
        return cls(
            impl,
            channels,
            (
                Service.from_descriptor(service)
                for module in modules
                for service in module.DESCRIPTOR.services_by_name.values()
            ),
        )

    def __init__(
        self,
        impl: ClientImpl,
        channels: Iterable[Channel],
        services: Iterable[Service],
    ):
        self._impl = impl
        self._impl.client = self
        self._impl.rpcs = PendingRpcs()

        self.services = descriptors.Services(services)

        self._channels_by_id = {
            channel.id: ChannelClient(
                self, channel, Services(self._impl, channel, self.services)
            )
            for channel in channels
        }

        # Optional function called before processing every non-error RPC packet.
        self.response_callback: (
            Callable[[PendingRpc, Any, Status | None], Any] | None
        ) = None

    def channel(self, channel_id: int | None = None) -> ChannelClient:
        """Returns a ChannelClient, which is used to call RPCs on a channel.

        If no channel is provided, the first channel is used.
        """
        if channel_id is None:
            return next(iter(self._channels_by_id.values()))

        return self._channels_by_id[channel_id]

    def channels(self) -> Iterable[ChannelClient]:
        """Accesses the ChannelClients in this client."""
        return self._channels_by_id.values()

    def method(self, method_name: str) -> Method:
        """Returns a Method matching the given name.

        Args:
          method_name: name as package.Service/Method or package.Service.Method.

        Raises:
          ValueError: the method name is not properly formatted
          KeyError: the method is not present
        """
        return descriptors.get_method(self.services, method_name)

    def methods(self) -> Iterator[Method]:
        """Iterates over all Methods supported by this client."""
        for service in self.services:
            yield from service.methods

    def process_packet(self, pw_rpc_raw_packet_data: bytes) -> Status:
        """Processes an incoming packet.

        Args:
          pw_rpc_raw_packet_data: raw binary data for exactly one RPC packet

        Returns:
          OK - the packet was processed by this client
          DATA_LOSS - the packet could not be decoded
          INVALID_ARGUMENT - the packet is for a server, not a client
          NOT_FOUND - the packet's channel ID is not known to this client
        """
        try:
            packet = packets.decode(pw_rpc_raw_packet_data)
        except DecodeError as err:
            _LOG.warning('Failed to decode packet: %s', err)
            _LOG.debug('Raw packet: %r', pw_rpc_raw_packet_data)
            return Status.DATA_LOSS

        if packets.for_server(packet):
            return Status.INVALID_ARGUMENT

        try:
            channel_client = self._channels_by_id[packet.channel_id]
        except KeyError:
            _LOG.warning('Unrecognized channel ID %d', packet.channel_id)
            return Status.NOT_FOUND

        try:
            rpc = self._look_up_service_and_method(packet, channel_client)
        except ValueError as err:
            _send_client_error(channel_client, packet, Status.NOT_FOUND)
            _LOG.warning('%s', err)
            return Status.OK

        _update_for_backwards_compatibility(rpc, packet)

        if packet.type not in (
            PacketType.RESPONSE,
            PacketType.SERVER_STREAM,
            PacketType.SERVER_ERROR,
        ):
            _LOG.error('%s: unexpected PacketType %s', rpc, packet.type)
            _LOG.debug('Packet:\n%s', packet)
            return Status.OK

        status = _decode_status(rpc, packet)

        try:
            payload = _decode_payload(rpc, packet)
        except DecodeError as err:
            _send_client_error(channel_client, packet, Status.DATA_LOSS)
            _LOG.warning(
                'Failed to decode %s response for %s: %s',
                rpc.method.response_type.DESCRIPTOR.full_name,
                rpc.method.full_name,
                err,
            )
            _LOG.debug('Raw payload: %s', packet.payload)

            # Make this an error packet so the error handler is called.
            packet.type = PacketType.SERVER_ERROR
            status = Status.DATA_LOSS

        # If set, call the response callback with non-error packets.
        if self.response_callback and packet.type != PacketType.SERVER_ERROR:
            self.response_callback(  # pylint: disable=not-callable
                rpc, payload, status
            )

        assert self._impl.rpcs
        meta = self._impl.rpcs.get_pending(rpc, status is not None)

        if meta is None:
            _send_client_error(
                channel_client, packet, Status.FAILED_PRECONDITION
            )
            _LOG.debug('Discarding response for %s, which is not pending', rpc)
            return Status.OK

        if packet.type == PacketType.SERVER_ERROR:
            assert status is not None and not status.ok()
            _LOG.warning('%s: invocation failed with %s', rpc, status)
            self._impl.handle_error(rpc, meta.context, status)
            return Status.OK

        if payload is not None:
            self._impl.handle_response(rpc, meta.context, payload)
        if status is not None:
            self._impl.handle_completion(rpc, meta.context, status)

        return Status.OK

    def _look_up_service_and_method(
        self, packet: RpcPacket, channel_client: ChannelClient
    ) -> PendingRpc:
        # Protobuf is sometimes silly so the 32 bit python bindings return
        # signed values from `fixed32` fields. Let's convert back to unsigned.
        # b/239712573
        service_id = packet.service_id & 0xFFFFFFFF
        try:
            service = self.services[service_id]
        except KeyError:
            raise ValueError(f'Unrecognized service ID {service_id}')

        # See above, also for b/239712573
        method_id = packet.method_id & 0xFFFFFFFF
        try:
            method = service.methods[method_id]
        except KeyError:
            raise ValueError(
                f'No method ID {method_id} in service {service.name}'
            )

        return PendingRpc(
            channel_client.channel, service, method, packet.call_id
        )

    def __repr__(self) -> str:
        return (
            f'pw_rpc.Client(channels={list(self._channels_by_id)}, '
            f'services={[s.full_name for s in self.services]})'
        )


def _send_client_error(
    client: ChannelClient, packet: RpcPacket, error: Status
) -> None:
    # Never send responses to SERVER_ERRORs.
    if packet.type != PacketType.SERVER_ERROR:
        client.channel.output(packets.encode_client_error(packet, error))
