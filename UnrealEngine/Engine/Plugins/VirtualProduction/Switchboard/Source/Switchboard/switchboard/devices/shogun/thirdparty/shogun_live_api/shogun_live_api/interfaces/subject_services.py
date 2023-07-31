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


class SubjectServices(ViconInterface):
    """Functions for controlling and monitoring subjects."""

    class ESubjectType(Enum):
        """Type of subject.

        Enum Values:
            EGeneral: General multi-segment subject.
            ERigidObject: Single or multi-segment rigid body.
            ELabelingCluster: Single-segment rigid body used to identify and label a general subject.
        """
        EGeneral = 1
        ERigidObject = 2
        ELabelingCluster = 3


    class ESubjectRole(Enum):
        """Role this subject supports.

        Enum Values:
            ELabeling: Subject for intermediate marker tracking and labeling.
            ESolving: Subject for kinematic solving.
        """
        ELabeling = 1
        ESolving = 2


    def __init__(self, client):
        """Initialises SubjectServices with a Client and checks if interface is supported."""
        super(SubjectServices, self).__init__(client)

    def load_tracking_configuration(self, file_path):
        """Load a tracking configuration from file.

        This will replace all subjects and smart objects

        Args:
            file_path < string >: Absolute path to a tracking configuration file or other MCP file containing subjects. This path must be accessible from the
                remote host.

        Return:
            return < Result >: Ok - On success.
                FileIOFailure - There was an unexpected error opening or reading the file.
                NotAvailable - The file did not contain a static item index.
                SerialisationFailure - The file is badly formed or contains invalid data.
        """
        return self.client.send_command("SubjectServices.LoadTrackingConfiguration", file_path)

    def save_tracking_configuration(self, file_path):
        """Save a tracking configuration to an MCP file.

        This will include all subjects and smart objects

        Args:
            file_path < string >: Absolute path to desired location of tracking configuration file. This path must be accessible from the remote host.

        Return:
            return < Result >: Ok - On success.
                DiskFull - There was insufficient space to save the file.
                FileIOFailure - There was an unexpected error opening or writing the file.
        """
        return self.client.send_command("SubjectServices.SaveTrackingConfiguration", file_path)

    def import_subject(self, directory, name, type):
        """This function is deprecated and will be removed in a future version.

        Please consider using SubjectServices.ImportSubjects instead.
        Import subject from file. If a subject with the same name is already loaded then it will be overwritten. The file path must
        be accessible from the remote host

        Args:
            directory < string >: Absolute path to a directory containing a valid VSK or VSS file matching the subject name.
            name < string >: Name of subject. 'Name.vsk' and 'Name.vss' will be loaded from the supplied directory (if present).
            type < SubjectServices.ESubjectType >: Type of subject.

        Return:
            return < Result >: Ok - On success.
                FileNotFound - If file does not exist.
                NotSupported - Opening the file is not supported by the remote host.
                InvalidType - The FilePath given is a directory not a file.
                NotPermitted - The remote host user does not have permissions to open the file.
                SerialisationFailure - The file is badly formed or contains invalid data.
                AlreadyPresent - A subject with the same name already exists and Overwrite was false.
                FileIOFailure - There was an unexpected error opening the file.
        """
        return self.client.send_command("SubjectServices.ImportSubject", directory, name, type)

    def import_subjects(self, file_paths, overwrite):
        """Import subjects from files on the remote host.

        Supported file types are vsk, vss and vst.The name of the imported subject will be the name of the file (not including the
        file extension) and the type of the subject is determined automatically.
        If a subject fails to load then import will stop, and the remaining subjects will not be attempted

        Args:
            file_paths < [string] >: List of absolute paths to subject files.
            overwrite < bool >: If a subject with the same name already exists, should it be overwritten.

        Return:
            return < Result >: Ok - On success.
                FileNotFound - If file does not exist.
                NotSupported - Opening the file is not supported by the remote host.
                InvalidType - The file path given is a directory not a file.
                NotPermitted - The remote host user does not have permissions to open the file.
                SerialisationFailure - The file is badly formed or contains invalid data.
                AlreadyPresent - A subject with the same name already exists and Overwrite was false.
                FileIOFailure - There was an unexpected error opening the file.
        """
        return self.client.send_command("SubjectServices.ImportSubjects", file_paths, overwrite)

    def export_subject(self, subject_name, directory, overwrite):
        """Export subject to VSK/VSS files.

        The subject name will be used as the filename, and one file will be created for each subject role with a corresponding extension

        Args:
            subject_name < string >: Name of the subject.
            directory < string >: Absolute path to a directory to save the subject file(s). This path must be accessible from the remote host.
            overwrite < bool >: If any existing files should be overwritten.

        Return:
            return < Result >: Ok - On success.
                AlreadyPresent - A file with the same name already exists and overwriting was not enabled.
                Failed - A file could not be written.
                NotFound - If named subject is not loaded.
                NotSupported - The subject type cannot be exported to VSK/VSS.
        """
        return self.client.send_command("SubjectServices.ExportSubject", subject_name, directory, overwrite)

    def remove_subject(self, name):
        """Remove a named subject.

        Args:
            name < string >: Name of the subject.

        Return:
            return < Result >: Ok - On success.
                NotFound - If subject was not already loaded.
        """
        return self.client.send_command("SubjectServices.RemoveSubject", name)

    def remove_all_subjects(self):
        """Remove all subjects.

        Return:
            return < Result >: Ok - On success.
                NotFound - If no subjects were loaded.
        """
        return self.client.send_command("SubjectServices.RemoveAllSubjects")

    def subjects(self):
        """Get names of all loaded subjects.

        Return:
            return < Result >: Ok - On success.
            subject_names < [string] >: Names of loaded subjects.
        """
        return self.client.send_command("SubjectServices.Subjects")

    def subject_type(self, name):
        """Determine type of a subject.

        Args:
            name < string >: Name of subject.

        Return:
            return < Result >: Ok - On success.
                NotFound - If named subject is not loaded.
            type < SubjectServices.ESubjectType >: Type of subject.
        """
        return self.client.send_command("SubjectServices.SubjectType", name)

    def subject_roles(self, name):
        """Determine roles supported by a subject.

        Args:
            name < string >: Name of subject.

        Return:
            return < Result >: Ok - On success.
                NotFound - If named subject is not loaded.
            roles < [SubjectServices.ESubjectRole] >: Roles supported by subject.
        """
        return self.client.send_command("SubjectServices.SubjectRoles", name)

    def add_subjects_changed_callback(self, function):
        """Callback issued whenever the list of loaded subjects has changed."""
        return self.client.add_callback("SubjectServices.SubjectsChangedCallback", function)

    def set_subject_enabled(self, subject_name, enable):
        """Enable or disable a subject.

        Args:
            subject_name < string >: Name of the subject.
            enable < bool >: Enable if true, otherwise disable.

        Return:
            return < Result >: Ok - On success.
                NotFound - If named subject is not loaded.
        """
        return self.client.send_command("SubjectServices.SetSubjectEnabled", subject_name, enable)

    def set_all_subjects_enabled(self, enable):
        """Enable or disable all subjects.

        Args:
            enable < bool >: Enable if true, otherwise disable.

        Return:
            return < Result >: Ok - On success.
                NotFound - If no subjects are loaded.
        """
        return self.client.send_command("SubjectServices.SetAllSubjectsEnabled", enable)

    def enabled_subjects(self):
        """Get names of all enabled subjects.

        Return:
            return < Result >: Ok - On success.
            subject_names < [string] >: Names of enabled subjects.
        """
        return self.client.send_command("SubjectServices.EnabledSubjects")

    def add_enabled_subjects_changed_callback(self, function):
        """This function is deprecated and will be removed in a future version.

        Please consider using SubjectServices.SubjectsChangedCallback instead.
        Callback issued whenever the list of subjects enabled has changed
        """
        return self.client.add_callback("SubjectServices.EnabledSubjectsChangedCallback", function)

    def remove_callback(self, callback_id):
        """remove callback of any type using the id supplied when it was added."""
        return self.client.remove_callback(callback_id)



