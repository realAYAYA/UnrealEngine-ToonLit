##################################################################################
## MIT License
##
## Copyright (c) 2019 Vicon Motion Systems Ltd
##
## Permission is hereby granted, free of charge, to any person obtaining a copy
## of this software and associated documentation files (the "Software"), to deal
## in the Software without restriction, including without limitation the rights
## to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
## copies of the Software, and to permit persons to whom the Software is
## furnished to do so, subject to the following conditions:
##
## The above copyright notice and this permission notice shall be included in all
## copies or substantial portions of the Software.
##
## THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
## IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
## FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
## AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
## LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
## OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
## SOFTWARE.
##################################################################################

from ....vicon_core_api.vicon_core_api import SchemaServices
from ....vicon_core_api.vicon_core_api import ViconInterface
from ....vicon_core_api.vicon_core_api import Result
from enum import Enum


class ApplicationServices(ViconInterface):
    """Functions for controlling the application and getting license information."""

    class ELicenseType(Enum):
        """ELicenseType.

        Enum Values:
            EStandalone: Locked to this machine.
            ENetwork: Network provided license.
            ECommuter: Commuter (checked out) network license.
            ETrial: Trial License.
            EGrace: Grace License.
            EUnknownType: License type could not be identified.
        """
        EStandalone = 0
        ENetwork = 1
        ECommuter = 2
        ETrial = 3
        EGrace = 4
        EUnknownType = 5


    def __init__(self, client):
        """Initialises ApplicationServices with a Client and checks if interface is supported."""
        super(ApplicationServices, self).__init__(client)

    def shutdown(self):
        """Shutdown the application, closing all device connections.

        Return:
            return < Result >: Ok - On success.
                NotPermitted - If application is in a mode that prevents shutdown.
            blocking_mode < string >: Application mode preventing shutdown (if any).
        """
        return self.client.send_command("ApplicationServices.Shutdown")

    def load_system_configuration(self, file_path):
        """Load a system configuration from file.

        The file path must be accessible from the remote host

        Args:
            file_path < string >: Absolute path to system file.

        Return:
            return < Result >: Ok - On success.
                NotFound - If system file does not exist.
                FileIOFailure - If system file could not be loaded.
        """
        return self.client.send_command("ApplicationServices.LoadSystemConfiguration", file_path)

    def save_system_configuration(self, file_path):
        """Save system configuration to a file.

        The file path must be accessible from the remote host

        Args:
            file_path < string >: Absolute path to desired location of system file.

        Return:
            return < Result >: Ok - On success.
                FileIOFailure - If system file could not be saved.
        """
        return self.client.send_command("ApplicationServices.SaveSystemConfiguration", file_path)

    def license_details(self):
        """Provides the type of license currently active and the days remaining if appropriate.

        Return:
            return < Result >: Ok - On success.
            type < ApplicationServices.ELicenseType >: The type of license in use.
            time_limited < bool >: Does the license have a time limit?
            days < int >: The number of days remaining.
        """
        return self.client.send_command("ApplicationServices.LicenseDetails")

    def license_info(self):
        """Provides a user facing description of the licensing state including information about the license type and network details or duration as appropriate.

        Return:
            return < Result >: Ok - On success.
            description < string >: A string that describes the licensing state.
        """
        return self.client.send_command("ApplicationServices.LicenseInfo")



SchemaServices.register_json_schema(ApplicationServices,"""{"Type": "NamedTuple", "TypeName": "ApplicationServices"}""")

SchemaServices.register_json_schema(ApplicationServices.ELicenseType,"""{"Type": "Enum32", "TypeName": "ApplicationServices.ELicenseType", "EnumValues": [["Standalone", 0], ["Network", 1], ["Commuter",
                                                                        2], ["Trial", 3], ["Grace", 4], ["UnknownType", 5]]}""")

SchemaServices.register_json_schema(ApplicationServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "ApplicationServices.Shutdown", "SubSchemas": [["Return", {"Type":
                                                           "UInt32", "Role": "Result"}], ["BlockingMode", {"Type": "String", "Role": "Output"}]]}""")

SchemaServices.register_json_schema(ApplicationServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "ApplicationServices.LoadSystemConfiguration", "SubSchemas": [["Return",
                                                           {"Type": "UInt32", "Role": "Result"}], ["FilePath", {"Type": "String", "Role": "Input"}]]}""")

SchemaServices.register_json_schema(ApplicationServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "ApplicationServices.SaveSystemConfiguration", "SubSchemas": [["Return",
                                                           {"Type": "UInt32", "Role": "Result"}], ["FilePath", {"Type": "String", "Role": "Input"}]]}""")

SchemaServices.register_json_schema(ApplicationServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "ApplicationServices.LicenseDetails", "SubSchemas": [["Return", {"Type":
                                                           "UInt32", "Role": "Result"}], ["Type", {"Type": "Ref", "Role": "Output", "TypeName": "ApplicationServices.ELicenseType"}],
                                                           ["TimeLimited", {"Type": "Bool", "Role": "Output"}], ["Days", {"Type": "Int32", "Role": "Output"}]]}""")

SchemaServices.register_json_schema(ApplicationServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "ApplicationServices.LicenseInfo", "SubSchemas": [["Return", {"Type":
                                                           "UInt32", "Role": "Result"}], ["Description", {"Type": "String", "Role": "Output"}]]}""")

