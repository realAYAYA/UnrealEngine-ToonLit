"""Simple default client implementation for the Live API."""
from datetime import datetime, timedelta
from json import JSONDecoder
from threading import Thread, Condition, Lock
import socket
import traceback
from .result import Result
from .schema import Schema, SchemaServices


class RPCError(Exception):
    """ Exception raised for all remote procedure call failures. """
    pass


class Client(object):
    """ Client for connecting to a Vicon application (for example Shogun Live) using the TerminalServer protocol.

    Currently this is a simple implementation that just supports callbacks and blocking functions.
    Connection to the server is performed synchronously when the client is created. Note that callbacks
    are called from within the client's receive thread and therefore it is not permissible to call any
    functions of the client from within callbacks.

    If the client was not successful in connecting to the host application, any attempt to send
    commands will raise RPCError: NotConnected.
    """

    check_schemas_schema = Schema.make_function("Terminal.CheckSchemas", Schema.make_list(Schema(Schema.Type.EString)))
    check_schemas_schema.add_input("Schemas", Schema.make_list(Schema.make_ref("Schema")))
    SchemaServices.register_schema(None, check_schemas_schema)

    def __init__(self, host="localhost", port=52800, connect_timeout_seconds=10, send_timeout_seconds=60, client_failed_callback=None):
        """ Initialise with the hostname (or IP address) and port at which the server may be reached.

        Args:
            host <string>: Name or ip address for the server
            port <int>: Server port to connect to.
            client_failed_callback < python function taking a string > : optionally supply a function
                to be called when the client stops or fails
        """
        self.connect_timeout_seconds = connect_timeout_seconds
        self.send_timeout_seconds = send_timeout_seconds
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.socket.setblocking(send_timeout_seconds)
        self.server_endpoint = (host, port)
        self.version = None
        self.client_failed_callback = client_failed_callback
        self.message_id_generator = 0
        self.callback_id_generator = 0
        self.pending_messages = {}
        self.callback_map = {}
        self.thread = None
        self.condition = Condition(Lock())
        self.connected = False
        self.decoder = JSONDecoder()
        self._message_generator = None
        self._connect()

    def stop(self):
        """ Stop the client and close its connection. """
        with self.condition:
            try:
                self.socket.shutdown(socket.SHUT_RDWR)
                self.socket.close()
            except socket.error:
                pass
            self.connected = False
            thread = self.thread
        if thread:
            thread.join()

    def server_version(self):
        """ Get the protocol version of the server.

        Returns:
            A tuple (MajorVersionNumber, MinorVersionNumber) or None if unknown. """
        return self.version

    def check_schemas(self, schema_list):
        """ Get the server to confirm if it supports the provided schemas.

        returns:
            A list of schema type_names for any of the schemas that are NOT supported"""
        return self.send_command("Terminal.CheckSchemas", schema_list)

    def send_json_command(self, name, json_arg_string="[]"):
        """ Send a command, providing the arguments as a single JSON string.

        Args:
            name <string>: The remote command name.
            json_arg_string <string>: The command arguments as a JSON string (should be a valid JSON list)

        Returns:
            A result code and a JSON string.  If the result code does not represent an RPC failure then
            the JSON string will contain a valid JSON list containing any additional outputs of the command.

        Raises:
            RPCError if it was not possible to obtain a result from the host application.
        """
        with self.condition:
            return self._locked_send_command(name, json_arg_string)

    def send_raw_command(self, name, *args):
        """ Send a command providing a structured list of command inputs but no Schema.

        Args:
            *args <various types>: The command's inputs as python objects

        Returns:
            A tuple containing a Result object and possibly other native python objects resulting
            from a vanilla JSON parse of the function's outputs.

        Raises:
            RPCError if it was not possible to obtain a result from the host application.
        """
        try:
            result, reply = self.send_json_command(name, SchemaServices.write(args))
            if result.is_rpc_error():
                raise RPCError(str(result))
            decoder = JSONDecoder()
            reply_tuple = (result,) + decoder.raw_decode(reply)
            return reply_tuple if len(reply_tuple) != 1 else reply_tuple[0]
        except ValueError as e:
            raise RPCError(str(e))

    def send_command(self, name, *args):
        """ Send a command by providing the command's name and a structured list of command inputs.

        This function requires that the command's schema has been registered with SchemaServices.
        If the function fails due to RPC mechanics the function will raise an RPCError.

        Args:
            schema < Schema >: The command's schema (see schema.Schema).
            *args <various types>: The command's inputs as python objects

        Returns:
            A tuple of command outputs. The first element of this tuple is the command's return value if any.

        Raises:
            RPCError if it was not possible to obtain a result from the host application.
        """
        with self.condition:
            if not self.connected:
                raise RPCError(str(Result.RPCNotConnected))
            schema = SchemaServices.schema(name)
            if schema is None:
                raise RPCError(name + "Schema is not registered with SchemaServices")
            try:
                result, reply = self._locked_send_command(name, SchemaServices.write(args))
                if result.is_rpc_error():
                    raise RPCError(str(result))
                if schema.sub_schemas and schema.sub_schemas[0][1].role == Schema.Role.EResult:
                    reply_tuple = (result,) + SchemaServices.read(reply, schema)
                else:
                    reply_tuple = SchemaServices.read(reply, schema)
                return reply_tuple if len(reply_tuple) != 1 else reply_tuple[0]
            except ValueError as e:
                raise RPCError(str(e))

    def add_schema_callback(self, callback_name, function, schema):
        """ Add a callback, specifying the callback's schema if any.

        Warning: Callbacks are called directly from within the client's receive thread. Therefore it is
        not safe to call client commands from within callback functions.

        Args:
            callback_name < string >: The name of the callback.
            function < python function >: The function to call when the callback is invoked. The function should
                take arguments that correspond to the callback's schema.
            schema < Schema >: The callback's schema or 'None'. If 'None' then the 'function' should
                take only a single argument that is the JSON string of the callback arguments. Note that
                add_schema_callback() does not attempt validate the callback's schema with the server. Use
                check_schemas() for this. Incorrect schemas will result in the callback function not being invoked.

        Returns:
            A result code and a callback id that can be used to remove the callback.
        """
        result = Result.Ok
        callback_id = 0
        with self.condition:
            self.callback_id_generator += 1
            if callback_name not in self.callback_map:
                result, _reply = self._locked_send_command("Terminal.EnableCallback", "[\"" + callback_name + "\",true]")
                if result:
                    callback_id = self.callback_id_generator
                    self.callback_map[callback_name] = [(callback_id, schema, function)]
            else:
                callback_id = self.callback_id_generator
                self.callback_map[callback_name].append((callback_id, schema, function))
            return result, (callback_name, callback_id)

    def add_callback(self, callback_name, function):
        """ Add a callback.

        The callback's schema must have deen registered with SchemaServices.
        Warning: Callbacks are called directly from within the client's receive thread. Therefore it is
        not safe to call client commands from within callback functions.

        Args:
            callback_name < string >: The name of the callback (whose schema has been registered with SchemaServices. 
                Note that add_schema_callback() does not attempt validate the callback's schema with the server. Use
                check_schemas() for this. Incorrect schemas will result in the callback function not being invoked.
            function < python function >: The function to call when the callback is invoked. The function should
                take arguments that correspond to the callback's schema.

        Returns:
            A result code, and a callback id that can be used to remove the callback or None if the callback could not be added.
        """
        schema = SchemaServices.schema(callback_name)
        if schema is None:
            return Result.NotFound, None
        return self.add_schema_callback(callback_name, function, schema)

    def remove_callback(self, callback_id):
        """ Remove a callback.

        Args:
            callback_id < int >: A non-zero callback Id returned from a successfull call to add_callback().

        Returns:
            A result code that indicated success if the callback was removed
        """
        with self.condition:
            callback_entries = self.callback_map.get(callback_id[0], None)
            if callback_entries is None:
                return Result.NotFound
            callback_entry = next((x for x in callback_entries if x[0] == callback_id[1]), None)
            if not callback_entry:
                return Result.NotFound
            callback_entries.remove(callback_entry)
            if not callback_entries:
                self._locked_send_command("Terminal.EnableCallback", "[\"" + callback_id[0] + "\",false]")
                self.callback_map.pop(callback_id[0])
            return Result.Ok

    def __enter__(self):
        return self

    def __exit__(self, type, value, traceback):
        self.stop()

    def _connect(self):
        """ Called by__init__ to connect to server """
        deadline_time = datetime.now() + timedelta(seconds=self.connect_timeout_seconds)
        if self.socket.connect_ex(self.server_endpoint) == 0:
            with self.condition:
                self.connected = True
                self._message_generator = self._read_message()
                self.thread = Thread()
                self.thread.run = self._read_loop
                self.thread.setDaemon(True)
                self.thread.start()
                while self.connected and (self.version is None) and datetime.now() < deadline_time:
                    self.condition.wait((deadline_time - datetime.now()).total_seconds())
                if not self.version:
                    self.connected = False
                    if self.client_failed_callback:
                        self.client_failed_callback("Failed to receive server version")
                        self.client_failed_callback = None
        elif self.client_failed_callback:
            self.client_failed_callback("Failed to connect to server")
            self.client_failed_callback = None

    def _locked_send_command(self, name, payload):
        """ Sends a command using a command name and a json string argument payload. """
        if not self.connected:
            return Result.RPCNotConnected, ""
        deadline_time = datetime.now() + timedelta(seconds=self.send_timeout_seconds)
        self.message_id_generator += 1
        message_id = self.message_id_generator
        message = "[\"" + name + "\"," + str(message_id) + "]" + payload + "\00"
        self.pending_messages[message_id] = (None, "")
        self.socket.send(message.encode())
        while datetime.now() < deadline_time:
            pending_message = self.pending_messages.get(message_id, None)
            if pending_message is None:
                return Result.RPCFailed, ""
            if pending_message[0] is not None:
                self.pending_messages.pop(message_id)
                return pending_message
            self.condition.wait((deadline_time - datetime.now()).total_seconds())
        return Result.RPCFailed, "Send command timed out waiting for a response"

    def _read_message(self):
        """ Receives a message from the server or raises an RPCError.

        Returns:
            A header list and a json string payload.
        """
        decoder = JSONDecoder()
        text_buffer = ""
        while True:
            try:
                read_chunk = self.socket.recv(8192)
            except socket.error:
                read_chunk = None
            if not read_chunk:
                raise RPCError("Connection Lost")
            cmd_start = 0
            cmd_end = read_chunk.find(b"\00")
            while cmd_end != -1:
                output = text_buffer + read_chunk[cmd_start:cmd_end].decode('utf8')
                text_buffer = ""
                try:
                    header, payload_index = decoder.raw_decode(output, 0)
                    if len(header) < 1 or len(header) > 2:
                        raise RPCError("Received invalid message header from server")
                    yield header, output[payload_index:]
                except ValueError:
                    raise RPCError("Received invalid message from server")
                cmd_start = cmd_end + 1
                cmd_end = read_chunk.find(b"\00", cmd_start)
            text_buffer += read_chunk[cmd_start:].decode('utf8')

    def _read_server_version(self):
        """ Receives and validates the server protocol version. """
        header, json_message = next(self._message_generator)
        with self.condition:
            if header is None or header[0] != "ViconTerminal":
                raise RPCError("Server endpoint was not a ViconTerminal")
            try:
                decoded, _index = self.decoder.raw_decode(json_message, 0)
                if len(decoded) == 2 and isinstance(decoded[0], int) and isinstance(decoded[1], int):
                    self.version = (decoded[0], decoded[1])
                    self.condition.notify_all()
                    return
            except ValueError:
                pass
            raise RPCError("Could not interpret ViconTerminal version from server")

    def _read_loop(self):
        """ Receives replies and callbacks from the Server until a failure is encountered. """
        try:
            self._read_server_version()
            while True:
                header, json_message = next(self._message_generator)
                if len(header) == 2:  # is command reply
                    command_id = header[0]
                    result = Result(header[1])
                    with self.condition:
                        if command_id in self.pending_messages:
                            if result.is_rpc_error():
                                self.pending_messages[command_id] = (result, None)
                            else:
                                self.pending_messages[command_id] = (result, json_message)
                            self.condition.notify_all()
                elif len(header) == 1:  # is callback
                    with self.condition:
                        callback_name = header[0]
                        callback_entries = self.callback_map.get(callback_name, [])
                    for callback_entry in callback_entries:
                        # decode json callback if we have a schema
                        if callback_entry[1] is not None:
                            try:
                                arg_tuple = SchemaServices.read(json_message, callback_entry[1])
                                callback_entry[2](*arg_tuple)
                            except Exception as e:
                                # if the user callback raises, we want to provide the stack trace so they can debug
                                raise RPCError('User callback raised exception: ' + traceback.format_exc())
                        # otherwise call the callback with the json text
                        else:
                            callback_entry[2](json_message)

        except RPCError as e:
            # when connection fails we complete all pending messages
            with self.condition:
                self.pending_messages.clear()
                self.connected = False
                self.condition.notify_all()
                if self.client_failed_callback:
                    self.client_failed_callback(str(e))
