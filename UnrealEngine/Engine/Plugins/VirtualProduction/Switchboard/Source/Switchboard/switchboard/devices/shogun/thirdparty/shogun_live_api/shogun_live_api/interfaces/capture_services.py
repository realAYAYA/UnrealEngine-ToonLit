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
from ..types import Timecode135MHz
from ..types import ETimecodeStandard


class CaptureServices(ViconInterface):
    """Functions and callbacks for controlling and monitoring capture."""

    class EState(Enum):
        """Possible states of a capture.

        Enum Values:
            ENone: Invalid / Capture did not start.
            EArmed: System armed and waiting to start.
            EStarted: A capture is started and in progress.
            EPartStopped: One or more sub-systems have stopped capturing but capture is ongoing.
            EStopped: All devices have stopped capturing, but data is still being written to disk.
            ECompleted: Capture completed - all files have been written to disk. This is the final state for this capture.
            ECanceling: Capture is in the process of being canceled.
            ECanceled: Capture canceled. This is the final state for this capture.
        """
        ENone = 0
        EArmed = 1
        EStarted = 2
        EPartStopped = 3
        EStopped = 4
        ECompleted = 5
        ECanceling = 6
        ECanceled = 7


    def __init__(self, client):
        """Initialises CaptureServices with a Client and checks if interface is supported."""
        super(CaptureServices, self).__init__(client)

    def set_capture_folder(self, folder):
        """Set the path for the default capture folder.

        Args:
            folder < string >: Path to a valid folder.

        Return:
            return < Result >: Ok - On success.
                InvalidArgument - If path does not correspond to a valid folder, or folder does not exist.
        """
        return self.client.send_command("CaptureServices.SetCaptureFolder", folder)

    def capture_folder(self):
        """Get the default capture folder path.

        Return:
            return < Result >: Ok - On success.
            folder < string >: The default folder path.
        """
        return self.client.send_command("CaptureServices.CaptureFolder")

    def set_capture_name(self, name):
        """Set the name for the next capture.

        Args:
            name < string >: Name suitable for use in file names.

        Return:
            return < Result >: Ok - On success.
                InvalidArgument - If the name includes invalid characters.
        """
        return self.client.send_command("CaptureServices.SetCaptureName", name)

    def capture_name(self):
        """Get the name for the next capture.

        Return:
            return < Result >: Ok - On success.
            name < string >: Name for the next capture.
        """
        return self.client.send_command("CaptureServices.CaptureName")

    def set_capture_description(self, description):
        """Set the description for the next capture.

        Args:
            description < string >: Description for the next capture.

        Return:
            return < Result >: Ok - On success.
        """
        return self.client.send_command("CaptureServices.SetCaptureDescription", description)

    def capture_description(self):
        """Get the description for the next capture.

        Return:
            return < Result >: Ok - On success.
            description < string >: Description for the next capture.
        """
        return self.client.send_command("CaptureServices.CaptureDescription")

    def set_capture_notes(self, notes):
        """Set the notes for the next capture.

        Args:
            notes < string >: Notes for the next capture.

        Return:
            return < Result >: Ok - On success.
        """
        return self.client.send_command("CaptureServices.SetCaptureNotes", notes)

    def capture_notes(self):
        """Get the notes for the next capture.

        Return:
            return < Result >: Ok - On success.
            notes < string >: Notes for the next capture.
        """
        return self.client.send_command("CaptureServices.CaptureNotes")

    def add_take_info_changed_callback(self, function):
        """Callback issued whenever any take info has changed.

        This includes the capture folder, capture name, capture description and capture notes
        """
        return self.client.add_callback("CaptureServices.TakeInfoChangedCallback", function)

    def set_capture_processed_data_enabled(self, enable):
        """Enable or disable processed data capture.

        Args:
            enable < bool >: Enable if true, otherwise disable

        Return:
            return < Result >: Ok - On success.
        """
        return self.client.send_command("CaptureServices.SetCaptureProcessedDataEnabled", enable)

    def capture_processed_data_enabled(self):
        """Determine if processed data capture is enabled.

        Return:
            return < Result >: Ok - On success.
            enable < bool >: True if enabled, otherwise false.
        """
        return self.client.send_command("CaptureServices.CaptureProcessedDataEnabled")

    def set_capture_video_enabled(self, enable):
        """Enable or disable video capture.

        Args:
            enable < bool >: Enable if true, otherwise disable

        Return:
            return < Result >: Ok - On success.
        """
        return self.client.send_command("CaptureServices.SetCaptureVideoEnabled", enable)

    def capture_video_enabled(self):
        """Determine if video capture is enabled.

        Return:
            return < Result >: Ok - On success.
            enable < bool >: True if enabled, otherwise false.
        """
        return self.client.send_command("CaptureServices.CaptureVideoEnabled")

    def add_capture_options_changed_callback(self, function):
        """Callback issued whenever capture options have changed.

        This includes whether to capture processed data and video
        """
        return self.client.add_callback("CaptureServices.CaptureOptionsChangedCallback", function)

    def set_start_on_timecode_enabled(self, enable):
        """Enable or disable 'Start on Timecode'.

        Args:
            enable < bool >: Enable if true, otherwise disable

        Return:
            return < Result >: Ok - On success.
        """
        return self.client.send_command("CaptureServices.SetStartOnTimecodeEnabled", enable)

    def start_on_timecode_enabled(self):
        """Determine if 'Start on Timecode' is enabled.

        Return:
            return < Result >: Ok - On success.
            enable < bool >: True if enabled, otherwise false.
        """
        return self.client.send_command("CaptureServices.StartOnTimecodeEnabled")

    def set_start_timecode(self, hours, minutes, seconds, frames):
        """Set the starting timecode if using 'Start on Timecode.

        Args:
            hours < int >: The timecode hour count
            minutes < int >: The timecode minute count
            seconds < int >: The timecode second count
            frames < int >: The timecode frame count

        Return:
            return < Result >: Ok - On success.
        """
        return self.client.send_command("CaptureServices.SetStartTimecode", hours, minutes, seconds, frames)

    def start_timecode(self):
        """The timecode that will be used by 'Start on Timecode'.

        Return:
            return < Result >: Ok - On success.
            hours < int >: The timecode hour count
            minutes < int >: The timecode minute count
            seconds < int >: The timecode second count
            frames < int >: The timecode frame count
        """
        return self.client.send_command("CaptureServices.StartTimecode")

    def set_stop_on_timecode_enabled(self, enable):
        """Enable or disable 'Stop on Timecode'.

        Args:
            enable < bool >: Enable if true, otherwise disable

        Return:
            return < Result >: Ok - On success.
        """
        return self.client.send_command("CaptureServices.SetStopOnTimecodeEnabled", enable)

    def stop_on_timecode_enabled(self):
        """Determine if 'Stop on Timecode' is enabled.

        Return:
            return < Result >: Ok - On success.
            enable < bool >: True if enabled, otherwise false.
        """
        return self.client.send_command("CaptureServices.StopOnTimecodeEnabled")

    def set_stop_timecode(self, hours, minutes, seconds, frames):
        """Set the stopping timecode if using 'Stop on Timecode.

        Args:
            hours < int >: The timecode hour count
            minutes < int >: The timecode minute count
            seconds < int >: The timecode second count
            frames < int >: The timecode frame count

        Return:
            return < Result >: Ok - On success.
        """
        return self.client.send_command("CaptureServices.SetStopTimecode", hours, minutes, seconds, frames)

    def stop_timecode(self):
        """The timecode that will be used by 'Stop on Timecode'.

        Return:
            return < Result >: Ok - On success.
            hours < int >: The timecode hour count
            minutes < int >: The timecode minute count
            seconds < int >: The timecode second count
            frames < int >: The timecode frame count
        """
        return self.client.send_command("CaptureServices.StopTimecode")

    def set_limit_capture_duration_enabled(self, enable):
        """Enable or disable capture duration limit.

        Args:
            enable < bool >: Enable if true, otherwise disable

        Return:
            return < Result >: Ok - On success.
        """
        return self.client.send_command("CaptureServices.SetLimitCaptureDurationEnabled", enable)

    def limit_capture_duration_enabled(self):
        """Determine if capture duration limit is enabled.

        Return:
            return < Result >: Ok - On success.
            enable < bool >: True if enabled, otherwise false.
        """
        return self.client.send_command("CaptureServices.LimitCaptureDurationEnabled")

    def set_duration_limit_in_seconds(self, seconds):
        """Set the maximum capture duration in seconds (if using capture duration limit).

        Args:
            seconds < float >: The maximum duration in seconds

        Return:
            return < Result >: Ok - On success.
        """
        return self.client.send_command("CaptureServices.SetDurationLimitInSeconds", seconds)

    def duration_limit_in_seconds(self):
        """Get the maximum capture duration in seconds (if using capture duration limit).

        Return:
            return < Result >: Ok - On success.
            seconds < float >: The maximum duration in seconds.
        """
        return self.client.send_command("CaptureServices.DurationLimitInSeconds")

    def add_auto_capture_options_changed_callback(self, function):
        """Callback issued whenever auto capture options have changed.

        This start and stop on timecode options and capture duration limit options
        """
        return self.client.add_callback("CaptureServices.AutoCaptureOptionsChangedCallback", function)

    def start_capture(self):
        """Start or Arm a capture using current settings.

        Return:
            return < Result >: Ok - On success.
                NotPermitted - If application is not ready to capture or a capture in already in progress.
                InvalidSettings - If any capture settings were invalid.
                Failed - On any other failure.
            id < int >: A non-zero identifier that uniquely identifies this capture.
        """
        return self.client.send_command("CaptureServices.StartCapture")

    def stop_capture(self, id):
        """Stop a capture that is in progress.

        Args:
            id < int >: The id of the capture to stop, or zero to stop any capture that is in progress.

        Return:
            return < Result >: Ok - On success.
                NotPermitted - If the specified capture is not in progress.
        """
        return self.client.send_command("CaptureServices.StopCapture", id)

    def cancel_capture(self, id):
        """Cancel a capture that is in progress.

        Args:
            id < int >: The id of the capture to cancel, or zero to cancel any capture that is in progress.

        Return:
            return < Result >: Ok - On success.
                NotPermitted - If the specified capture is not in progress.
        """
        return self.client.send_command("CaptureServices.CancelCapture", id)

    def latest_capture_state(self):
        """Get the current state of the most recently started capture.

        Return:
            return < Result >: Ok - On success.
                NotAvailable - If there have been no captures.
            id < int >: Id of the latest capture.
            state < CaptureServices.EState >: The capture's state.
        """
        return self.client.send_command("CaptureServices.LatestCaptureState")

    def latest_capture_name(self):
        """Get the name of the most recently started capture.

        Return:
            return < Result >: Ok - On success.
                NotAvailable - If there have been no captures.
            id < int >: Id of the latest capture.
            name < string >: Name of the capture.
        """
        return self.client.send_command("CaptureServices.LatestCaptureName")

    def latest_capture_timecode(self):
        """Get the start timecode of the most recently started capture.

        Return:
            return < Result >: Ok - On success.
                NotAvailable - If there have been no captures.
            id < int >: Id of the latest capture.
            timecode < Timecode135MHz >: The capture's start timecode.
        """
        return self.client.send_command("CaptureServices.LatestCaptureTimecode")

    def latest_capture_file_paths(self):
        """Get the full paths of the files written by the most recent capture.

        Note that this information will be unavailable until the capture is complete

        Return:
            return < Result >: Ok - On success.
                NotAvailable - If there have been no captures.
            id < int >: Id of the latest capture.
            file_paths < [string] >: List of paths to files generated.
        """
        return self.client.send_command("CaptureServices.LatestCaptureFilePaths")

    def latest_capture_errors(self):
        """Get any errors relating to the most recently started capture.

        Return:
            return < Result >: Ok - On success.
                NotAvailable - If there have been no captures.
            id < int >: Id of the latest capture.
            errors < [string] >: Details of any errors that occurred.
        """
        return self.client.send_command("CaptureServices.LatestCaptureErrors")

    def add_latest_capture_changed_callback(self, function):
        """Callback issued whenever any information pertaining to the latest capture changes.

        The callback supplies both the id and the state of the latest capture. You are guaranteed to receive a callback for every
        capture state transition and you may also receive additional callbacks when other information relating to the capture changes

        Args:
            id < int >: Id of the latest capture.
            state < CaptureServices.EState >: The state of the latest capture.
        """
        return self.client.add_callback("CaptureServices.LatestCaptureChangedCallback", function)

    def remove_callback(self, callback_id):
        """remove callback of any type using the id supplied when it was added."""
        return self.client.remove_callback(callback_id)



