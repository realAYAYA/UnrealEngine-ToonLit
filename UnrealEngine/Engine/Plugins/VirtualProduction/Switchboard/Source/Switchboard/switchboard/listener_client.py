# Copyright Epic Games, Inc. All Rights Reserved.

from collections import deque
import datetime
import select
import socket
from threading import Thread
import traceback
from typing import Callable, Dict, Optional
import uuid

from PySide2 import QtCore

from . import message_protocol
from .switchboard_logging import LOGGER


class ListenerQtHandler(QtCore.QObject):
    listener_connecting = QtCore.Signal(object)
    listener_connected = QtCore.Signal(object)
    listener_connection_failed = QtCore.Signal(object)


class ListenerClient(object):
    '''Connects to a server running SwitchboardListener.

    Runs a thread to service its socket, and upon receiving complete messages,
    invokes a handler callback from that thread (`handle_connection()`).

    `disconnect_delegate` is invoked on `disconnect()` or on socket errors.

    Handlers in the `delegates` map are passed a dict containing the entire
    JSON response from the listener, routed according to the "command" field
    string value. All new messages and handlers should follow this pattern.

    The other, legacy (VCS/file) delegates are each passed different (or no)
    arguments; for details, see `route_message()`.
    '''

    KEEPALIVE_INTERVAL_SEC = 1.0
    SELECT_TIMEOUT_SEC = 0.1
    EXTRA_HITCH_TOLERANCE_SEC = 0.4
    HITCH_THRESHOLD_SEC = (KEEPALIVE_INTERVAL_SEC + SELECT_TIMEOUT_SEC
                           + EXTRA_HITCH_TOLERANCE_SEC)

    def __init__(self, address, port, buffer_size=1024):
        self.address = address
        self.port = port
        self.buffer_size = buffer_size

        self.listener_qt_handler = ListenerQtHandler()

        self.message_queue = deque()
        self.close_socket = False

        self.socket = None
        self.handle_connection_thread = None

        # TODO: Consider converting these delegates to Signals and sending dict

        self.disconnect_delegate = None

        self.command_accepted_delegate = None
        self.command_declined_delegate = None

        self.vcs_init_completed_delegate = None
        self.vcs_init_failed_delegate = None
        self.vcs_report_revision_completed_delegate = None
        self.vcs_report_revision_failed_delegate = None
        self.vcs_sync_completed_delegate = None
        self.vcs_sync_failed_delegate = None

        self.send_file_completed_delegate = None
        self.send_file_failed_delegate = None
        self.receive_file_completed_delegate = None
        self.receive_file_failed_delegate = None

        self.delegates: Dict[str, Optional[Callable[[Dict], None]]] = {
            "state": None,
            "get sync status": None,
        }

        self.last_activity = datetime.datetime.now()

    @property
    def server_address(self):
        if self.address:
            return (self.address, self.port)
        return None

    @property
    def is_connected(self):
        # I ran into an issue where running disconnect in a thread was causing the socket maintain it's reference
        # But self.socket.getpeername() fails because socket is sent to none. I am assuming that is due
        # it python's threading. Adding a try except to handle this
        try:
            if self.socket and self.socket.getpeername():
                return True
        except:
            return False
        return False

    def connect(self, address=None):
        '''
        Initiates a connection.

        Calling this function starts the connection setup on a background
        thread. The caller should connect slots to the listener_qt_handler's
        "listener_connected" and "listener_connection_failed" signals to
        determine whether the connection was created successfully.
        '''
        self.disconnect()

        if address:
            self.address = address
        elif not self.address:
            LOGGER.debug('No address has been set. Cannot connect')
            self.listener_qt_handler.listener_connection_failed.emit(self)
            return False

        self.close_socket = False
        self.last_activity = datetime.datetime.now()

        self.listener_qt_handler.listener_connecting.emit(self)
        establish_connection_thread = Thread(target=self._establish_connection)
        establish_connection_thread.start()

        return True

    def _establish_connection(self):
        try:
            LOGGER.info(f"Connecting to {self.address}:{self.port}")

            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.connect(self.server_address)

            # Create a thread that waits for messages from the server
            self.handle_connection_thread = Thread(target=self.handle_connection)
            self.handle_connection_thread.start()

        except OSError:
            LOGGER.error(f"Socket error: {self.address}:{self.port}")
            self.socket = None
            self.listener_qt_handler.listener_connection_failed.emit(self)
            return False

        self.listener_qt_handler.listener_connected.emit(self)
        return True

    def disconnect(self, unexpected=False, exception=None):
        if self.is_connected:
            _, msg = message_protocol.create_disconnect_message()
            self.send_message(msg)

            self.close_socket = True
            self.handle_connection_thread.join()

    def update_last_activity(self):
        now = datetime.datetime.now()
        delta_sec = (now - self.last_activity).total_seconds()
        self.last_activity = now
        if delta_sec > self.HITCH_THRESHOLD_SEC:
            LOGGER.warning(
                f'Hitch detected; {delta_sec:.1f} seconds since last keepalive'
                f' to {self.address}')

    def handle_connection(self):
        buffer = []

        while self.is_connected:
            try:
                rlist = [self.socket]
                wlist = []
                xlist = []

                read_sockets, _, _ = select.select(rlist, wlist, xlist,
                                                   self.SELECT_TIMEOUT_SEC)

                while len(self.message_queue):
                    message_bytes = self.message_queue.pop()
                    self.update_last_activity()
                    self.socket.sendall(message_bytes)
                
                for rs in read_sockets:
                    received_data = rs.recv(self.buffer_size).decode()
                    self.process_received_data(buffer, received_data)

                delta = datetime.datetime.now() - self.last_activity
                if delta.total_seconds() > self.KEEPALIVE_INTERVAL_SEC:
                    _, msg = message_protocol.create_keep_alive_message()
                    self.update_last_activity()
                    self.socket.sendall(msg)

                if self.close_socket and len(self.message_queue) == 0:
                    self.socket.shutdown(socket.SHUT_RDWR)
                    self.socket.close()
                    self.socket = None

                    if self.disconnect_delegate:
                        self.disconnect_delegate(unexpected=False, exception=None)

                    break

            except ConnectionResetError as e:
                self.socket.shutdown(socket.SHUT_RDWR)
                self.socket.close()
                self.socket = None

                if self.disconnect_delegate:
                    self.disconnect_delegate(unexpected=True, exception=e)

                return # todo: this needs to send a signal back to the main thread so the thread can be joined

            except OSError as e: # likely a socket error, so self.socket is not useable any longer
                self.socket = None

                if self.disconnect_delegate:
                    self.disconnect_delegate(unexpected=True, exception=e)

                return

    def route_message(self, message):
        ''' Routes the received message to its delegate '''
        delegate = self.delegates.get(message['command'], None)
        if delegate:
            delegate(message)
            return

        if "command accepted" in message:
            message_id = uuid.UUID(message['id'])
            if message['command accepted'] == True:
                if self.command_accepted_delegate:
                    self.command_accepted_delegate(message_id)
            else:
                if self.command_declined_delegate:
                    self.command_declined_delegate(message_id, message["error"])

        elif "vcs init complete" in message:
            if message['vcs init complete'] == True:
                if self.vcs_init_completed_delegate:
                    self.vcs_init_completed_delegate()
            else:
                if self.vcs_init_failed_delegate:
                    self.vcs_init_failed_delegate(message['error'])

        elif "vcs report revision complete" in message:
            if message['vcs report revision complete'] == True:
                if self.vcs_report_revision_completed_delegate:
                    self.vcs_report_revision_completed_delegate(message['revision'])
            else:
                if self.vcs_report_revision_failed_delegate:
                    self.vcs_report_revision_failed_delegate(message['error'])

        elif "vcs sync complete" in message:
            if message['vcs sync complete'] == True:
                if self.vcs_sync_completed_delegate:
                    self.vcs_sync_completed_delegate(message['revision'])
            else:
                if self.vcs_sync_failed_delegate:
                    self.vcs_sync_failed_delegate(message['error'])

        elif "send file complete" in message:
            if message['send file complete'] == True:
                if self.send_file_completed_delegate:
                    self.send_file_completed_delegate(message['destination'])
            else:
                if self.send_file_failed_delegate:
                    self.send_file_failed_delegate(message['destination'], message['error'])
                    
        elif "receive file complete" in message:
            if message['receive file complete'] == True:
                if self.receive_file_completed_delegate:
                    self.receive_file_completed_delegate(message['source'], message['content'])
            else:
                if self.receive_file_failed_delegate:
                    self.receive_file_failed_delegate(message['source'], message['error'])
        else:
            LOGGER.error(f'Unhandled message: {message}')
            raise ValueError

    def process_received_data(self, buffer, received_data):
        for symbol in received_data:
            buffer.append(symbol)

            if symbol == '\x00': # found message end
                buffer.pop() # remove terminator
                message = message_protocol.decode_message(buffer)
                buffer.clear()

                # route message to its assigned delegate
                try:
                    self.route_message(message)
                except:
                    LOGGER.error(f"Error while parsing message: \n\n=== Traceback BEGIN ===\n{traceback.format_exc()}=== Traceback END ===\n")

    def send_message(self, message_bytes):
        if self.is_connected:
            LOGGER.message(f'Message: Sending ({self.address}): {message_bytes}')
            self.message_queue.appendleft(message_bytes)
        else:
            LOGGER.error(f'Message: Failed to send ({self.address}): {message_bytes}. No socket connected')
            if self.disconnect_delegate:
                self.disconnect_delegate(unexpected=True, exception=None)