SchemaServices.register_json_schema(SubjectServices,"""{"Type": "NamedTuple", "TypeName": "SubjectServices"}""")

SchemaServices.register_json_schema(SubjectServices.ESubjectType,"""{"Type": "Enum32", "TypeName": "SubjectServices.ESubjectType", "EnumValues": [["General", 1], ["RigidObject", 2], ["LabelingCluster",
                                                                    3]]}""")

SchemaServices.register_json_schema(SubjectServices.ESubjectRole,"""{"Type": "Enum32", "TypeName": "SubjectServices.ESubjectRole", "EnumValues": [["Labeling", 1], ["Solving", 2]]}""")

SchemaServices.register_json_schema(SubjectServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "SubjectServices.LoadTrackingConfiguration", "SubSchemas": [["Return",
                                                       {"Type": "UInt32", "Role": "Result"}], ["FilePath", {"Type": "String", "Role": "Input"}]]}""")

SchemaServices.register_json_schema(SubjectServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "SubjectServices.SaveTrackingConfiguration", "SubSchemas": [["Return",
                                                       {"Type": "UInt32", "Role": "Result"}], ["FilePath", {"Type": "String", "Role": "Input"}]]}""")

SchemaServices.register_json_schema(SubjectServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "SubjectServices.ImportSubject", "SubSchemas": [["Return", {"Type":
                                                       "UInt32", "Role": "Result"}], ["Directory", {"Type": "String", "Role": "Input"}], ["Name", {"Type": "String", "Role": "Input"}],
                                                       ["Type", {"Type": "Ref", "Role": "Input", "TypeName": "SubjectServices.ESubjectType"}]]}""")

SchemaServices.register_json_schema(SubjectServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "SubjectServices.ImportSubjects", "SubSchemas": [["Return", {"Type":
                                                       "UInt32", "Role": "Result"}], ["FilePaths", {"Type": "List", "Role": "Input", "SubSchemas": [["", {"Type": "String"}]]}],
                                                       ["Overwrite", {"Type": "Bool", "Role": "Input"}]]}""")

