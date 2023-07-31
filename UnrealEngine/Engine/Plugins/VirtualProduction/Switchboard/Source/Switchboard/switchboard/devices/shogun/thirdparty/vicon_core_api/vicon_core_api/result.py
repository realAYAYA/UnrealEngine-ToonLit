"""This module defines result codes and descriptions for API operations."""
from collections import OrderedDict


class Result(object):
    """Generic operation result code and description."""
    ERROR_FLAG = 0x80000000
    CATEGORY_MASK = 0x7FFF0000
    RPC_CATEGORY = 0x00010000
    code_map = OrderedDict()

    def __init__(self, code=ERROR_FLAG):
        self.code = code

    def __bool__(self):
        return (self.code & self.ERROR_FLAG) == 0
    __nonzero__ = __bool__

    def __eq__(self, other):
        return self.code == other.code

    def __ne__(self, other):
        return not self == other

    def __repr__(self):
        try:
            description = self.code_map[self.code]
            return "Result." + description.split(':')[0]
        except KeyError:
            return object.__repr__(self)

    def __str__(self):
        return self.code_map.get(self.code, "Invalid error code!")

    def is_rpc_error(self):
        """Returns true if the result indicates a Remote Procedure Call error."""
        return (self.code & self.CATEGORY_MASK) == self.RPC_CATEGORY

    @classmethod
    def add_code(cls, message, code=None):
        """Registers a code value and description."""
        if code is None:
            code = next(reversed(cls.code_map)) + 1
        cls.code_map[code] = message
        return Result(code)

# Generic successful result codes.
Result.Ok = Result.add_code("Ok: The function succeeded", 0)
Result.OkButNoEffect = Result.add_code("OkButNoEffect: The function succeeded but had no effect")
Result.OkButPartial = Result.add_code("OkButPartial: The function succeeded but was only partially applicable")
Result.OkButFinished = Result.add_code("OkButFinished: Successfully finished - no further operation possible")
Result.OkButModified = Result.add_code("OkButModified: The function succeeded but one or more values or parameters were modified")

# Generic failure result codes.
Result.Failed = Result.add_code("Failed: The function failed", Result.ERROR_FLAG)
Result.FailedWithException = Result.add_code("Failed: The function generated an exception")
Result.Finished = Result.add_code("Finished: The service requested has finished")
Result.Stopping = Result.add_code("Stopping: The service requested is stopping")
Result.NotImplemented = Result.add_code("NotImplemented: The function is not implemented in this version")
Result.NotSupported = Result.add_code("NotSupported: The function is not supported")
Result.NotPermitted = Result.add_code("NotPermitted: The function is not permitted at this time")
Result.NotAvailable = Result.add_code("NotAvailable: The information requested was not available")
Result.NotConnected = Result.add_code("NotConnected: The required connection is not present")
Result.NotSet = Result.add_code("NotSet: The requested information has not been set")
Result.NotFound = Result.add_code("NotFound: The requested information, instance or type was not found")
Result.NotRequired = Result.add_code("NotRequired: The action requested is not necessary")
Result.AlreadyPresent = Result.add_code("AlreadyPresent: The data or item being set was already present")
Result.AlreadyInProgress = Result.add_code("AlreadyInProgress: The operation requested is already in progress")
Result.AlreadySet = Result.add_code("AlreadySet: The data or item being set is already set")
Result.InvalidArgument = Result.add_code("InvalidArgument: A supplied argument was invalid")
Result.InvalidIndex = Result.add_code("InvalidIndex: A supplied index was invalid or out of range")
Result.InvalidName = Result.add_code("InvalidName: A supplied name was invalid or unrecognized")
Result.InvalidId = Result.add_code("InvalidId: A supplied id was invalid or unrecognized")
Result.InvalidHandle = Result.add_code("InvalidHandle: A supplied handle was invalid")
Result.InvalidToken = Result.add_code("InvalidToken: A supplied token was invalid")
Result.InvalidPointer = Result.add_code("InvalidPointer: A supplied pointer was invalid")
Result.InvalidFormat = Result.add_code("InvalidFormat: The data provided or requested was not in the expected format")
Result.InvalidType = Result.add_code("InvalidType: An incorrect type was provided")
Result.InvalidVersion = Result.add_code("InvalidVersion: An object or file was the wrong version")
Result.InvalidSession = Result.add_code("InvalidSession: The session specified was not valid")
Result.InvalidInfo = Result.add_code("InvalidInfo: The information supplied was not valid")
Result.InvalidValue = Result.add_code("InvalidValue: A value supplied was not valid")
Result.InvalidSettings = Result.add_code("InvalidSettings: The settings supplied were not valid")
Result.ProtocolFailure = Result.add_code("ProtocolFailure: A protocol error was detected")
Result.NetworkFailure = Result.add_code("NetworkFailure: A network error occurred")
Result.AllocationFailure = Result.add_code("AllocationFailure: Failed to allocate memory or item")
Result.SerialisationFailure = Result.add_code("SerialisationFailure: Failed during serialization/deserialization of data")
Result.FileIOFailure = Result.add_code("FileIOFailure: An operation to read or write a file failed")
Result.TimedOut = Result.add_code("TimedOut: The operation timed out")
Result.WouldBlock = Result.add_code("WouldBlock: The operation failed as it would have had to block")
Result.EndOfData = Result.add_code("EndOfData: The operation failed as the end of a data stream has been reached")
Result.Conflict = Result.add_code("Conflict: The operation detected a conflict")
Result.FileNotOpen = Result.add_code("FileNotOpen: The file is not open or could not be opened")
Result.ShuttingDown = Result.add_code("ShuttingDown: The operation failed because the component is shutting down")
Result.DiskFull = Result.add_code("DiskFull: No space left on device")
Result.Canceled = Result.add_code("Canceled: The operation has been canceled")
Result.FileNotFound = Result.add_code("FileNotFound: The file does not exist or is unreadable")

# RPC failure result codes.
Result.RPCUnknown = Result.add_code("RPCUnknown: The specified remote function or callback is not known", Result.ERROR_FLAG | Result.RPC_CATEGORY)
Result.RPCFailed = Result.add_code("RPCFailed: The remote function or callback invocation could not be invoked")
Result.RPCInvalid = Result.add_code("RPCInvalid: The remote function or callback invocation was badly formed or otherwise invalid.")
Result.RPCNotConnected = Result.add_code("RPCNotConnected: The connection to the remote function or callback is not open")