SchemaServices.register_json_schema(CaptureServices,"""{"Type": "NamedTuple", "TypeName": "CaptureServices"}""")

SchemaServices.register_json_schema(CaptureServices.EState,"""{"Type": "Enum32", "TypeName": "CaptureServices.EState", "EnumValues": [["None", 0], ["Armed", 1], ["Started", 2], ["PartStopped",
                                                              3], ["Stopped", 4], ["Completed", 5], ["Canceling", 6], ["Canceled", 7]]}""")

SchemaServices.register_json_schema(CaptureServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "CaptureServices.SetCaptureFolder", "SubSchemas": [["Return", {"Type":
                                                       "UInt32", "Role": "Result"}], ["Folder", {"Type": "String", "Role": "Input"}]]}""")

SchemaServices.register_json_schema(CaptureServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "CaptureServices.CaptureFolder", "SubSchemas": [["Return", {"Type":
                                                       "UInt32", "Role": "Result"}], ["Folder", {"Type": "String", "Role": "Output"}]]}""")

SchemaServices.register_json_schema(CaptureServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "CaptureServices.SetCaptureName", "SubSchemas": [["Return", {"Type":
                                                       "UInt32", "Role": "Result"}], ["Name", {"Type": "String", "Role": "Input"}]]}""")

SchemaServices.register_json_schema(CaptureServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "CaptureServices.CaptureName", "SubSchemas": [["Return", {"Type":
                                                       "UInt32", "Role": "Result"}], ["Name", {"Type": "String", "Role": "Output"}]]}""")

