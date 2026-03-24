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

#include "pw_bluetooth_proxy/gatt/gatt.h"

#include <mutex>

#include "lib/stdcompat/utility.h"
#include "pw_assert/check.h"
#include "pw_bluetooth/att.emb.h"
#include "pw_bluetooth/emboss_util.h"
#include "pw_bluetooth/l2cap_frames.emb.h"
#include "pw_containers/algorithm.h"
#include "pw_log/log.h"
#include "pw_span/cast.h"

namespace pw::bluetooth::proxy::gatt {

void Client::Delegate::HandleNotification(ConnectionHandle connection_handle,
                                          AttributeHandle value_handle,
                                          multibuf::MultiBuf&& value) {
  DoHandleNotification(connection_handle, value_handle, std::move(value));
}

void Client::Delegate::HandleError(Error error,
                                   ConnectionHandle connection_handle) {
  DoHandleError(error, connection_handle);
}

Client::Client()
    : client_id_(internal::ClientId{0}),
      connection_handle_(ConnectionHandle{0}),
      gatt_(nullptr) {}

Client::~Client() { Close(); }

Client::Client(Client&& other) { Move(std::move(other)); }

Client& Client::operator=(Client&& other) {
  Move(std::move(other));
  return *this;
}

void Client::Move(Client&& other) {
  if (gatt_ != nullptr) {
    Close();
  }
  client_id_ = other.client_id_;
  connection_handle_ = other.connection_handle_;
  gatt_ = std::exchange(other.gatt_, nullptr);
}

Client::Client(internal::ClientId client_id,
               ConnectionHandle connection_handle,
               Gatt& gatt)
    : client_id_(client_id),
      connection_handle_(connection_handle),
      gatt_(&gatt) {}

void Client::Close() {
  if (gatt_ == nullptr) {
    // Already closed
    return;
  }
  gatt_->UnregisterClient(client_id_, connection_handle_);
  gatt_ = nullptr;
}

Status Client::InterceptNotification(AttributeHandle value_handle) {
  return gatt_->InterceptNotification(
      client_id_, connection_handle_, value_handle);
}

Status Client::CancelInterceptNotification(AttributeHandle value_handle) {
  return gatt_->CancelInterceptNotification(
      client_id_, connection_handle_, value_handle);
}

void Server::Delegate::HandleWriteWithoutResponse(
    ConnectionHandle connection_handle,
    AttributeHandle value_handle,
    multibuf::MultiBuf&& value) {
  DoHandleWriteWithoutResponse(
      connection_handle, value_handle, std::move(value));
}

void Server::Delegate::HandleWriteAvailable(
    ConnectionHandle connection_handle) {
  DoHandleWriteAvailable(connection_handle);
}

void Server::Delegate::HandleError(Error error,
                                   ConnectionHandle connection_handle) {
  DoHandleError(error, connection_handle);
}

Server::Server(internal::ServerId server_id,
               ConnectionHandle connection_handle,
               Gatt& gatt)
    : server_id_(server_id),
      connection_handle_(connection_handle),
      gatt_(&gatt) {}

Server::~Server() { Close(); }

Server::Server(Server&& other) { Move(std::move(other)); }

Server& Server::operator=(Server&& other) {
  Move(std::move(other));
  return *this;
}

void Server::Move(Server&& other) {
  if (gatt_ != nullptr) {
    Close();
  }
  server_id_ = other.server_id_;
  connection_handle_ = other.connection_handle_;
  gatt_ = std::exchange(other.gatt_, nullptr);
}

void Server::Close() {
  if (gatt_ == nullptr) {
    // Already closed
    return;
  }
  gatt_->UnregisterServer(server_id_, connection_handle_);
  gatt_ = nullptr;
}

Status Server::AddCharacteristic(CharacteristicInfo characteristic) {
  if (gatt_ == nullptr) {
    return Status::FailedPrecondition();
    ;
  }
  return gatt_->AddCharacteristic(
      server_id_, connection_handle_, characteristic);
}

Status Server::RemoveCharacteristic(CharacteristicInfo characteristic) {
  if (gatt_ == nullptr) {
    return Status::FailedPrecondition();
  }
  return gatt_->RemoveCharacteristic(
      server_id_, connection_handle_, characteristic);
}

StatusWithMultiBuf Server::SendNotification(AttributeHandle value_handle,
                                            multibuf::MultiBuf&& value) {
  if (gatt_ == nullptr) {
    return {.status = Status::FailedPrecondition(), .buf = std::move(value)};
  }
  return gatt_->SendNotification(
      server_id_, connection_handle_, value_handle, std::move(value));
}

Gatt::Gatt(L2capChannelManagerInterface& l2cap,
           Allocator& allocator,
           multibuf::MultiBufAllocator& multibuf_allocator)
    : l2cap_(l2cap),
      allocator_(allocator),
      multibuf_allocator_(multibuf_allocator) {}

Gatt::~Gatt() { ResetConnections(); }

Result<Client> Gatt::CreateClient(ConnectionHandle connection_handle,
                                  Client::Delegate& delegate) {
  std::lock_guard lock(mutex_);

  if (next_id_ == std::numeric_limits<uint16_t>::max()) {
    return Status::ResourceExhausted();
  }

  internal::ClientId client_id{next_id_++};
  UniquePtr<ClientState> client =
      allocator_.MakeUnique<ClientState>(client_id, delegate);
  if (client == nullptr) {
    return Status::Unavailable();
  }

  auto conn_iter = FindOrInterceptAttChannel(connection_handle);
  if (conn_iter == connections_.end()) {
    return Status::Unavailable();
  }

  auto [_, inserted] = conn_iter->clients.insert(*client.Release());
  PW_CHECK(inserted);

  return Client(client_id, connection_handle, *this);
}

Result<Server> Gatt::CreateServer(
    ConnectionHandle connection_handle,
    span<const CharacteristicInfo> characteristics,
    Server::Delegate& delegate) {
  std::lock_guard lock(mutex_);

  auto conn_iter = connections_.find(cpp23::to_underlying(connection_handle));

  // Ensure that no characteristics are already registered.
  if (conn_iter != connections_.end()) {
    for (auto characteristic : characteristics) {
      if (conn_iter->characteristics.find(
              cpp23::to_underlying(characteristic.value_handle)) !=
          conn_iter->characteristics.end()) {
        return Status::AlreadyExists();
      }
    }
  }

  if (next_id_ == std::numeric_limits<uint16_t>::max()) {
    return Status::ResourceExhausted();
  }
  internal::ServerId server_id{next_id_++};
  UniquePtr<ServerState> server =
      allocator_.MakeUnique<ServerState>(server_id, delegate);
  if (server == nullptr) {
    return Status::ResourceExhausted();
  }

  IntrusiveMap<std::underlying_type_t<AttributeHandle>, CharacteristicState>
      characteristics_temp;
  for (CharacteristicInfo characteristic : characteristics) {
    UniquePtr<CharacteristicState> characteristic_ptr =
        allocator_.MakeUnique<CharacteristicState>(characteristic.value_handle,
                                                   server_id);
    if (characteristic_ptr == nullptr) {
      DrainCharacteristics(characteristics_temp);
      return Status::ResourceExhausted();
    }
    auto [_, inserted] =
        characteristics_temp.insert(*characteristic_ptr.Release());
    PW_CHECK(inserted);
  }

  conn_iter = FindOrInterceptAttChannel(connection_handle);
  if (conn_iter == connections_.end()) {
    while (!characteristics_temp.empty()) {
      CharacteristicState& state = *characteristics_temp.begin();
      characteristics_temp.erase(state);
      allocator_.Delete(&state);
    }
    return Status::Unavailable();
  }

  auto [_, inserted] = conn_iter->servers.insert(*server.Release());
  PW_CHECK(inserted);

  conn_iter->characteristics.merge(characteristics_temp);

  return Server(server_id, connection_handle, *this);
}

void Gatt::UnregisterClient(internal::ClientId client_id,
                            ConnectionHandle connection_handle) {
  Client::Delegate* delegate = nullptr;
  {
    std::lock_guard lock(mutex_);
    auto conn_iter = connections_.find(cpp23::to_underlying(connection_handle));
    if (conn_iter == connections_.end()) {
      return;
    }
    auto client_iter = conn_iter->clients.find(cpp23::to_underlying(client_id));
    if (client_iter == conn_iter->clients.end()) {
      return;
    }
    for (auto iter = conn_iter->intercepted_notifications_.begin();
         iter != conn_iter->intercepted_notifications_.end();) {
      if (iter->second == client_id) {
        iter = conn_iter->intercepted_notifications_.erase(iter);
        continue;
      }
      ++iter;
    }
    ClientState& client = *client_iter;
    delegate = &client.delegate;
    conn_iter->clients.erase(client_iter);
    allocator_.Delete(&client);
  }

  // Call outside of lock to avoid deadlock.
  delegate->HandleError(Error::kClosedByClient, connection_handle);

  // Leave connection/channel in connections_ map even if there are no clients
  // remaining.
}

void Gatt::UnregisterServer(internal::ServerId server_id,
                            ConnectionHandle connection_handle) {
  Server::Delegate* delegate = nullptr;
  {
    std::lock_guard queue_lock(write_available_mutex_);
    std::lock_guard lock(mutex_);
    auto conn_iter = connections_.find(cpp23::to_underlying(connection_handle));
    if (conn_iter == connections_.end()) {
      return;
    }

    auto server_iter = conn_iter->servers.find(cpp23::to_underlying(server_id));
    if (server_iter == conn_iter->servers.end()) {
      return;
    }
    ServerState& server = *server_iter;

    // Erase all characteristics owned by the server.
    for (auto iter = conn_iter->characteristics.begin();
         iter != conn_iter->characteristics.end();) {
      auto& characteristic = *iter;
      if (characteristic.server_id != server_id) {
        ++iter;
        continue;
      }
      iter = conn_iter->characteristics.erase(characteristic);
      allocator_.Delete(&characteristic);
    }

    delegate = &server.delegate;
    conn_iter->servers.erase(server_iter);
    allocator_.Delete(&server);

    // Clean up write_available_queue_.
    for (auto iter = write_available_queue_.begin();
         iter != write_available_queue_.end();) {
      if (iter->server_id == server_id) {
        iter = write_available_queue_.erase(iter);
      }
      ++iter;
    }
  }

  // Call outside of lock to avoid deadlock.
  delegate->HandleError(Error::kClosedByClient, connection_handle);

  // Leave connection/channel in connections_ map even if there are no servers
  // remaining.
}

Status Gatt::InterceptNotification(internal::ClientId client,
                                   ConnectionHandle connection_handle,
                                   AttributeHandle value_handle) {
  std::lock_guard lock(mutex_);

  auto conn_iter = connections_.find(cpp23::to_underlying(connection_handle));
  if (conn_iter == connections_.end()) {
    return Status::NotFound();
  }

  auto notification_iter =
      pw::containers::Find(conn_iter->intercepted_notifications_,
                           std::make_pair(value_handle, client));
  if (notification_iter != conn_iter->intercepted_notifications_.end()) {
    return Status::AlreadyExists();
  }

  bool success = conn_iter->intercepted_notifications_.try_emplace_back(
      value_handle, client);

  if (!success) {
    return Status::Unavailable();
  }

  return OkStatus();
}

Status Gatt::CancelInterceptNotification(internal::ClientId client,
                                         ConnectionHandle connection_handle,
                                         AttributeHandle value_handle) {
  std::lock_guard lock(mutex_);
  auto conn_iter = connections_.find(cpp23::to_underlying(connection_handle));
  if (conn_iter == connections_.end()) {
    return Status::NotFound();
  }

  auto notification_iter =
      pw::containers::Find(conn_iter->intercepted_notifications_,
                           std::make_pair(value_handle, client));
  if (notification_iter == conn_iter->intercepted_notifications_.end()) {
    return Status::NotFound();
  }

  conn_iter->intercepted_notifications_.erase(notification_iter);
  return OkStatus();
}

Result<UniquePtr<ChannelProxy>> Gatt::InterceptAttChannel(
    ConnectionHandle connection_handle) {
  return l2cap_.InterceptBasicModeChannel(
      connection_handle,
      static_cast<uint16_t>(emboss::L2capFixedCid::LE_U_ATTRIBUTE_PROTOCOL),
      static_cast<uint16_t>(emboss::L2capFixedCid::LE_U_ATTRIBUTE_PROTOCOL),
      AclTransportType::kLe,
      pw::bind_member<&Gatt::OnSpanReceivedFromController>(this),
      pw::bind_member<&Gatt::OnSpanReceivedFromHost>(this),
      [this, connection_handle](L2capChannelEvent event) {
        OnL2capEvent(event, connection_handle);
      });
}

bool Gatt::OnSpanReceivedFromController(ConstByteSpan payload,
                                        ConnectionHandle connection_handle,
                                        uint16_t /*local_channel_id*/,
                                        uint16_t /*remote_channel_id*/) {
  std::lock_guard lock(mutex_);
  auto conn_iter = connections_.find(cpp23::to_underlying(connection_handle));
  if (conn_iter == connections_.end()) {
    return false;
  }

  if (payload.size() < sizeof(emboss::AttOpcode)) {
    return false;
  }

  const emboss::AttOpcode op_code{static_cast<uint8_t>(payload[0])};

  PW_MODIFY_DIAGNOSTICS_PUSH();
  PW_MODIFY_DIAGNOSTIC(ignored, "-Wswitch-enum");
  switch (op_code) {
    case emboss::AttOpcode::ATT_WRITE_CMD:
      return OnAttWriteCmdFromController(payload, conn_iter);
    case emboss::AttOpcode::ATT_HANDLE_VALUE_NTF:
      return OnAttHandleValueNtfFromController(payload, conn_iter);
    default:
      return false;
  }
  PW_MODIFY_DIAGNOSTICS_POP();
}

bool Gatt::OnSpanReceivedFromHost(ConstByteSpan /*payload*/,
                                  ConnectionHandle /*connection_handle*/,
                                  uint16_t /*local_channel_id*/,
                                  uint16_t /*remote_channel_id*/) {
  // Intercepting outbound ATT packets is not supported.
  return false;
}

void Gatt::OnL2capEvent(L2capChannelEvent event,
                        ConnectionHandle connection_handle) {
  if (event == L2capChannelEvent::kReset) {
    ResetConnections();
  } else if (event == L2capChannelEvent::kChannelClosedByOther) {
    OnChannelClosedEvent(connection_handle);
  } else if (event == L2capChannelEvent::kWriteAvailable) {
    OnWriteAvailable(connection_handle);
  }
}

void Gatt::OnChannelClosedEvent(ConnectionHandle connection_handle) {
  IntrusiveMap<std::underlying_type_t<internal::ClientId>, ClientState>
      closing_clients;
  IntrusiveMap<std::underlying_type_t<internal::ServerId>, ServerState>
      closing_servers;

  {
    std::lock_guard queue_lock(write_available_mutex_);
    std::lock_guard lock(mutex_);

    auto conn_iter = connections_.find(cpp23::to_underlying(connection_handle));
    if (conn_iter == connections_.end()) {
      return;
    }

    Connection& conn = *conn_iter;
    closing_clients.swap(conn.clients);
    closing_servers.swap(conn.servers);
    DrainCharacteristics(conn.characteristics);

    connections_.erase(conn_iter);
    allocator_.Delete(&conn);

    // Clean up write_available_queue_
    for (auto iter = write_available_queue_.begin();
         iter != write_available_queue_.end();) {
      if (iter->connection_handle == connection_handle) {
        iter = write_available_queue_.erase(iter);
      }
      ++iter;
    }
  }

  // Notify delegates outside of mutex to avoid deadlock.
  while (!closing_clients.empty()) {
    auto client_iter = closing_clients.begin();
    ClientState& client = *client_iter;
    client.delegate.HandleError(Error::kDisconnection, connection_handle);
    closing_clients.erase(client_iter);
    allocator_.Delete(&client);
  }

  // Notify delegates outside of mutex to avoid deadlock.
  while (!closing_servers.empty()) {
    auto server_iter = closing_servers.begin();
    ServerState& server = *server_iter;
    server.delegate.HandleError(Error::kDisconnection, connection_handle);
    closing_servers.erase(server_iter);
    allocator_.Delete(&server);
  }
}

void Gatt::OnWriteAvailable(ConnectionHandle connection_handle) {
  std::lock_guard queue_lock(write_available_mutex_);
  {
    std::lock_guard lock(mutex_);

    auto conn_iter = connections_.find(cpp23::to_underlying(connection_handle));
    if (conn_iter == connections_.end()) {
      return;
    }

    for (ServerState& server : conn_iter->servers) {
      bool inserted = write_available_queue_.try_emplace_back(
          QueuedWriteAvailable{internal::ServerId{server.key()},
                               connection_handle,
                               &server.delegate});
      if (!inserted) {
        PW_LOG_WARN(
            "Cannot allocate write_available_queue_ item, unable to notify "
            "more servers");
        break;
      }
    }
  }

  // Call delegate outside of mutex_ lock so that clients can call
  // Server::SendNotification() without deadlock.
  for (uint16_t i = 0; i < write_available_queue_.size();) {
    if (write_available_queue_[i].connection_handle == connection_handle) {
      write_available_queue_[i].delegate->HandleWriteAvailable(
          connection_handle);
      write_available_queue_[i] = std::move(write_available_queue_.back());
      write_available_queue_.pop_back();
    } else {
      ++i;
    }
  }
}

void Gatt::ResetConnections() {
  IntrusiveMap<std::underlying_type_t<ConnectionHandle>, Connection>
      closed_connections;

  {
    std::lock_guard queue_lock(write_available_mutex_);
    std::lock_guard lock(mutex_);
    closed_connections.swap(connections_);
    write_available_queue_.clear();
  }

  // Notify delegates outside of mutex to avoid deadlock.
  while (!closed_connections.empty()) {
    auto conn_iter = closed_connections.begin();
    Connection& conn = *conn_iter;

    while (!conn.clients.empty()) {
      auto client_iter = conn.clients.begin();
      ClientState& client = *client_iter;
      client.delegate.HandleError(Error::kReset, ConnectionHandle{conn.key()});
      conn.clients.erase(client_iter);
      allocator_.Delete(&client);
    }

    while (!conn.servers.empty()) {
      auto server_iter = conn.servers.begin();
      ServerState& server = *server_iter;
      server.delegate.HandleError(Error::kReset, ConnectionHandle{conn.key()});
      conn.servers.erase(server_iter);
      allocator_.Delete(&server);
    }

    DrainCharacteristics(conn.characteristics);

    closed_connections.erase(conn_iter);
    allocator_.Delete(&conn);
  }
}

StatusWithMultiBuf Gatt::SendNotification(internal::ServerId server_id,
                                          ConnectionHandle connection_handle,
                                          AttributeHandle value_handle,
                                          multibuf::MultiBuf&& value) {
  std::lock_guard lock(mutex_);

  auto conn_iter = connections_.find(cpp23::to_underlying(connection_handle));
  if (conn_iter == connections_.end()) {
    PW_LOG_WARN(
        "Attempt to send GATT notification for non-offloaded connection");
    return {Status::FailedPrecondition(), std::move(value)};
  }

  auto char_iter =
      conn_iter->characteristics.find(cpp23::to_underlying(value_handle));
  if (char_iter == conn_iter->characteristics.end()) {
    PW_LOG_WARN(
        "Attempt to send GATT notification for non-offloaded attribute");
    return {Status::FailedPrecondition(), std::move(value)};
  }

  if (char_iter->server_id != server_id) {
    PW_LOG_WARN(
        "Attempt to send GATT notification for attribute owned by different "
        "server");
    return {Status::InvalidArgument(), std::move(value)};
  }

  const size_t packet_size =
      emboss::AttHandleValueNtf::MinSizeInBytes() + value.size();
  std::optional<multibuf::MultiBuf> multibuf_result =
      multibuf_allocator_.AllocateContiguous(packet_size);
  if (!multibuf_result.has_value()) {
    PW_LOG_WARN("Failed to allocate buffer for TX GATT notification");
    return {Status::ResourceExhausted(), std::move(value)};
  }
  multibuf::MultiBuf multibuf = std::move(*multibuf_result);
  span<uint8_t> multibuf_span =
      span_cast<uint8_t>(multibuf.ContiguousSpan().value());

  Result<emboss::AttHandleValueNtfWriter> writer =
      MakeEmbossWriter<emboss::AttHandleValueNtfWriter>(value.size(),
                                                        &multibuf_span);
  PW_CHECK(writer.ok());
  writer->attribute_opcode().Write(emboss::AttOpcode::ATT_HANDLE_VALUE_NTF);
  writer->attribute_handle().Write(cpp23::to_underlying(value_handle));

  span<uint8_t> attribute_bytes(
      writer->attribute_value().BackingStorage().data(),
      writer->attribute_value().SizeInBytes());
  PW_CHECK_OK(value.CopyTo(as_writable_bytes(attribute_bytes)));

  StatusWithMultiBuf write_result =
      conn_iter->att_channel->Write(std::move(multibuf));
  if (!write_result.status.ok()) {
    return {write_result.status, std::move(value)};
  }
  return {OkStatus()};
}

Gatt::ConnectionMap::iterator Gatt::FindOrInterceptAttChannel(
    ConnectionHandle connection_handle) {
  auto conn_iter = connections_.find(cpp23::to_underlying(connection_handle));
  if (conn_iter != connections_.end()) {
    return conn_iter;
  }
  UniquePtr<Connection> connection =
      allocator_.MakeUnique<Connection>(connection_handle, allocator_);
  if (connection == nullptr) {
    return connections_.end();
  }

  Result<UniquePtr<ChannelProxy>> channel_result =
      InterceptAttChannel(connection_handle);
  if (!channel_result.ok()) {
    return connections_.end();
  }

  connection->att_channel = std::move(channel_result.value());
  auto [iter, inserted] = connections_.insert(*connection.Release());
  PW_CHECK(inserted);
  return iter;
}

void Gatt::DrainCharacteristics(CharacteristicMap& characteristics) {
  while (!characteristics.empty()) {
    auto char_iter = characteristics.begin();
    CharacteristicState& char_state = *char_iter;
    characteristics.erase(char_iter);
    allocator_.Delete(&char_state);
  }
}

bool Gatt::OnAttHandleValueNtfFromController(
    ConstByteSpan payload, ConnectionMap::iterator conn_iter) {
  const size_t attribute_size =
      payload.size() - emboss::AttHandleValueNtf::MinSizeInBytes();

  Result<emboss::AttHandleValueNtfView> view =
      MakeEmbossView<emboss::AttHandleValueNtfView>(
          attribute_size,
          reinterpret_cast<const uint8_t*>(payload.data()),
          payload.size());
  if (!view.ok()) {
    PW_LOG_WARN("Received invalid ATT_HANDLE_VALUE_NTF");
    return false;
  }

  PW_CHECK(view->attribute_opcode().Read() ==
           emboss::AttOpcode::ATT_HANDLE_VALUE_NTF);

  AttributeHandle att_handle{view->attribute_handle().Read()};

  bool intercepted = false;
  for (auto& [intercepted_handle, client_id] :
       conn_iter->intercepted_notifications_) {
    if (att_handle != intercepted_handle) {
      continue;
    }
    intercepted = true;

    auto client_iter = conn_iter->clients.find(cpp23::to_underlying(client_id));
    PW_CHECK(client_iter != conn_iter->clients.end());

    std::optional<multibuf::MultiBuf> buffer =
        multibuf_allocator_.AllocateContiguous(attribute_size);
    if (!buffer.has_value()) {
      PW_LOG_WARN("Failed to allocate multibuf for attribute value");
      return true;
    }

    pw::span<const uint8_t> backing_storage(
        view->attribute_value().BackingStorage().data(),
        view->attribute_value().SizeInBytes());
    auto bytes_copied = buffer->CopyFrom(as_bytes(backing_storage));
    PW_CHECK(bytes_copied.ok());
    PW_CHECK_UINT_EQ(bytes_copied.size(), attribute_size);

    client_iter->delegate.HandleNotification(
        ConnectionHandle{conn_iter->key()}, att_handle, std::move(*buffer));
  }

  return intercepted;
}

bool Gatt::OnAttWriteCmdFromController(ConstByteSpan payload,
                                       ConnectionMap::iterator conn_iter) {
  if (payload.size() < emboss::AttWriteCmd::MinSizeInBytes()) {
    PW_LOG_WARN("Received invalid ATT_WRITE_CMD");
    return false;
  }
  const size_t attribute_size =
      payload.size() - emboss::AttWriteCmd::MinSizeInBytes();

  Result<emboss::AttWriteCmdView> view =
      MakeEmbossView<emboss::AttWriteCmdView>(
          attribute_size,
          reinterpret_cast<const uint8_t*>(payload.data()),
          payload.size());
  if (!view.ok()) {
    PW_LOG_WARN("Received invalid ATT_WRITE_CMD");
    return false;
  }

  PW_CHECK(view->attribute_opcode().Read() == emboss::AttOpcode::ATT_WRITE_CMD);

  AttributeHandle att_handle{view->attribute_handle().Read()};

  auto char_iter =
      conn_iter->characteristics.find(cpp23::to_underlying(att_handle));
  if (char_iter == conn_iter->characteristics.end()) {
    return false;
  }

  auto server_iter =
      conn_iter->servers.find(cpp23::to_underlying(char_iter->server_id));
  PW_CHECK(server_iter != conn_iter->servers.end());

  std::optional<multibuf::MultiBuf> buffer =
      multibuf_allocator_.AllocateContiguous(attribute_size);
  if (!buffer.has_value()) {
    PW_LOG_WARN("Failed to allocate multibuf for attribute value");
    return true;
  }

  pw::span<const uint8_t> backing_storage(
      view->attribute_value().BackingStorage().data(),
      view->attribute_value().SizeInBytes());
  auto bytes_copied = buffer->CopyFrom(as_bytes(backing_storage));
  PW_CHECK(bytes_copied.ok());
  PW_CHECK_UINT_EQ(bytes_copied.size(), attribute_size);

  server_iter->delegate.HandleWriteWithoutResponse(
      ConnectionHandle{conn_iter->key()}, att_handle, std::move(*buffer));

  // The command was intercepted.
  return true;
}

Status Gatt::AddCharacteristic(internal::ServerId server_id,
                               ConnectionHandle connection_handle,
                               CharacteristicInfo characteristic) {
  std::lock_guard lock(mutex_);
  auto conn_iter = connections_.find(cpp23::to_underlying(connection_handle));
  if (conn_iter == connections_.end()) {
    return Status::FailedPrecondition();
  }

  auto server_iter = conn_iter->servers.find(cpp23::to_underlying(server_id));
  if (server_iter == conn_iter->servers.end()) {
    return Status::FailedPrecondition();
  }

  auto char_iter = conn_iter->characteristics.find(
      cpp23::to_underlying(characteristic.value_handle));
  if (char_iter != conn_iter->characteristics.end()) {
    return Status::AlreadyExists();
  }

  UniquePtr<CharacteristicState> characteristic_ptr =
      allocator_.MakeUnique<CharacteristicState>(characteristic.value_handle,
                                                 server_id);
  if (characteristic_ptr == nullptr) {
    return Status::ResourceExhausted();
  }
  auto [_, inserted] =
      conn_iter->characteristics.insert(*characteristic_ptr.Release());
  PW_CHECK(inserted);
  return OkStatus();
}

Status Gatt::RemoveCharacteristic(internal::ServerId server_id,
                                  ConnectionHandle connection_handle,
                                  CharacteristicInfo characteristic) {
  std::lock_guard lock(mutex_);
  auto conn_iter = connections_.find(cpp23::to_underlying(connection_handle));
  if (conn_iter == connections_.end()) {
    return Status::FailedPrecondition();
  }

  auto char_iter = conn_iter->characteristics.find(
      cpp23::to_underlying(characteristic.value_handle));
  if (char_iter == conn_iter->characteristics.end() ||
      char_iter->server_id != server_id) {
    return Status::NotFound();
  }

  conn_iter->characteristics.erase(char_iter);
  return OkStatus();
}

}  // namespace pw::bluetooth::proxy::gatt
