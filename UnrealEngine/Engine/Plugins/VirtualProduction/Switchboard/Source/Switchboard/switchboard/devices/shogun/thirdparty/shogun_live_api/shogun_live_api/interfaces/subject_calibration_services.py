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


class SubjectCalibrationServices(ViconInterface):
    """Functions for controlling and monitoring subject calibration."""

    class ESubjectCalibrationState(Enum):
        """ESubjectCalibrationState.

        Enum Values:
            ENone: Subject calibration has not yet been run.  The system is ready to subject calibrate in None, Completed or Cancelled.
            ETPoseLabelling: The system is attempting to to identify a subject T-Pose.
            ECalibrating: The system is calibrating the subject.
            ECompleted: Subject calibration has completed.  This is an end state of the process.
            ECancelled: The last subject calibration was cancelled.  This is an end state of the process.
        """
        ENone = 0
        ETPoseLabelling = 1
        ECalibrating = 2
        ECompleted = 3
        ECancelled = 4


    def __init__(self, client):
        """Initialises SubjectCalibrationServices with a Client and checks if interface is supported."""
        super(SubjectCalibrationServices, self).__init__(client)

    def can_start_subject_calibration(self):
        """Is the system able to initiate subject calibration?.

        Return:
            return < bool >: True - If you can start subject calibration.
            reason < string >: The reason why subject calibration cannot be initiated.
        """
        return self.client.send_command("SubjectCalibrationServices.CanStartSubjectCalibration")

    def can_start_subject_recalibration(self):
        """Is the system able to initiate subject re-calibration?.

        Return:
            return < bool >: True - If you can start subject re-calibration.
            reason < string >: The reason why subject re-calibration cannot be initiated.
        """
        return self.client.send_command("SubjectCalibrationServices.CanStartSubjectRecalibration")

    def can_accept_t_pose(self):
        """Can the system accept the current state as a T-Pose?.

        Return:
            return < bool >: True - If you can accept the current state as a T-Pose.
            reason < string >: The reason why the T-Pose cannot be selected.
        """
        return self.client.send_command("SubjectCalibrationServices.CanAcceptTPose")

    def can_stop_subject_calibration(self):
        """Can the system stop or cancel subject calibration?.

        Return:
            return < bool >: True - If you can stop subject calibration.
            reason < string >: The reason why subject calibration cannot be ended.
        """
        return self.client.send_command("SubjectCalibrationServices.CanStopSubjectCalibration")

    def add_can_start_subject_calibration_changed_callback(self, function):
        """Callback issued whenever the systems ability to start subject calibration changed.

        Args:
            able < bool >: Can the system start subject calibration?
        """
        return self.client.add_callback("SubjectCalibrationServices.CanStartSubjectCalibrationChangedCallback", function)

    def add_can_start_subject_recalibration_changed_callback(self, function):
        """Callback issued whenever the systems ability to start subject re-calibration changed.

        Args:
            able < bool >: Can the system start subject re-calibration?
        """
        return self.client.add_callback("SubjectCalibrationServices.CanStartSubjectRecalibrationChangedCallback", function)

    def add_can_accept_t_pose_changed_callback(self, function):
        """Callback issued whenever the systems ability to accept a T-Pose changed.

        Args:
            able < bool >: Can the system accept a T-Pose?
        """
        return self.client.add_callback("SubjectCalibrationServices.CanAcceptTPoseChangedCallback", function)

    def add_can_end_subject_calibration_changed_callback(self, function):
        """Callback issued whenever the systems ability to end subject calibration or re-calibration changed.

        Args:
            able < bool >: Can the system end subject calibration?
        """
        return self.client.add_callback("SubjectCalibrationServices.CanEndSubjectCalibrationChangedCallback", function)

    def start_subject_calibration(self, capture_name):
        """Initiates subject calibration.

        Args:
            capture_name < string >: A capture name to use for the subject calibration files.  Leave blank to use the default.  If the name already exists in
                the capture directory it will be overwritten.

        Return:
            return < Result >: Ok - On success.
                InvalidArgument - If the capture name has invalid characters.
                NotAvailable - If calibration is currently running.
            subject_calibration_session < int >: Provides the session id for the calibration.  This is provided to to Cancel and Stop to ensure the correct subject calibration
                is being terminated.
        """
        return self.client.send_command("SubjectCalibrationServices.StartSubjectCalibration", capture_name)

    def start_subject_recalibration(self, capture_name):
        """Initiates subject re-calibration.

        The subject to update should be named using 'set subject name'

        Args:
            capture_name < string >: A capture name to use for the subject calibration files.  Leave blank to use the default.  If the name already exists in
                the capture directory it will be overwritten.

        Return:
            return < Result >: Ok - On success.
                InvalidArgument - If the capture name has invalid characters.
                NotAvailable - If calibration is currently running.
            subject_calibration_session < int >: Provides the session id for the calibration.  This is provided to to Cancel and Stop to ensure the correct subject calibration
                is being terminated.
        """
        return self.client.send_command("SubjectCalibrationServices.StartSubjectRecalibration", capture_name)

    def accept_t_pose(self, subject_calibration_session):
        """During calibration, mark the subject as being in the T-pose.

        Args:
            subject_calibration_session < int >: The session id for the calibration.  This will have been provided by StartSubjectCalibration or StartSubjectRecalibration.

        Return:
            return < Result >: Ok - On success.
        """
        return self.client.send_command("SubjectCalibrationServices.AcceptTPose", subject_calibration_session)

    def cancel_subject_calibration(self, subject_calibration_session):
        """Cancel the currently running subject calibration or re-calibration.

        Args:
            subject_calibration_session < int >: The session id for the calibration.  This will have been provided by StartSubjectCalibration or StartSubjectRecalibration.

        Return:
            return < Result >: Ok - On success.
                NotAvailable - If calibration is not currently running.
        """
        return self.client.send_command("SubjectCalibrationServices.CancelSubjectCalibration", subject_calibration_session)

    def stop_subject_calibration(self, subject_calibration_session):
        """Stop the currently running subject calibration or re-calibration.

        Args:
            subject_calibration_session < int >: The session id for the calibration.  This will have been provided by StartSubjectCalibration or StartSubjectRecalibration.

        Return:
            return < Result >: Ok - On success.
                NotAvailable - If calibration is not currently running.
        """
        return self.client.send_command("SubjectCalibrationServices.StopSubjectCalibration", subject_calibration_session)

    def latest_subject_calibration_state(self):
        """Get the current state of calibration or re-calibration.

        Return:
            return < Result >: Ok - On success.
            session_id < int >: The currently executing session id.
            state < SubjectCalibrationServices.ESubjectCalibrationState >: The current state of calibration.
        """
        return self.client.send_command("SubjectCalibrationServices.LatestSubjectCalibrationState")

    def add_latest_subject_calibration_state_changed_callback(self, function):
        """Callback issued whenever the calibration state changes.

        Args:
            session_id < int >: The currently executing session id.
            state < SubjectCalibrationServices.ESubjectCalibrationState >: The current state of calibration.
        """
        return self.client.add_callback("SubjectCalibrationServices.LatestSubjectCalibrationStateChangedCallback", function)

    def set_new_subject_name(self, subject_name):
        """Set the name for the subject to be created by the next subject calibration, or updated by recalibration.

        Args:
            subject_name < string >: Subject name to set.

        Return:
            return < Result >: Ok - On success.
        """
        return self.client.send_command("SubjectCalibrationServices.SetNewSubjectName", subject_name)

    def new_subject_name(self):
        """The name for the subject to be created by the next subject calibration, or updated by recalibration.

        Return:
            return < Result >: Ok - On success.
                InvalidArgument - If the name contains .
            subject_name < string >: The subject name.
        """
        return self.client.send_command("SubjectCalibrationServices.NewSubjectName")

    def subject_labelling_templates(self):
        """The set of templates available for generating the labeling skeleton.

        Return:
            return < Result >: Ok - On success.
            labelling_templates < [string] >: The set of labeling templates.
        """
        return self.client.send_command("SubjectCalibrationServices.SubjectLabellingTemplates")

    def set_subject_labelling_template(self, labelling_template):
        """Set the template for generating the labeling skeleton.

        Should be from the set provided by SubjectLabellingTemplates

        Args:
            labelling_template < string >: The labeling template to use for the next subject calibration.

        Return:
            return < Result >: Ok - On success.
                NotFound - The provided template name is not available.
        """
        return self.client.send_command("SubjectCalibrationServices.SetSubjectLabellingTemplate", labelling_template)

    def subject_labelling_template(self):
        """The template for generating the labeling skeleton.

        Return:
            return < Result >: Ok - On success.
            labelling_template < string >: The labeling template to use for the next subject calibration.
            labelling_template_path < string >: The full path to the template.
        """
        return self.client.send_command("SubjectCalibrationServices.SubjectLabellingTemplate")

    def subject_solving_templates(self):
        """The set of templates available for generating the solving skeleton.

        Return:
            return < Result >: Ok - On success.
            solving_templates < [string] >: The set of solving templates.
        """
        return self.client.send_command("SubjectCalibrationServices.SubjectSolvingTemplates")

    def set_subject_solving_template(self, solving_template):
        """Set the template for generating the solving skeleton.

        Should be from the set provided by SubjectSolvingTemplates

        Args:
            solving_template < string >: The solving template to use for the next subject calibration.

        Return:
            return < Result >: Ok - On success.
                NotFound - The provided template name is not available.
        """
        return self.client.send_command("SubjectCalibrationServices.SetSubjectSolvingTemplate", solving_template)

    def subject_solving_template(self):
        """The template for generating the solving skeleton.

        Return:
            return < Result >: Ok - On success.
            solving_template < string >: The solving template to use for the next subject calibration.
            solving_template_path < string >: The full path to the template.
        """
        return self.client.send_command("SubjectCalibrationServices.SubjectSolvingTemplate")

    def subject_skins(self):
        """The set of skins available for generating the solving skeleton.

        Return:
            return < Result >: Ok - On success.
            skins < [string] >: The set of available skins.
        """
        return self.client.send_command("SubjectCalibrationServices.SubjectSkins")

    def set_subject_skin(self, skin):
        """Set the skin for generating the solving skeleton.

        Should be from the set provided by SubjectSkins

        Args:
            skin < string >: The skin use for the next subject calibration.

        Return:
            return < Result >: Ok - On success.
                NotFound - The provided skin name is not available.
        """
        return self.client.send_command("SubjectCalibrationServices.SetSubjectSkin", skin)

    def subject_skin(self):
        """The skin to use for subject calibration.

        Return:
            return < Result >: Ok - On success.
            skin < string >: The skin name to use for the next subject calibration.
            skin_path < string >: The full path to the skin.
        """
        return self.client.send_command("SubjectCalibrationServices.SubjectSkin")

    def add_subject_calibration_settings_changed_callback(self, function):
        """Callback issued whenever any of the calibration settings changes."""
        return self.client.add_callback("SubjectCalibrationServices.SubjectCalibrationSettingsChangedCallback", function)

    def remove_callback(self, callback_id):
        """remove callback of any type using the id supplied when it was added."""
        return self.client.remove_callback(callback_id)