SchemaServices.register_json_schema(CaptureServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "CaptureServices.SetCaptureDescription", "SubSchemas": [["Return",
                                                       {"Type": "UInt32", "Role": "Result"}], ["Description", {"Type": "String", "Role": "Input"}]]}""")

SchemaServices.register_json_schema(CaptureServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "CaptureServices.CaptureDescription", "SubSchemas": [["Return", {"Type":
                                                       "UInt32", "Role": "Result"}], ["Description", {"Type": "String", "Role": "Output"}]]}""")

SchemaServices.register_json_schema(CaptureServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "CaptureServices.SetCaptureNotes", "SubSchemas": [["Return", {"Type":
                                                       "UInt32", "Role": "Result"}], ["Notes", {"Type": "String", "Role": "Input"}]]}""")

SchemaServices.register_json_schema(CaptureServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "CaptureServices.CaptureNotes", "SubSchemas": [["Return", {"Type":
                                                       "UInt32", "Role": "Result"}], ["Notes", {"Type": "String", "Role": "Output"}]]}""")

SchemaServices.register_json_schema(CaptureServices,"""{"Type": "NamedTuple", "Role": "Callback", "TypeName": "CaptureServices.TakeInfoChangedCallback"}""")

SchemaServices.register_json_schema(CaptureServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "CaptureServices.SetCaptureProcessedDataEnabled", "SubSchemas": [["Return",
                                                       {"Type": "UInt32", "Role": "Result"}], ["Enable", {"Type": "Bool", "Role": "Input"}]]}""")

SchemaServices.register_json_schema(CaptureServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "CaptureServices.CaptureProcessedDataEnabled", "SubSchemas": [["Return",
                                                       {"Type": "UInt32", "Role": "Result"}], ["Enable", {"Type": "Bool", "Role": "Output"}]]}""")

