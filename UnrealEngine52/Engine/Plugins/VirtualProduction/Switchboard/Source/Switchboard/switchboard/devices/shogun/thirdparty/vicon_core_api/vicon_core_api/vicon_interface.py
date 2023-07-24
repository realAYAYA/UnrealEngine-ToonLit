"""Defines an interface for Vicon Live API client services."""
from .result import Result
from .schema import SchemaServices


class ViconInterface(object):

    def __init__(self, client):
        self.client = client
        self.unsupported = self.client.check_schemas(SchemaServices.interface_schemas(self.__class__.__name__))

    def call(self, function_name, *args):
        """Call the named function on the Live API terminal with the given arguments."""
        if function_name in self.unsupported:
            raise RuntimeError(str(Result.RPCUnknown))
        return self.client.send_command(function_name, *args)
