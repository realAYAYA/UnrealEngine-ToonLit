# Copyright Epic Games, Inc. All Rights Reserved.

import os
import unrealcmd

#-------------------------------------------------------------------------------
class Uat(unrealcmd.MultiPlatformCmd):
    """ Runs an UnrealAutomationTool command """
    command     = unrealcmd.Arg(str, "The UnrealAutomationTool command to run")
    unprojected = unrealcmd.Opt(False, "No implicit '-project=' please")
    uatargs     = unrealcmd.Arg([str], "Arguments to pass to the UAT command")

    def complete_command(self, prefix):
        import os
        import re

        def _read_foreach_subdir(dir, func):
            for entry in (x for x in os.scandir(dir) if x.is_dir()):
                yield from func(entry.path)

        def _read_buildcommand_types(dir, depth=0):
            for entry in os.scandir(dir):
                if entry.name.endswith(".cs"):
                    with open(entry.path, "rt") as lines:
                        for line in lines:
                            m = re.search(r"(\w+) : BuildCommand", line)
                            if m:
                                yield m.group(1)
                                break
                elif entry.is_dir() and entry.name != "obj" and depth < 2:
                    yield from _read_buildcommand_types(entry.path, depth + 1)
                    continue

        def _find_automation_rules(dir):
            for entry in os.scandir(dir):
                if entry.name.endswith(".Automation.csproj"):
                    yield from _read_buildcommand_types(dir)
                    return

        ue_context = self.get_unreal_context()

        uat_script_roots = (
            ue_context.get_engine().get_dir() / "Source/Programs/AutomationTool",
        )

        if project := ue_context.get_project():
            uat_script_roots = (
                *uat_script_roots,
                project.get_dir() / "Build",
            )

        for script_root in uat_script_roots:
            yield from _read_foreach_subdir(script_root, _find_automation_rules)

    def main(self):
        self.use_all_platforms()

        ue_context = self.get_unreal_context()

        cmd = ue_context.get_engine().get_dir() / "Build/BatchFiles"
        cmd /= "RunUAT.bat" if os.name == "nt" else "RunUAT.sh"
        args = (self.args.command,)

        if args[0] and not self.args.unprojected:
            if project := ue_context.get_project():
                args = (*args, "-project=" + str(project.get_path()))

        exec_context = self.get_exec_context()
        cmd = exec_context.create_runnable(cmd, *args, *self.args.uatargs)
        return cmd.run()