SchemaServices.register_json_schema(CaptureServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "CaptureServices.SetCaptureVideoEnabled", "SubSchemas": [["Return",
                                                       {"Type": "UInt32", "Role": "Result"}], ["Enable", {"Type": "Bool", "Role": "Input"}]]}""")

SchemaServices.register_json_schema(CaptureServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "CaptureServices.CaptureVideoEnabled", "SubSchemas": [["Return", {"Type":
                                                       "UInt32", "Role": "Result"}], ["Enable", {"Type": "Bool", "Role": "Output"}]]}""")

SchemaServices.register_json_schema(CaptureServices,"""{"Type": "NamedTuple", "Role": "Callback", "TypeName": "CaptureServices.CaptureOptionsChangedCallback"}""")

SchemaServices.register_json_schema(CaptureServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "CaptureServices.SetStartOnTimecodeEnabled", "SubSchemas": [["Return",
                                                       {"Type": "UInt32", "Role": "Result"}], ["Enable", {"Type": "Bool", "Role": "Input"}]]}""")

SchemaServices.register_json_schema(CaptureServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "CaptureServices.StartOnTimecodeEnabled", "SubSchemas": [["Return",
                                                       {"Type": "UInt32", "Role": "Result"}], ["Enable", {"Type": "Bool", "Role": "Output"}]]}""")