SchemaServices.register_json_schema(SubjectCalibrationServices,"""{"Type": "NamedTuple", "TypeName": "SubjectCalibrationServices"}""")

SchemaServices.register_json_schema(SubjectCalibrationServices.ESubjectCalibrationState,"""{"Type": "Enum32", "TypeName": "SubjectCalibrationServices.ESubjectCalibrationState", "EnumValues": [["None", 0], ["TPoseLabelling",
                                                                                           1], ["Calibrating", 2], ["Completed", 3], ["Cancelled", 4]]}""")

SchemaServices.register_json_schema(SubjectCalibrationServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "SubjectCalibrationServices.CanStartSubjectCalibration", "SubSchemas":
                                                                  [["Return", {"Type": "Bool", "Role": "Return"}], ["Reason", {"Type": "String", "Role": "Output"}]]}""")

SchemaServices.register_json_schema(SubjectCalibrationServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "SubjectCalibrationServices.CanStartSubjectRecalibration", "SubSchemas":
                                                                  [["Return", {"Type": "Bool", "Role": "Return"}], ["Reason", {"Type": "String", "Role": "Output"}]]}""")

SchemaServices.register_json_schema(SubjectCalibrationServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "SubjectCalibrationServices.CanAcceptTPose", "SubSchemas": [["Return",
                                                                  {"Type": "Bool", "Role": "Return"}], ["Reason", {"Type": "String", "Role": "Output"}]]}""")

SchemaServices.register_json_schema(SubjectCalibrationServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "SubjectCalibrationServices.CanStopSubjectCalibration", "SubSchemas":
                                                                  [["Return", {"Type": "Bool", "Role": "Return"}], ["Reason", {"Type": "String", "Role": "Output"}]]}""")

