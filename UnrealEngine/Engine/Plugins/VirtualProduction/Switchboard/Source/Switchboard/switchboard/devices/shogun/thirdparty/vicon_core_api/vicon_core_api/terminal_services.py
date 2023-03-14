
from .vicon_interface import ViconInterface

class TerminalServices(ViconInterface):
    """Functions for getting information about the Vicon Core API terminal."""
    def __init__(self, client):
        """Initialises TerminalServices with a Client and checks if interface is supported."""
        super(TerminalServices, self).__init__(client)

    def application_information(self):
        """Get information about the application running the Vicon Core API server.

        Return:
            return < Result >: Ok - On success.
            name < string >: Application name.
            version < string >: Application version.
            changeset < string >: Application changeset.
        """
        result, args, _ = self.client.send_raw_command("Terminal.AppInfo")
        return result, args[0], args[1], args[2]