SchemaServices.register_json_schema(CaptureServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "CaptureServices.SetStartTimecode", "SubSchemas": [["Return", {"Type":
                                                       "UInt32", "Role": "Result"}], ["Hours", {"Type": "UInt8", "Role": "Input"}], ["Minutes", {"Type": "UInt8", "Role": "Input"}],
                                                       ["Seconds", {"Type": "UInt8", "Role": "Input"}], ["Frames", {"Type": "UInt8", "Role": "Input"}]]}""")

SchemaServices.register_json_schema(CaptureServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "CaptureServices.StartTimecode", "SubSchemas": [["Return", {"Type":
                                                       "UInt32", "Role": "Result"}], ["Hours", {"Type": "UInt8", "Role": "Output"}], ["Minutes", {"Type": "UInt8", "Role": "Output"}],
                                                       ["Seconds", {"Type": "UInt8", "Role": "Output"}], ["Frames", {"Type": "UInt8", "Role": "Output"}]]}""")

SchemaServices.register_json_schema(CaptureServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "CaptureServices.SetStopOnTimecodeEnabled", "SubSchemas": [["Return",
                                                       {"Type": "UInt32", "Role": "Result"}], ["Enable", {"Type": "Bool", "Role": "Input"}]]}""")

SchemaServices.register_json_schema(CaptureServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "CaptureServices.StopOnTimecodeEnabled", "SubSchemas": [["Return",
                                                       {"Type": "UInt32", "Role": "Result"}], ["Enable", {"Type": "Bool", "Role": "Output"}]]}""")

SchemaServices.register_json_schema(CaptureServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "CaptureServices.SetStopTimecode", "SubSchemas": [["Return", {"Type":
                                                       "UInt32", "Role": "Result"}], ["Hours", {"Type": "UInt8", "Role": "Input"}], ["Minutes", {"Type": "UInt8", "Role": "Input"}],
                                                       ["Seconds", {"Type": "UInt8", "Role": "Input"}], ["Frames", {"Type": "UInt8", "Role": "Input"}]]}""")

SchemaServices.register_json_schema(CaptureServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "CaptureServices.StopTimecode", "SubSchemas": [["Return", {"Type":
                                                       "UInt32", "Role": "Result"}], ["Hours", {"Type": "UInt8", "Role": "Output"}], ["Minutes", {"Type": "UInt8", "Role": "Output"}],
                                                       ["Seconds", {"Type": "UInt8", "Role": "Output"}], ["Frames", {"Type": "UInt8", "Role": "Output"}]]}""")

SchemaServices.register_json_schema(CaptureServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "CaptureServices.SetLimitCaptureDurationEnabled", "SubSchemas": [["Return",
                                                       {"Type": "UInt32", "Role": "Result"}], ["Enable", {"Type": "Bool", "Role": "Input"}]]}""")

SchemaServices.register_json_schema(CaptureServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "CaptureServices.LimitCaptureDurationEnabled", "SubSchemas": [["Return",
                                                       {"Type": "UInt32", "Role": "Result"}], ["Enable", {"Type": "Bool", "Role": "Output"}]]}""")