SchemaServices.register_json_schema(SubjectCalibrationServices,"""{"Type": "NamedTuple", "Role": "Callback", "TypeName": "SubjectCalibrationServices.CanStartSubjectCalibrationChangedCallback",
                                                                  "SubSchemas": [["Able", {"Type": "Bool", "Role": "Input"}]]}""")

SchemaServices.register_json_schema(SubjectCalibrationServices,"""{"Type": "NamedTuple", "Role": "Callback", "TypeName": "SubjectCalibrationServices.CanStartSubjectRecalibrationChangedCallback",
                                                                  "SubSchemas": [["Able", {"Type": "Bool", "Role": "Input"}]]}""")

SchemaServices.register_json_schema(SubjectCalibrationServices,"""{"Type": "NamedTuple", "Role": "Callback", "TypeName": "SubjectCalibrationServices.CanAcceptTPoseChangedCallback", "SubSchemas":
                                                                  [["Able", {"Type": "Bool", "Role": "Input"}]]}""")

SchemaServices.register_json_schema(SubjectCalibrationServices,"""{"Type": "NamedTuple", "Role": "Callback", "TypeName": "SubjectCalibrationServices.CanEndSubjectCalibrationChangedCallback",
                                                                  "SubSchemas": [["Able", {"Type": "Bool", "Role": "Input"}]]}""")

