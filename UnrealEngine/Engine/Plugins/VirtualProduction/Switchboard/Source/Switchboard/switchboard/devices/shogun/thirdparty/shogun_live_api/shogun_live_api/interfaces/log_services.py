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
from enum import Enum
from ....vicon_core_api.vicon_core_api import Result


class LogServices(ViconInterface):
    """Functions for controlling application logging."""

    class ELogLevel(Enum):
        """The log level determines the maximum verbosity of logging.

        Be aware that increasing the verbosity of logging can adversely affect performance


        Enum Values:
            EOff: Turn off message logging.
            EError: Only log errors.
            EWarn: Also log warnings.
            EInfo: Also log user information.
            EDefault: Also log general status messages.
            EDebug: Also log debugging or troubleshooting messages.
        """
        EOff = 0
        EError = 1
        EWarn = 2
        EInfo = 3
        EDefault = 4
        EDebug = 5


    def __init__(self, client):
        """Initialises LogServices with a Client and checks if interface is supported."""
        super(LogServices, self).__init__(client)

    def global_log_level(self):
        """Get the global log level.

        All categories will log at this level unless specifically overriden

        Return:
            return < Result >: Ok - On success.
            log_level < LogServices.ELogLevel >: Global log level.
        """
        return self.client.send_command("LogServices.GlobalLogLevel")

    def set_global_log_level(self, log_level):
        """Set the global log level.

        All categories will log at this level unless specifically overriden

        Args:
            log_level < LogServices.ELogLevel >: Global log level.

        Return:
            return < Result >: Ok - On success.
        """
        return self.client.send_command("LogServices.SetGlobalLogLevel", log_level)

    def log_categories(self):
        """Get the current log categories.

        The categories may be updated as additional messages are logged

        Return:
            return < Result >: Ok - On success.
            categories < [string] >: Log category names.
        """
        return self.client.send_command("LogServices.LogCategories")

    def category_log_level(self, category):
        """Get the log level for this category.

        Args:
            category < string >: Category name.

        Return:
            return < Result >: Ok - On success.
                NotFound - There is no category with the specified name.
            log_level < LogServices.ELogLevel >: Category log level.
            use_global_level < bool >: If this category is using the global log level.
        """
        return self.client.send_command("LogServices.CategoryLogLevel", category)

    def set_category_log_level(self, category, log_level):
        """Set the log level for this category, overriding the global log level.

        Args:
            category < string >: Category name.
            log_level < LogServices.ELogLevel >: Category log level.

        Return:
            return < Result >: Ok - On success.
        """
        return self.client.send_command("LogServices.SetCategoryLogLevel", category, log_level)

    def clear_category_log_level(self, category):
        """Clear the log level for this category, so that the global log level is used instead.

        Args:
            category < string >: Category name.

        Return:
            return < Result >: Ok - On success.
        """
        return self.client.send_command("LogServices.ClearCategoryLogLevel", category)

    def log(self, category, log_level, message):
        """Send a message to the application log.

        Args:
            category < string >: Category name.
            log_level < LogServices.ELogLevel >: Level to log at.
            message < string >: Text to log.

        Return:
            return < Result >: Ok - On success.
        """
        return self.client.send_command("LogServices.Log", category, log_level, message)



SchemaServices.register_json_schema(LogServices,"""{"Type": "NamedTuple", "TypeName": "LogServices"}""")

SchemaServices.register_json_schema(LogServices.ELogLevel,"""{"Type": "Enum32", "TypeName": "LogServices.ELogLevel", "EnumValues": [["Off", 0], ["Error", 1], ["Warn", 2], ["Info", 3],
                                                             ["Default", 4], ["Debug", 5]]}""")

SchemaServices.register_json_schema(LogServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "LogServices.GlobalLogLevel", "SubSchemas": [["Return", {"Type": "UInt32",
                                                   "Role": "Result"}], ["LogLevel", {"Type": "Ref", "Role": "Output", "TypeName": "LogServices.ELogLevel"}]]}""")

SchemaServices.register_json_schema(LogServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "LogServices.SetGlobalLogLevel", "SubSchemas": [["Return", {"Type":
                                                   "UInt32", "Role": "Result"}], ["LogLevel", {"Type": "Ref", "Role": "Input", "TypeName": "LogServices.ELogLevel"}]]}""")

SchemaServices.register_json_schema(LogServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "LogServices.LogCategories", "SubSchemas": [["Return", {"Type": "UInt32",
                                                   "Role": "Result"}], ["Categories", {"Type": "List", "Role": "Output", "SubSchemas": [["", {"Type": "String"}]]}]]}""")

SchemaServices.register_json_schema(LogServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "LogServices.CategoryLogLevel", "SubSchemas": [["Return", {"Type":
                                                   "UInt32", "Role": "Result"}], ["Category", {"Type": "String", "Role": "Input"}], ["LogLevel", {"Type": "Ref", "Role": "Output",
                                                   "TypeName": "LogServices.ELogLevel"}], ["UseGlobalLevel", {"Type": "Bool", "Role": "Output"}]]}""")

SchemaServices.register_json_schema(LogServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "LogServices.SetCategoryLogLevel", "SubSchemas": [["Return", {"Type":
                                                   "UInt32", "Role": "Result"}], ["Category", {"Type": "String", "Role": "Input"}], ["LogLevel", {"Type": "Ref", "Role": "Input",
                                                   "TypeName": "LogServices.ELogLevel"}]]}""")

SchemaServices.register_json_schema(LogServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "LogServices.ClearCategoryLogLevel", "SubSchemas": [["Return", {"Type":
                                                   "UInt32", "Role": "Result"}], ["Category", {"Type": "String", "Role": "Input"}]]}""")

SchemaServices.register_json_schema(LogServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "LogServices.Log", "SubSchemas": [["Return", {"Type": "UInt32", "Role":
                                                   "Result"}], ["Category", {"Type": "String", "Role": "Input"}], ["LogLevel", {"Type": "Ref", "Role": "Input", "TypeName":
                                                   "LogServices.ELogLevel"}], ["Message", {"Type": "String", "Role": "Input"}]]}""")