SchemaServices.register_json_schema(CaptureServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "CaptureServices.SetDurationLimitInSeconds", "SubSchemas": [["Return",
                                                       {"Type": "UInt32", "Role": "Result"}], ["Seconds", {"Type": "Float64", "Role": "Input"}]]}""")

SchemaServices.register_json_schema(CaptureServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "CaptureServices.DurationLimitInSeconds", "SubSchemas": [["Return",
                                                       {"Type": "UInt32", "Role": "Result"}], ["Seconds", {"Type": "Float64", "Role": "Output"}]]}""")

SchemaServices.register_json_schema(CaptureServices,"""{"Type": "NamedTuple", "Role": "Callback", "TypeName": "CaptureServices.AutoCaptureOptionsChangedCallback"}""")

SchemaServices.register_json_schema(CaptureServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "CaptureServices.StartCapture", "SubSchemas": [["Return", {"Type":
                                                       "UInt32", "Role": "Result"}], ["Id", {"Type": "UInt32", "Role": "Output"}]]}""")

SchemaServices.register_json_schema(CaptureServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "CaptureServices.StopCapture", "SubSchemas": [["Return", {"Type":
                                                       "UInt32", "Role": "Result"}], ["Id", {"Type": "UInt32", "Role": "Input"}]]}""")

SchemaServices.register_json_schema(CaptureServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "CaptureServices.CancelCapture", "SubSchemas": [["Return", {"Type":
                                                       "UInt32", "Role": "Result"}], ["Id", {"Type": "UInt32", "Role": "Input"}]]}""")

SchemaServices.register_json_schema(CaptureServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "CaptureServices.LatestCaptureState", "SubSchemas": [["Return", {"Type":
                                                       "UInt32", "Role": "Result"}], ["Id", {"Type": "UInt32", "Role": "Output"}], ["State", {"Type": "Ref", "Role": "Output",
                                                       "TypeName": "CaptureServices.EState"}]]}""")

SchemaServices.register_json_schema(CaptureServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "CaptureServices.LatestCaptureName", "SubSchemas": [["Return", {"Type":
                                                       "UInt32", "Role": "Result"}], ["Id", {"Type": "UInt32", "Role": "Output"}], ["Name", {"Type": "String", "Role": "Output"}]]}""")

SchemaServices.register_json_schema(CaptureServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "CaptureServices.LatestCaptureTimecode", "SubSchemas": [["Return",
                                                       {"Type": "UInt32", "Role": "Result"}], ["Id", {"Type": "UInt32", "Role": "Output"}], ["Timecode", {"Type": "Ref", "Role":
                                                       "Output", "TypeName": "Timecode135MHz"}]]}""")

SchemaServices.register_json_schema(CaptureServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "CaptureServices.LatestCaptureFilePaths", "SubSchemas": [["Return",
                                                       {"Type": "UInt32", "Role": "Result"}], ["Id", {"Type": "UInt32", "Role": "Output"}], ["FilePaths", {"Type": "List", "Role":
                                                       "Output", "SubSchemas": [["", {"Type": "String"}]]}]]}""")

SchemaServices.register_json_schema(CaptureServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "CaptureServices.LatestCaptureErrors", "SubSchemas": [["Return", {"Type":
                                                       "UInt32", "Role": "Result"}], ["Id", {"Type": "UInt32", "Role": "Output"}], ["Errors", {"Type": "List", "Role": "Output",
                                                       "SubSchemas": [["", {"Type": "String"}]]}]]}""")

SchemaServices.register_json_schema(CaptureServices,"""{"Type": "NamedTuple", "Role": "Callback", "TypeName": "CaptureServices.LatestCaptureChangedCallback", "SubSchemas": [["Id",
                                                       {"Type": "UInt32", "Role": "Input"}], ["State", {"Type": "Ref", "Role": "Input", "TypeName": "CaptureServices.EState"}]]}""")

