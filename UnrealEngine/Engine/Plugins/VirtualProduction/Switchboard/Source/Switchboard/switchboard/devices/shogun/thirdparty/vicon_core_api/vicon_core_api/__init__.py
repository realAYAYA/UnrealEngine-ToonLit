""" Vicon core remote control API.

    The main components included are a client for communication with the terminal server, a schema which is used to define terminal functionality and a result class which provides information on operation success or failure.

    WIRE FORMAT DOCUMENTATION:
        The wire protocol takes the form of null terminated messages containing concatenated JSON.
        Descriptions of the main components of the format are given below:

    Versioning:
        Upon connection the server always sends a server identifier of the form:

        ["ViconTerminal"][1,0]\00

        In this message "ViconTerminal" is the protocol name and the second array contains a major and minor version number

    Sending commands:
        Commands can be sent to the server as follows:

        ["<CommandName>", <Id>][<Args...>]\00

        In this message <CommandName> is the name of a command supported by the server. <Id> should be an integer
        value that uniquely identifies this command invocation - it is used to correlate replies.
        <Args> can be any legal JSON values that correctly represent the input arguments expected by the function.
        To determine this, you will need to know the specific commands's 'Schema' (See schema.Schema)
        All of the input arguments must be provided and in the correct order. For example if the server supported a function
        "SetCameraName" taking a camera number and a name, the call might look like this:

        ["CameraCommands.SetCameraName",1234][5,"MyCameraFive"]

    Replies:
        All commands receive a reply from the server whether successful or not. These take one of two forms:

        [<Id>, <ResultCode>][<OutputArgs...>]\00    or
        [<Id>, <ResultCode>]\00

        The <Id> corresponds to that used when issuing the command. The <ResultCode> is a 32-bit unsigned integer indicating
        whether the command was successful (see result.Result). Some result codes indicate a failure in the RPC mechanism,
        such as connection loss or an unknown function name. These codes can be identified with the expression:
        
        (<ResultCode> & 0x7FFF0000) == 0x00010000

        In these cases, no outputs from the command will be available and so the output list is ommited from the message.
        Otherwise the output list will be present (and empty if the command has no outputs).
        Note that if a command returns a result code as its first output, then this is provided in the <ResultCode> of the
        message header rather than in the outputs list. Note that result codes indicate some kind of failure if their
        top bit is set. i.e.:

        is_failure = (<ResultCode> & 0x80000000) != 0

    Callbacks:
        The server can also issue callbacks. These are spontaneous messages of the form:

        ["<CallbackName">][<Args...>]\00

        You will only receive a particular type of callback from the server if you have enabled it. This is done
        using one of the servers built-in commands described below.

    Built-in server commands:
        Some commands are supplied by the server itself rather than the underlying application:

        ["Terminal.EnableCallback", <Id>]["<CallbackName>", true|false]\00

        This enables or disables calbacks of type <CallbackName> from the server. The function succeeds if the callback
        type was successfully enabled or disabled.

        ["Terminal.CheckSchemas", <Id>][[<Schemas...>]]\00

        This allows you to send a list of schemas to the server that describe functions, callbacks or types.
        The server checks whether it supports these schemas. It returns a result code (that will indicates success
        only if all schemas are supported) and also a list of schema type names containing any schemas that are not supported.
        Note that if the input schema list contains 'Ref' schemas then all schema dependencies should also be present in the list.

    """

from .schema import Schema, SchemaServices
from .result import Result
from .vicon_interface import ViconInterface
from .client import Client
from .client import RPCError
from .terminal_services import TerminalServices