SchemaServices.register_json_schema(SubjectServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "SubjectServices.ExportSubject", "SubSchemas": [["Return", {"Type":
                                                       "UInt32", "Role": "Result"}], ["SubjectName", {"Type": "String", "Role": "Input"}], ["Directory", {"Type": "String", "Role":
                                                       "Input"}], ["Overwrite", {"Type": "Bool", "Role": "Input"}]]}""")

SchemaServices.register_json_schema(SubjectServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "SubjectServices.RemoveSubject", "SubSchemas": [["Return", {"Type":
                                                       "UInt32", "Role": "Result"}], ["Name", {"Type": "String", "Role": "Input"}]]}""")

SchemaServices.register_json_schema(SubjectServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "SubjectServices.RemoveAllSubjects", "SubSchemas": [["Return", {"Type":
                                                       "UInt32", "Role": "Result"}]]}""")

SchemaServices.register_json_schema(SubjectServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "SubjectServices.Subjects", "SubSchemas": [["Return", {"Type": "UInt32",
                                                       "Role": "Result"}], ["SubjectNames", {"Type": "List", "Role": "Output", "SubSchemas": [["", {"Type": "String"}]]}]]}""")

SchemaServices.register_json_schema(SubjectServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "SubjectServices.SubjectType", "SubSchemas": [["Return", {"Type":
                                                       "UInt32", "Role": "Result"}], ["Name", {"Type": "String", "Role": "Input"}], ["Type", {"Type": "Ref", "Role": "Output",
                                                       "TypeName": "SubjectServices.ESubjectType"}]]}""")

SchemaServices.register_json_schema(SubjectServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "SubjectServices.SubjectRoles", "SubSchemas": [["Return", {"Type":
                                                       "UInt32", "Role": "Result"}], ["Name", {"Type": "String", "Role": "Input"}], ["Roles", {"Type": "List", "Role": "Output",
                                                       "SubSchemas": [["", {"Type": "Ref", "TypeName": "SubjectServices.ESubjectRole"}]]}]]}""")

SchemaServices.register_json_schema(SubjectServices,"""{"Type": "NamedTuple", "Role": "Callback", "TypeName": "SubjectServices.SubjectsChangedCallback"}""")

SchemaServices.register_json_schema(SubjectServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "SubjectServices.SetSubjectEnabled", "SubSchemas": [["Return", {"Type":
                                                       "UInt32", "Role": "Result"}], ["SubjectName", {"Type": "String", "Role": "Input"}], ["Enable", {"Type": "Bool", "Role":
                                                       "Input"}]]}""")

SchemaServices.register_json_schema(SubjectServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "SubjectServices.SetAllSubjectsEnabled", "SubSchemas": [["Return",
                                                       {"Type": "UInt32", "Role": "Result"}], ["Enable", {"Type": "Bool", "Role": "Input"}]]}""")

SchemaServices.register_json_schema(SubjectServices,"""{"Type": "NamedTuple", "Role": "Function", "TypeName": "SubjectServices.EnabledSubjects", "SubSchemas": [["Return", {"Type":
                                                       "UInt32", "Role": "Result"}], ["SubjectNames", {"Type": "List", "Role": "Output", "SubSchemas": [["", {"Type": "String"}]]}]]}""")

SchemaServices.register_json_schema(SubjectServices,"""{"Type": "NamedTuple", "Role": "Callback", "TypeName": "SubjectServices.EnabledSubjectsChangedCallback"}""")