SchemaServices.register_json_schema(SubjectCalibrationServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "SubjectCalibrationServices.StartSubjectCalibration", "SubSchemas":
                                                                  [["Return", {"Type": "UInt32", "Role": "Result"}], ["SubjectCalibrationSession", {"Type": "UInt32", "Role": "Output"}],
                                                                  ["CaptureName", {"Type": "String", "Role": "Input"}]]}""")

SchemaServices.register_json_schema(SubjectCalibrationServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "SubjectCalibrationServices.StartSubjectRecalibration", "SubSchemas":
                                                                  [["Return", {"Type": "UInt32", "Role": "Result"}], ["SubjectCalibrationSession", {"Type": "UInt32", "Role": "Output"}],
                                                                  ["CaptureName", {"Type": "String", "Role": "Input"}]]}""")

SchemaServices.register_json_schema(SubjectCalibrationServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "SubjectCalibrationServices.AcceptTPose", "SubSchemas": [["Return",
                                                                  {"Type": "UInt32", "Role": "Result"}], ["SubjectCalibrationSession", {"Type": "UInt32", "Role": "Input"}]]}""")

SchemaServices.register_json_schema(SubjectCalibrationServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "SubjectCalibrationServices.CancelSubjectCalibration", "SubSchemas":
                                                                  [["Return", {"Type": "UInt32", "Role": "Result"}], ["SubjectCalibrationSession", {"Type": "UInt32", "Role": "Input"}]]}""")

SchemaServices.register_json_schema(SubjectCalibrationServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "SubjectCalibrationServices.StopSubjectCalibration", "SubSchemas":
                                                                  [["Return", {"Type": "UInt32", "Role": "Result"}], ["SubjectCalibrationSession", {"Type": "UInt32", "Role": "Input"}]]}""")

SchemaServices.register_json_schema(SubjectCalibrationServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "SubjectCalibrationServices.LatestSubjectCalibrationState", "SubSchemas":
                                                                  [["Return", {"Type": "UInt32", "Role": "Result"}], ["SessionId", {"Type": "UInt32", "Role": "Output"}], ["State", {"Type":
                                                                  "Ref", "Role": "Output", "TypeName": "SubjectCalibrationServices.ESubjectCalibrationState"}]]}""")

SchemaServices.register_json_schema(SubjectCalibrationServices,"""{"Type": "NamedTuple", "Role": "Callback", "TypeName": "SubjectCalibrationServices.LatestSubjectCalibrationStateChangedCallback",
                                                                  "SubSchemas": [["SessionId", {"Type": "UInt32", "Role": "Input"}], ["State", {"Type": "Ref", "Role": "Input", "TypeName":
                                                                  "SubjectCalibrationServices.ESubjectCalibrationState"}]]}""")

