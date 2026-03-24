# Copyright 2023 The Pigweed Authors
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
"""Tests for pw_console.socket_client"""

import socket
import unittest


from pw_console import socket_client


class TestSocketClient(unittest.TestCase):
    """Tests for SocketClient."""

    def test_parse_config_default(self) -> None:
        config = "default"
        with unittest.mock.patch.object(
            socket_client.SocketClient, 'connect', return_value=None
        ):
            client = socket_client.SocketClient(config)
            self.assertEqual(
                client._socket_init_args,  # pylint: disable=protected-access
                (socket.AF_INET6, socket.SOCK_STREAM),
            )
            self.assertEqual(
                client._address,  # pylint: disable=protected-access
                (
                    socket_client.SocketClient.DEFAULT_SOCKET_SERVER,
                    socket_client.SocketClient.DEFAULT_SOCKET_PORT,
                ),
            )

    def test_parse_config_unix_file(self) -> None:
        # Skip test if UNIX sockets are not supported.
        if not hasattr(socket, 'AF_UNIX'):
            return

        config = 'file:fake_file_path'
        with unittest.mock.patch.object(
            socket_client.SocketClient, 'connect', return_value=None
        ):
            client = socket_client.SocketClient(config)
            self.assertEqual(
                client._socket_init_args,  # pylint: disable=protected-access
                (
                    socket.AF_UNIX,  # pylint: disable=no-member
                    socket.SOCK_STREAM,
                ),
            )
            self.assertEqual(
                client._address,  # pylint: disable=protected-access
                'fake_file_path',
            )

    def _check_config_parsing(
        self, config: str, expected_address: str, expected_port: int
    ) -> None:
        with unittest.mock.patch.object(
            socket_client.SocketClient, 'connect', return_value=None
        ):
            fake_getaddrinfo_return_value = [
                (socket.AF_INET6, socket.SOCK_STREAM, 0, None, None)
            ]
            with unittest.mock.patch.object(
                socket,
                'getaddrinfo',
                return_value=fake_getaddrinfo_return_value,
            ) as mock_getaddrinfo:
                client = socket_client.SocketClient(config)
                mock_getaddrinfo.assert_called_with(
                    expected_address, expected_port, type=socket.SOCK_STREAM
                )
                # Assert the init args are what is returned by ``getaddrinfo``
                # not necessarily the correct ones, since this test should not
                # perform any network action.
                self.assertEqual(
                    client._socket_init_args,  # pylint: disable=protected-access
                    (
                        socket.AF_INET6,
                        socket.SOCK_STREAM,
                    ),
                )

    def test_parse_config_ipv4_domain(self) -> None:
        self._check_config_parsing(
            config='file.com/some_long/path:80',
            expected_address='file.com/some_long/path',
            expected_port=80,
        )

    def test_parse_config_ipv4_domain_no_port(self) -> None:
        self._check_config_parsing(
            config='file.com/some/path',
            expected_address='file.com/some/path',
            expected_port=socket_client.SocketClient.DEFAULT_SOCKET_PORT,
        )

    def test_parse_config_ipv4_address(self) -> None:
        self._check_config_parsing(
            config='8.8.8.8:8080',
            expected_address='8.8.8.8',
            expected_port=8080,
        )

    def test_parse_config_ipv4_address_no_port(self) -> None:
        self._check_config_parsing(
            config='8.8.8.8',
            expected_address='8.8.8.8',
            expected_port=socket_client.SocketClient.DEFAULT_SOCKET_PORT,
        )

    def test_parse_config_ipv6_domain(self) -> None:
        self._check_config_parsing(
            config='[file.com/some_long/path]:80',
            expected_address='file.com/some_long/path',
            expected_port=80,
        )

    def test_parse_config_ipv6_domain_no_port(self) -> None:
        self._check_config_parsing(
            config='[file.com/some/path]',
            expected_address='file.com/some/path',
            expected_port=socket_client.SocketClient.DEFAULT_SOCKET_PORT,
        )

    def test_parse_config_ipv6_address(self) -> None:
        self._check_config_parsing(
            config='[2001:4860:4860::8888:8080]:666',
            expected_address='2001:4860:4860::8888:8080',
            expected_port=666,
        )

    def test_parse_config_ipv6_address_no_port(self) -> None:
        self._check_config_parsing(
            config='[2001:4860:4860::8844]',
            expected_address='2001:4860:4860::8844',
            expected_port=socket_client.SocketClient.DEFAULT_SOCKET_PORT,
        )

    def test_parse_config_ipv6_local(self) -> None:
        self._check_config_parsing(
            config='[fe80::100%eth0]:80',
            expected_address='fe80::100%eth0',
            expected_port=80,
        )

    def test_parse_config_ipv6_local_no_port(self) -> None:
        self._check_config_parsing(
            config='[fe80::100%eth0]',
            expected_address='fe80::100%eth0',
            expected_port=socket_client.SocketClient.DEFAULT_SOCKET_PORT,
        )

    def test_parse_config_ipv6_local_windows(self) -> None:
        self._check_config_parsing(
            config='[fe80::100%4]:80',
            expected_address='fe80::100%4',
            expected_port=80,
        )

    def test_parse_config_ipv6_local_no_port_windows(self) -> None:
        self._check_config_parsing(
            config='[fe80::100%4]',
            expected_address='fe80::100%4',
            expected_port=socket_client.SocketClient.DEFAULT_SOCKET_PORT,
        )

    def test_close(self) -> None:
        with unittest.mock.patch.object(
            socket_client.SocketClient, 'connect', return_value=None
        ):
            client = socket_client.SocketClient("default")
            # Manually set connected to True since we mocked connect
            client._connected = True  # pylint: disable=protected-access

            # Mock the underlying socket
            mock_socket = unittest.mock.MagicMock()
            client.socket = mock_socket

            client.close()

            mock_socket.close.assert_called_once()
            self.assertFalse(
                client._connected  # pylint: disable=protected-access
            )

            # Call close again, should not call mock_socket.close again
            client.close()
            mock_socket.close.assert_called_once()

    def test_del_calls_close(self) -> None:  # pylint: disable=no-self-use
        with unittest.mock.patch.object(
            socket_client.SocketClient, 'connect', return_value=None
        ):
            client = socket_client.SocketClient("default")
            with unittest.mock.patch.object(client, 'close') as mock_close:
                client.__del__()  # pylint: disable=unnecessary-dunder-call
                mock_close.assert_called_once()

    def test_handle_disconnect_calls_close(
        self,
    ) -> None:  # pylint: disable=no-self-use
        on_disconnect_called = False

        def on_disconnect(_client):
            nonlocal on_disconnect_called
            on_disconnect_called = True

        with unittest.mock.patch.object(
            socket_client.SocketClient, 'connect', return_value=None
        ):
            client = socket_client.SocketClient(
                "default", on_disconnect=on_disconnect
            )
            with unittest.mock.patch.object(client, 'close') as mock_close:
                client._handle_disconnect()  # pylint: disable=protected-access
                mock_close.assert_called_once()
                self.assertTrue(on_disconnect_called)


if __name__ == '__main__':
    unittest.main()
