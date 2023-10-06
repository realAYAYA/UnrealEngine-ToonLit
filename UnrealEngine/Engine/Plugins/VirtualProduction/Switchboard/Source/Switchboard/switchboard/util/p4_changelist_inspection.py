# Copyright Epic Games, Inc. All Rights Reserved.
import os
import subprocess
import typing
from enum import IntFlag

from ..config import CONFIG
from ..p4_utils import run, valueForMarshalledKey, hasValueForMarshalledKey
from ..switchboard_utils import get_hidden_sp_startupinfo


def has_source_code_changes(earlier_cl: int, later_cl: int, engine_path: str) -> bool:
    """
    Checks whether the CLs between current_cl (exclusive) and future_cl (inclusive) are content only.
    
    Raises P4Error if the data cannot be obtained.
    """
    if earlier_cl == later_cl:
        return False
    
    return any_of_changelists(
        earlier_cl,
        later_cl,
        engine_path,
        lambda cl_description:
            EChangelistFileFlags.Code in determine_changelist_type(valueForMarshalledKey(cl_description, "change")),
        [earlier_cl]
    )

def any_of_changelists(
    earlier_cl: int,
    later_cl: int,
    engine_path: str,
    predicate_callback: typing.Callable[[typing.Dict], bool],
    skipped_cls: typing.List[int] = None
):
    """
    Checks whether a predicate holds for all CLs from current_cl to future_cl (both inclusive),
    The callback can accept the result of an entry returned by "p4 -z tag -G changes".
    """
    # Maybe we need to use -m 10 here to only get 10 CLs at a time... if there are a lot of changes p4 may hang up
    changes: dict = run(f"changes {engine_path}/...@{earlier_cl},{later_cl}")
    _raise_exception_on_error(changes)
    
    for change in changes:
        cl_number = int(valueForMarshalledKey(change, "change"))
        should_skip = skipped_cls is not None and cl_number in skipped_cls
        if not should_skip and predicate_callback(change):
            return True
        
    return False


class EChangelistFileFlags(IntFlag):
    # CL contains source changes
    Code = 1,
    # CL contains content changes
    Content = 2
    
    
def determine_changelist_type(changelist: int) -> EChangelistFileFlags:
    compiled_ue_source_extensions = ["c", "cs", "cpp", "h", "hpp"]
    
    changes: dict = run(f"describe {changelist}")
    _raise_exception_on_error(changes)
    
    has_source_changes = False
    has_content_changes = False
    for change in changes:
        file_index: int = 0
        # Each changed file path name will have an entry depotFilex
        while hasValueForMarshalledKey(change, f"depotFile{file_index}"):
            file_path_name: str = valueForMarshalledKey(change, f"depotFile{file_index}")
            
            has_source_changes = any([file_path_name.endswith(extension) for extension in compiled_ue_source_extensions])
            # If it's not a source file, treat it as a content file
            has_content_changes = not has_source_changes
            
            # All flags fulfilled - no need to inspect more files
            if has_source_changes and has_content_changes:
                break
            
            file_index += 1
    
    result = 0
    if has_source_changes:
        result |= EChangelistFileFlags.Code
    if has_content_changes:
        result |= EChangelistFileFlags.Content
    return result


class P4Error(RuntimeError):
    def __init__(self, message: str):
        self.message = message


def _raise_exception_on_error(p4_command_result: list):
    """
    Accepts the result of p4_util.run and raises an error if any error is set
    """
    if len(p4_command_result) > 0 and valueForMarshalledKey(p4_command_result[0], "code") == "error":
        error_message = valueForMarshalledKey(p4_command_result[0], "data")
        raise P4Error(error_message)