SchemaServices.register_json_schema(SubjectCalibrationServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "SubjectCalibrationServices.SetNewSubjectName", "SubSchemas": [["Return",
                                                                  {"Type": "UInt32", "Role": "Result"}], ["SubjectName", {"Type": "String", "Role": "Input"}]]}""")

SchemaServices.register_json_schema(SubjectCalibrationServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "SubjectCalibrationServices.NewSubjectName", "SubSchemas": [["Return",
                                                                  {"Type": "UInt32", "Role": "Result"}], ["SubjectName", {"Type": "String", "Role": "Output"}]]}""")

SchemaServices.register_json_schema(SubjectCalibrationServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "SubjectCalibrationServices.SubjectLabellingTemplates", "SubSchemas":
                                                                  [["Return", {"Type": "UInt32", "Role": "Result"}], ["LabellingTemplates", {"Type": "List", "Role": "Output", "SubSchemas":
                                                                  [["", {"Type": "String"}]]}]]}""")

SchemaServices.register_json_schema(SubjectCalibrationServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "SubjectCalibrationServices.SetSubjectLabellingTemplate", "SubSchemas":
                                                                  [["Return", {"Type": "UInt32", "Role": "Result"}], ["LabellingTemplate", {"Type": "String", "Role": "Input"}]]}""")

SchemaServices.register_json_schema(SubjectCalibrationServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "SubjectCalibrationServices.SubjectLabellingTemplate", "SubSchemas":
                                                                  [["Return", {"Type": "UInt32", "Role": "Result"}], ["LabellingTemplate", {"Type": "String", "Role": "Output"}], ["LabellingTemplatePath",
                                                                  {"Type": "String", "Role": "Output"}]]}""")

SchemaServices.register_json_schema(SubjectCalibrationServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "SubjectCalibrationServices.SubjectSolvingTemplates", "SubSchemas":
                                                                  [["Return", {"Type": "UInt32", "Role": "Result"}], ["SolvingTemplates", {"Type": "List", "Role": "Output", "SubSchemas":
                                                                  [["", {"Type": "String"}]]}]]}""")

SchemaServices.register_json_schema(SubjectCalibrationServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "SubjectCalibrationServices.SetSubjectSolvingTemplate", "SubSchemas":
                                                                  [["Return", {"Type": "UInt32", "Role": "Result"}], ["SolvingTemplate", {"Type": "String", "Role": "Input"}]]}""")

SchemaServices.register_json_schema(SubjectCalibrationServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "SubjectCalibrationServices.SubjectSolvingTemplate", "SubSchemas":
                                                                  [["Return", {"Type": "UInt32", "Role": "Result"}], ["SolvingTemplate", {"Type": "String", "Role": "Output"}], ["SolvingTemplatePath",
                                                                  {"Type": "String", "Role": "Output"}]]}""")

SchemaServices.register_json_schema(SubjectCalibrationServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "SubjectCalibrationServices.SubjectSkins", "SubSchemas": [["Return",
                                                                  {"Type": "UInt32", "Role": "Result"}], ["Skins", {"Type": "List", "Role": "Output", "SubSchemas": [["", {"Type": "String"}]]}]]}""")

SchemaServices.register_json_schema(SubjectCalibrationServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "SubjectCalibrationServices.SetSubjectSkin", "SubSchemas": [["Return",
                                                                  {"Type": "UInt32", "Role": "Result"}], ["Skin", {"Type": "String", "Role": "Input"}]]}""")

SchemaServices.register_json_schema(SubjectCalibrationServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "SubjectCalibrationServices.SubjectSkin", "SubSchemas": [["Return",
                                                                  {"Type": "UInt32", "Role": "Result"}], ["Skin", {"Type": "String", "Role": "Output"}], ["SkinPath", {"Type": "String", "Role":
                                                                  "Output"}]]}""")

SchemaServices.register_json_schema(SubjectCalibrationServices,"""{"Type": "NamedTuple", "Role": "Callback", "TypeName": "SubjectCalibrationServices.SubjectCalibrationSettingsChangedCallback"}""")

