# Copyright Epic Games, Inc. All Rights Reserved.

#-------------------------------------------------------------------------------
class Tips(object):
    def get_tips(self):
        return (
            "Auto-attach a debugger with '.run ... --attach'",
            "Stage a cook with the '.stage' command",
            "Change project within a ushell session using '.project <uprojpath>'",
            "To modify build configuration use '.build xml [set|clear|edit]'",
            "Active build configuration will be shown when running '.build'",
            "List current build configuration with '.build xml'",
            "Generate a ClangDb (compile_commands.json) using '.build misc clangdb'",
            "'.project' (no arguments) will fuzzy search for .uprojects from CWD",
        )
